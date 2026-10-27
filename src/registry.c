#include "apps_internal.h"

#include <stdlib.h>

#include "spisor/file.h"
#include "spisor/path.h"

#define ECOSYSTEM_DIRECTORY "forebay"

/** Where the registry lived while it was named after the dashboard; a read falls back to it, as basekit's does. */
#define LEGACY_DIRECTORY "cairn"

static char *registry_under(const apps_facts *facts, const char *directory) {
    const char *segments[] = {facts->home, ".config", directory, "apps.json"};
    return spisor_path_join(APPS_WINDOWS, segments, 4);
}

/** basekit's resolveExistingAppsFile; NULL only when memory runs out, which is raised. */
static char *registry_file(const apps_facts *facts, apps_error *error) {
    char *override = apps_trimmed_environment(facts, "HUB_APPS_FILE", error);
    if (override == NULL || override[0] != '\0') {
        return override;
    }
    free(override);
    char *canonical = registry_under(facts, ECOSYSTEM_DIRECTORY);
    if (canonical == NULL || spisor_file_exists(canonical)) {
        if (canonical == NULL) {
            apps_raise_out_of_memory(error);
        }
        return canonical;
    }
    char *legacy = registry_under(facts, LEGACY_DIRECTORY);
    if (legacy == NULL) {
        free(canonical);
        apps_raise_out_of_memory(error);
        return NULL;
    }
    if (spisor_file_exists(legacy)) {
        free(canonical);
        return legacy;
    }
    free(legacy);
    return canonical;
}

/**
 * basekit's readJson: read as Node reads UTF-8, strip whole-line comments, then JSON.parse. Answers
 * an empty object for anything JSON.parse refuses or that is not an object, NULL when memory runs out.
 *
 * @implNote An allocation failing inside cJSON itself is indistinguishable from a refusal through
 * spisor_parse_json, so only that one case still reads as an empty registry.
 */
static cJSON *parse_registry(const uint8_t *bytes, size_t length) {
    size_t decoded_length = 0;
    size_t text_length = 0;
    uint8_t *decoded = spisor_decode_utf8_lossy(bytes, length, &decoded_length);
    char *text = decoded == NULL ? NULL : apps_strip_comment_lines((const char *)decoded, decoded_length, &text_length);
    free(decoded);
    if (text == NULL) {
        return NULL;
    }
    cJSON *parsed = spisor_parse_json((const uint8_t *)text, text_length);
    free(text);
    if (!cJSON_IsObject(parsed)) {
        cJSON_Delete(parsed);
        return cJSON_CreateObject();
    }
    return parsed;
}

cJSON *apps_read_registry(const apps_facts *facts, apps_error *error) {
    char *file = registry_file(facts, error);
    if (file == NULL) {
        return NULL;
    }
    cJSON *registry = NULL;
    uint8_t *bytes = NULL;
    size_t length = 0;
    if (spisor_file_exists(file) && (bytes = spisor_read_file(file, &length)) != NULL) {
        registry = parse_registry(bytes, length);
        free(bytes);
    } else {
        registry = cJSON_CreateObject();
    }
    free(file);
    if (registry == NULL) {
        apps_raise_out_of_memory(error);
    }
    return registry;
}
