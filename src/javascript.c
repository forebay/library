#include "apps_internal.h"

int apps_is_nullish(const cJSON *value) {
    return value == NULL || cJSON_IsNull(value);
}

int apps_is_truthy(const cJSON *value) {
    if (apps_is_nullish(value) || cJSON_IsFalse(value)) {
        return 0;
    }
    if (cJSON_IsNumber(value)) {
        return value->valuedouble != 0;
    }
    if (cJSON_IsString(value)) {
        return value->valuestring[0] != '\0';
    }
    return 1;
}
