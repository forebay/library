#include "apps_internal.h"

#include <stdlib.h>
#include <string.h>

#define LARGEST_ARRAY_INDEX 4294967294.0
#define LONGEST_ARRAY_INDEX 10

typedef struct path_kind {
    const char *name;
    const char *environment;
} path_kind;

static const path_kind path_kinds[] = {
    {"repos", "HUB_REPOS_SUBDIR"},
    {"plugin", "HUB_PLUGIN_SUBDIR"},
    {"cache", "HUB_CACHE_SUBDIR"},
    {"config", "HUB_CONFIG_SUBDIR"},
};

static const char *const copied_before_detect[] = {"icon", "home"};
static const char *const copied_after_wire_format[] = {
    "usage", "accent", "wrapperCommand", "npmPlugins", "startupHook", "discovery", "projects", "modelCatalog",
};

/** A JSON value reads a member only when it is an object; JavaScript answers undefined for any other value. */
static const cJSON *member(const cJSON *value, const char *name) {
    return cJSON_IsObject(value) ? cJSON_GetObjectItemCaseSensitive(value, name) : NULL;
}

/** An array index in the sense ECMAScript orders own keys by: canonical, and at most 2^32 - 2. */
static int is_array_index(const char *key, double *index) {
    size_t length = strlen(key);
    if (length == 0 || length > LONGEST_ARRAY_INDEX || (length > 1 && key[0] == '0')) {
        return 0;
    }
    double value = 0;
    for (size_t position = 0; position < length; position++) {
        if (key[position] < '0' || key[position] > '9') {
            return 0;
        }
        value = value * 10 + (key[position] - '0');
    }
    *index = value;
    return value <= LARGEST_ARRAY_INDEX;
}

typedef struct ordered_entry {
    const cJSON *entry;
    double index;
    int indexed;
    size_t position;
} ordered_entry;

static int compare_entries(const void *left, const void *right) {
    const ordered_entry *first = left;
    const ordered_entry *second = right;
    if (first->indexed != second->indexed) {
        return first->indexed ? -1 : 1;
    }
    if (first->indexed && first->index != second->index) {
        return first->index < second->index ? -1 : 1;
    }
    return first->position < second->position ? -1 : first->position > second->position;
}

/** The registry's entries in Object.entries order: array indices ascending, then every other key as written. */
static ordered_entry *entries_in_order(const cJSON *registry, size_t *count) {
    *count = (size_t)cJSON_GetArraySize(registry);
    ordered_entry *entries = malloc((*count == 0 ? 1 : *count) * sizeof(ordered_entry));
    if (entries == NULL) {
        return NULL;
    }
    size_t position = 0;
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, registry) {
        ordered_entry *ordered = &entries[position];
        ordered->entry = entry;
        ordered->position = position;
        ordered->index = 0;
        ordered->indexed = is_array_index(entry->string, &ordered->index);
        position++;
    }
    qsort(entries, *count, sizeof(ordered_entry), compare_entries);
    return entries;
}

static int is_valid(const cJSON *id, const cJSON *entry) {
    const cJSON *label = member(entry, "label");
    const cJSON *candidates = member(member(entry, "home"), "candidates");
    return cJSON_IsString(id) && id->valuestring[0] != '\0' && cJSON_IsString(label) && cJSON_IsArray(candidates) &&
           cJSON_GetArraySize(candidates) > 0;
}

static int is_single_segment(const char *name) {
    return name[0] != '\0' && strcmp(name, ".") != 0 && strcmp(name, "..") != 0 && strpbrk(name, "/\\") == NULL;
}

/** basekit's pathName: the declared name, then the environment's, then the default; each only as one segment. */
static char *path_name(const path_kind *kind, const cJSON *declared, const apps_facts *facts) {
    const char *candidates[2] = {cJSON_IsString(declared) ? declared->valuestring : NULL,
                                 spisor_environment(facts->call, kind->environment)};
    for (size_t index = 0; index < 2; index++) {
        if (candidates[index] == NULL) {
            continue;
        }
        char *trimmed = apps_trim(candidates[index]);
        if (trimmed == NULL || is_single_segment(trimmed)) {
            return trimmed;
        }
        free(trimmed);
    }
    return apps_copy(kind->name);
}

static cJSON *path_names(const cJSON *declared, const apps_facts *facts) {
    cJSON *names = cJSON_CreateObject();
    for (size_t index = 0; names != NULL && index < sizeof(path_kinds) / sizeof(path_kinds[0]); index++) {
        char *name = path_name(&path_kinds[index], member(declared, path_kinds[index].name), facts);
        if (name == NULL || cJSON_AddStringToObject(names, path_kinds[index].name, name) == NULL) {
            cJSON_Delete(names);
            names = NULL;
        }
        free(name);
    }
    return names;
}

static int add_copy(cJSON *descriptor, const char *name, const cJSON *value) {
    if (value == NULL) {
        return 1;
    }
    cJSON *copy = cJSON_Duplicate(value, 1);
    return copy != NULL && cJSON_AddItemToObject(descriptor, name, copy);
}

static int add_declared(cJSON *descriptor, const cJSON *entry, const char *const *names, size_t count) {
    for (size_t index = 0; index < count; index++) {
        if (!add_copy(descriptor, names[index], member(entry, names[index]))) {
            return 0;
        }
    }
    return 1;
}

/** A field basekit writes as `w.field ?? fallback`. */
static int add_defaulted(cJSON *descriptor, const cJSON *value, const char *name, const char *fallback) {
    if (!apps_is_nullish(value)) {
        return add_copy(descriptor, name, value);
    }
    return cJSON_AddStringToObject(descriptor, name, fallback) != NULL;
}

static int add_detect(cJSON *descriptor, const cJSON *entry, const cJSON *id) {
    const cJSON *detect = member(entry, "detect");
    cJSON *resolved = cJSON_AddObjectToObject(descriptor, "detect");
    if (resolved == NULL) {
        return 0;
    }
    const cJSON *binary = member(detect, "binary");
    return add_copy(resolved, "binary", apps_is_nullish(binary) ? id : binary) &&
           add_defaulted(resolved, member(detect, "pkg"), "pkg", "");
}

static int add_proxy_port(cJSON *descriptor, const cJSON *entry) {
    const cJSON *port = member(entry, "proxyPort");
    if (!apps_is_nullish(port)) {
        return add_copy(descriptor, "proxyPort", port);
    }
    return cJSON_AddNumberToObject(descriptor, "proxyPort", 0) != NULL;
}

static int add_paths(cJSON *descriptor, const cJSON *entry, const apps_facts *facts) {
    cJSON *paths = path_names(member(entry, "paths"), facts);
    if (paths == NULL || !cJSON_AddItemToObject(descriptor, "paths", paths)) {
        cJSON_Delete(paths);
        return 0;
    }
    return 1;
}

static int fill_descriptor(cJSON *descriptor, const cJSON *entry, const cJSON *id, const apps_facts *facts) {
    return add_copy(descriptor, "id", id) && add_copy(descriptor, "label", member(entry, "label")) &&
           add_declared(descriptor, entry, copied_before_detect, 2) && add_detect(descriptor, entry, id) &&
           add_copy(descriptor, "loader", member(entry, "loader")) &&
           add_defaulted(descriptor, member(entry, "commandsSubdir"), "commandsSubdir", "commands") &&
           add_paths(descriptor, entry, facts) && add_proxy_port(descriptor, entry) &&
           add_defaulted(descriptor, member(entry, "integration"), "integration", "env-baseurl") &&
           add_defaulted(descriptor, member(entry, "wireFormat"), "wireFormat", "anthropic") &&
           add_declared(descriptor, entry, copied_after_wire_format,
                        sizeof(copied_after_wire_format) / sizeof(copied_after_wire_format[0]));
}

/**
 * One entry as basekit's build answers it, NULL with nothing raised when it is not valid.
 *
 * @implNote basekit spreads the entry, so a string or array entry contributes only index keys and
 * fails isValid; reading members of objects alone is the same outcome.
 */
static cJSON *build_one(const cJSON *entry, const cJSON *key, apps_error *error, const apps_facts *facts) {
    const cJSON *declared_id = member(entry, "id");
    const cJSON *id = apps_is_nullish(declared_id) ? key : declared_id;
    if (!is_valid(id, entry)) {
        return NULL;
    }
    cJSON *descriptor = cJSON_CreateObject();
    if (descriptor == NULL || !fill_descriptor(descriptor, entry, id, facts)) {
        cJSON_Delete(descriptor);
        apps_raise_out_of_memory(error);
        return NULL;
    }
    return descriptor;
}

static int append_entry(cJSON *apps, const cJSON *entry, const apps_facts *facts, apps_error *error) {
    if (cJSON_IsNull(entry)) {
        apps_raise(error, "the registry entry ", entry->string,
                   " is null, which makes basekit throw while reading every app",
                   "remove the entry from the registry or make it an object");
        return 0;
    }
    cJSON *key = cJSON_CreateString(entry->string);
    if (key == NULL) {
        apps_raise_out_of_memory(error);
        return 0;
    }
    cJSON *descriptor = build_one(entry, key, error, facts);
    cJSON_Delete(key);
    if (descriptor != NULL) {
        cJSON_AddItemToArray(apps, descriptor);
    }
    return !error->raised;
}

cJSON *apps_build(const apps_facts *facts, apps_error *error) {
    cJSON *registry = apps_read_registry(facts);
    cJSON *apps = cJSON_CreateArray();
    size_t count = 0;
    ordered_entry *entries = registry == NULL ? NULL : entries_in_order(registry, &count);
    if (registry == NULL || apps == NULL || entries == NULL) {
        apps_raise_out_of_memory(error);
    }
    for (size_t index = 0; !error->raised && index < count; index++) {
        append_entry(apps, entries[index].entry, facts, error);
    }
    free(entries);
    cJSON_Delete(registry);
    if (error->raised) {
        cJSON_Delete(apps);
        return NULL;
    }
    return apps;
}
