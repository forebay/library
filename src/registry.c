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

/** basekit's resolveExistingAppsFile; NULL only when memory runs out. */
static char *registry_file(const apps_facts *facts) {
    apps_error unreachable = {0, 0, NULL, NULL};
    char *override = apps_trimmed_environment(facts, "HUB_APPS_FILE", &unreachable);
    apps_clear_error(&unreachable);
    if (override == NULL || override[0] != '\0') {
        return override;
    }
    free(override);
    char *canonical = registry_under(facts, ECOSYSTEM_DIRECTORY);
    if (canonical == NULL || spisor_file_exists(canonical)) {
        return canonical;
    }
    char *legacy = registry_under(facts, LEGACY_DIRECTORY);
    if (legacy != NULL && spisor_file_exists(legacy)) {
        free(canonical);
        return legacy;
    }
    free(legacy);
    return canonical;
}

static cJSON *parse_registry(const uint8_t *bytes, size_t length) {
    size_t text_length = 0;
    char *text = apps_registry_text(bytes, length, &text_length);
    if (text == NULL) {
        return NULL;
    }
    cJSON *parsed = apps_is_json_parse_text(text, text_length) ? spisor_parse_json((const uint8_t *)text, text_length)
                                                                : NULL;
    free(text);
    if (!cJSON_IsObject(parsed)) {
        cJSON_Delete(parsed);
        return NULL;
    }
    return parsed;
}

cJSON *apps_read_registry(const apps_facts *facts) {
    char *file = registry_file(facts);
    cJSON *registry = NULL;
    if (file != NULL && spisor_file_exists(file)) {
        size_t length = 0;
        uint8_t *bytes = spisor_read_file(file, &length);
        if (bytes != NULL) {
            registry = parse_registry(bytes, length);
            free(bytes);
        }
    }
    free(file);
    return registry == NULL ? cJSON_CreateObject() : registry;
}
