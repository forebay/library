#include "apps_internal.h"

#include <stdlib.h>
#include <string.h>

char *apps_copy(const char *text) {
    size_t length = strlen(text);
    char *copy = malloc(length + 1);
    if (copy != NULL) {
        memcpy(copy, text, length + 1);
    }
    return copy;
}

char *apps_join(const char *first, const char *second) {
    size_t first_length = strlen(first);
    size_t second_length = strlen(second);
    char *joined = malloc(first_length + second_length + 1);
    if (joined != NULL) {
        memcpy(joined, first, first_length);
        memcpy(joined + first_length, second, second_length + 1);
    }
    return joined;
}

void apps_raise(apps_error *error, const char *prefix, const char *subject, const char *suffix, const char *fix) {
    if (error->raised) {
        return;
    }
    error->raised = 1;
    error->fix = fix;
    char *head = apps_join(prefix, subject);
    error->message = head == NULL ? NULL : apps_join(head, suffix);
    free(head);
    if (error->message == NULL) {
        error->out_of_memory = 1;
    }
}

void apps_raise_out_of_memory(apps_error *error) {
    if (!error->raised) {
        error->raised = 1;
        error->out_of_memory = 1;
    }
}

void apps_clear_error(apps_error *error) {
    free(error->message);
    error->message = NULL;
    error->raised = 0;
    error->out_of_memory = 0;
    error->fix = NULL;
}

void apps_free_strings(char **strings, size_t count) {
    if (strings == NULL) {
        return;
    }
    for (size_t index = 0; index < count; index++) {
        free(strings[index]);
    }
    free(strings);
}

const cJSON *apps_home_member(const cJSON *descriptor, const char *name) {
    return cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(descriptor, "home"), name);
}

const char *apps_descriptor_id(const cJSON *descriptor) {
    return cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(descriptor, "id"));
}
