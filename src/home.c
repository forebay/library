#include "apps_internal.h"

#include <stdlib.h>

#include "spisor/file.h"
#include "spisor/path.h"

/** basekit's `home.<name> ? trimmed(env[home.<name>]) : ""`, the name keyed as JavaScript keys any value. */
static char *variable_named_by(const cJSON *descriptor, const char *name, const apps_facts *facts, apps_error *error) {
    const cJSON *declared = apps_home_member(descriptor, name);
    if (!apps_is_truthy(declared)) {
        return apps_copy("");
    }
    char *key = apps_to_property_key(declared);
    if (key == NULL) {
        apps_raise_out_of_memory(error);
        return NULL;
    }
    char *value = apps_trimmed_environment(facts, key, error);
    free(key);
    return value;
}

char **apps_expand_candidates(const cJSON *descriptor, const apps_facts *facts, size_t *count, apps_error *error) {
    const cJSON *candidates = apps_home_member(descriptor, "candidates");
    *count = (size_t)cJSON_GetArraySize(candidates);
    char **expanded = calloc(*count == 0 ? 1 : *count, sizeof(char *));
    if (expanded == NULL) {
        apps_raise_out_of_memory(error);
        return NULL;
    }
    size_t index = 0;
    const cJSON *candidate = NULL;
    cJSON_ArrayForEach(candidate, candidates) {
        if (!cJSON_IsString(candidate)) {
            apps_raise(error, "app ", apps_descriptor_id(descriptor),
                       " declares a home candidate that is not a string, which makes basekit throw expanding it",
                       "make every entry of home.candidates in the registry a path string");
            break;
        }
        expanded[index] = spisor_expand_home(APPS_WINDOWS, candidate->valuestring, facts->home);
        if (expanded[index++] == NULL) {
            apps_raise_out_of_memory(error);
            break;
        }
    }
    if (error->raised) {
        apps_free_strings(expanded, *count);
        return NULL;
    }
    return expanded;
}

static char *xdg_home(const cJSON *descriptor, const apps_facts *facts, apps_error *error) {
    const cJSON *subdirectory = apps_home_member(descriptor, "xdgSubdir");
    if (!apps_is_truthy(subdirectory)) {
        return apps_copy("");
    }
    char *xdg = apps_trimmed_environment(facts, "XDG_CONFIG_HOME", error);
    if (xdg == NULL || xdg[0] == '\0') {
        return xdg;
    }
    char *joined = NULL;
    if (!cJSON_IsString(subdirectory)) {
        apps_raise(error, "app ", apps_descriptor_id(descriptor),
                   " declares an xdgSubdir that is not a string, which makes basekit throw joining it",
                   "make home.xdgSubdir in the registry a directory name");
    } else {
        const char *segments[] = {xdg, subdirectory->valuestring};
        joined = spisor_path_join(APPS_WINDOWS, segments, 2);
        if (joined == NULL) {
            apps_raise_out_of_memory(error);
        }
    }
    free(xdg);
    return joined;
}

static char *existing_or_last(char **candidates, size_t count) {
    for (size_t index = 0; index < count; index++) {
        if (spisor_file_exists(candidates[index])) {
            return apps_copy(candidates[index]);
        }
    }
    return apps_copy(count == 0 ? "" : candidates[count - 1]);
}

/** Answers the step's path when it settles the home, and NULL to move on or when it raised. */
static char *settled(char *step) {
    if (step != NULL && step[0] == '\0') {
        free(step);
        return NULL;
    }
    return step;
}

char *apps_resolve_home(const cJSON *descriptor, const apps_facts *facts, apps_error *error) {
    char *home = settled(variable_named_by(descriptor, "envOverride", facts, error));
    if (home == NULL && !error->raised) {
        home = settled(variable_named_by(descriptor, "nativeEnv", facts, error));
    }
    if (home == NULL && !error->raised) {
        home = settled(xdg_home(descriptor, facts, error));
    }
    if (home != NULL || error->raised) {
        return home;
    }
    size_t count = 0;
    char **candidates = apps_expand_candidates(descriptor, facts, &count, error);
    if (candidates == NULL) {
        return NULL;
    }
    home = existing_or_last(candidates, count);
    apps_free_strings(candidates, count);
    if (home == NULL) {
        apps_raise_out_of_memory(error);
    }
    return home;
}
