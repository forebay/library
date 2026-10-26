#include "apps_internal.h"

#include <stdlib.h>
#include <string.h>

/**
 * @implNote ASCII only, where basekit lowercases with JavaScript's full Unicode mapping; the two
 * differ only for a non-ASCII letter, which the parity fixtures record rather than paper over.
 */
static void lowercase_ascii(char *text) {
    for (; *text != '\0'; text++) {
        if (*text >= 'A' && *text <= 'Z') {
            *text = (char)(*text - 'A' + 'a');
        }
    }
}

static char *lowercased(const char *text) {
    char *copy = apps_copy(text);
    if (copy != NULL) {
        lowercase_ascii(copy);
    }
    return copy;
}

static int is_token_character(char character) {
    return (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9');
}

/** basekit's hasArgvToken over a lowercased argv: the binary must equal one whole [a-z0-9]+ run. */
static int has_token(const char *argv, const char *binary) {
    size_t length = strlen(binary);
    const char *cursor = argv;
    while (*cursor != '\0') {
        while (*cursor != '\0' && !is_token_character(*cursor)) {
            cursor++;
        }
        const char *start = cursor;
        while (is_token_character(*cursor)) {
            cursor++;
        }
        if ((size_t)(cursor - start) == length && length > 0 && memcmp(start, binary, length) == 0) {
            return 1;
        }
    }
    return 0;
}

static char *joined_arguments(const apps_facts *facts) {
    char *joined = apps_copy("");
    const cJSON *argument = NULL;
    int first = 1;
    cJSON_ArrayForEach(argument, facts->arguments) {
        char *separated = joined == NULL ? NULL : apps_join(joined, first ? "" : " ");
        char *next = separated == NULL ? NULL : apps_join(separated, argument->valuestring);
        free(separated);
        free(joined);
        joined = next;
        first = 0;
    }
    if (joined != NULL) {
        lowercase_ascii(joined);
    }
    return joined;
}

/** basekit filters every app before taking the first, so a binary that throws does so whatever matched earlier. */
static const cJSON *argument_hit(const cJSON *apps, const apps_facts *facts, apps_error *error) {
    char *argv = joined_arguments(facts);
    if (argv == NULL) {
        apps_raise_out_of_memory(error);
        return NULL;
    }
    const cJSON *hit = NULL;
    const cJSON *app = NULL;
    cJSON_ArrayForEach(app, apps) {
        const cJSON *binary = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(app, "detect"), "binary");
        if (!apps_is_truthy(binary)) {
            continue;
        }
        if (!cJSON_IsString(binary)) {
            apps_raise(error, "app ", apps_descriptor_id(app),
                       " declares a detect.binary that is not a string, which makes basekit throw matching it",
                       "make detect.binary in the registry an executable name");
            break;
        }
        char *wanted = lowercased(binary->valuestring);
        if (wanted == NULL) {
            apps_raise_out_of_memory(error);
            break;
        }
        if (hit == NULL && has_token(argv, wanted)) {
            hit = app;
        }
        free(wanted);
    }
    free(argv);
    return error->raised ? NULL : hit;
}

static const cJSON *environment_hit(const cJSON *apps, const apps_facts *facts, apps_error *error) {
    const cJSON *hit = NULL;
    const cJSON *app = NULL;
    cJSON_ArrayForEach(app, apps) {
        const cJSON *native = apps_home_member(app, "nativeEnv");
        if (!apps_is_truthy(native)) {
            continue;
        }
        char *key = apps_to_property_key(native);
        char *value = key == NULL ? NULL : apps_trimmed_environment(facts, key, error);
        free(key);
        if (value == NULL) {
            apps_raise_out_of_memory(error);
            break;
        }
        if (hit == NULL && value[0] != '\0') {
            hit = app;
        }
        free(value);
    }
    return error->raised ? NULL : hit;
}

/** basekit's normalizePath: backslashes become slashes, trailing slashes go, and the rest is lowercased. */
static char *normalized_path(const char *path) {
    char *normal = lowercased(path);
    if (normal == NULL) {
        return NULL;
    }
    for (char *cursor = normal; *cursor != '\0'; cursor++) {
        if (*cursor == '\\') {
            *cursor = '/';
        }
    }
    size_t length = strlen(normal);
    while (length > 0 && normal[length - 1] == '/') {
        normal[--length] = '\0';
    }
    return normal;
}

static int starts_below(const char *path, const char *prefix) {
    size_t length = strlen(prefix);
    return strncmp(path, prefix, length) == 0 && path[length] == '/';
}

static int ends_with(const char *text, const char *suffix) {
    size_t text_length = strlen(text);
    size_t suffix_length = strlen(suffix);
    return text_length >= suffix_length && strcmp(text + text_length - suffix_length, suffix) == 0;
}

static int prefix_match(const char *forced, const char *candidate) {
    char *normal_candidate = normalized_path(candidate);
    int matched = normal_candidate != NULL && forced[0] != '\0' && normal_candidate[0] != '\0' &&
                  (strcmp(forced, normal_candidate) == 0 || starts_below(forced, normal_candidate) ||
                   starts_below(normal_candidate, forced));
    free(normal_candidate);
    return matched;
}

static int any_candidate_matches(const char *forced, const cJSON *app, const apps_facts *facts, apps_error *error) {
    size_t count = 0;
    char **candidates = apps_expand_candidates(app, facts, &count, error);
    int matched = 0;
    for (size_t index = 0; candidates != NULL && !matched && index < count; index++) {
        matched = prefix_match(forced, candidates[index]);
    }
    apps_free_strings(candidates, count);
    return matched;
}

static int xdg_suffix_matches(const char *forced, const cJSON *app, apps_error *error) {
    const cJSON *subdirectory = apps_home_member(app, "xdgSubdir");
    if (!apps_is_truthy(subdirectory)) {
        return 0;
    }
    if (!cJSON_IsString(subdirectory)) {
        apps_raise(error, "app ", apps_descriptor_id(app),
                   " declares an xdgSubdir that is not a string, which makes basekit throw matching it",
                   "make home.xdgSubdir in the registry a directory name");
        return 0;
    }
    char *lowered = lowercased(subdirectory->valuestring);
    char *suffix = lowered == NULL ? NULL : apps_join("/", lowered);
    int matched = suffix != NULL && ends_with(forced, suffix);
    free(suffix);
    free(lowered);
    return matched;
}

static int last_segment_names(const char *forced, const cJSON *app) {
    if (!apps_is_truthy(apps_home_member(app, "nativeEnv"))) {
        return 0;
    }
    const char *slash = strrchr(forced, '/');
    char *id = lowercased(apps_descriptor_id(app));
    int matched = id != NULL && strcmp(slash == NULL ? forced : slash + 1, id) == 0;
    free(id);
    return matched;
}

/** basekit's forcedDirMatchesApp, with forced already normalised. */
static int forced_directory_matches(const char *forced, const cJSON *app, const apps_facts *facts, apps_error *error) {
    if (any_candidate_matches(forced, app, facts, error) || error->raised) {
        return !error->raised;
    }
    if (xdg_suffix_matches(forced, app, error) || error->raised) {
        return !error->raised;
    }
    return last_segment_names(forced, app);
}

static const cJSON *forced_directory_hit(const cJSON *apps, const apps_facts *facts, apps_error *error) {
    char *forced = apps_trimmed_environment(facts, "HUB_CONFIG_DIR", error);
    char *normal = forced == NULL ? NULL : normalized_path(forced);
    const cJSON *hit = NULL;
    if (forced != NULL && forced[0] != '\0') {
        const cJSON *app = NULL;
        cJSON_ArrayForEach(app, apps) {
            if (normal == NULL) {
                apps_raise_out_of_memory(error);
            }
            if (error->raised || forced_directory_matches(normal, app, facts, error)) {
                hit = error->raised ? NULL : app;
                break;
            }
        }
    }
    free(normal);
    free(forced);
    return hit;
}

static const cJSON *first_hit(const cJSON *apps, const apps_facts *facts, apps_error *error) {
    const cJSON *hit = argument_hit(apps, facts, error);
    if (hit == NULL && !error->raised) {
        hit = environment_hit(apps, facts, error);
    }
    if (hit == NULL && !error->raised) {
        hit = forced_directory_hit(apps, facts, error);
    }
    return hit;
}

char *apps_current_id(const apps_facts *facts, apps_error *error) {
    char *override = apps_trimmed_environment(facts, "CORE_APP", error);
    if (override == NULL || override[0] != '\0') {
        return override;
    }
    free(override);
    cJSON *apps = apps_build(facts, error);
    if (apps == NULL) {
        return NULL;
    }
    const cJSON *hit = first_hit(apps, facts, error);
    char *id = error->raised ? NULL : apps_copy(hit == NULL ? "" : apps_descriptor_id(hit));
    cJSON_Delete(apps);
    if (id == NULL && !error->raised) {
        apps_raise_out_of_memory(error);
    }
    return id;
}
