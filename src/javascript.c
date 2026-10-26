#include "apps_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SHORTEST_ROUND_TRIP_DIGITS 17
#define LARGEST_PLAIN_EXPONENT 21
#define SMALLEST_PLAIN_EXPONENT (-6)

int apps_is_javascript_space(uint32_t code_point) {
    switch (code_point) {
    case 0x09:
    case 0x0A:
    case 0x0B:
    case 0x0C:
    case 0x0D:
    case 0x20:
    case 0xA0:
    case 0x1680:
    case 0x2028:
    case 0x2029:
    case 0x202F:
    case 0x205F:
    case 0x3000:
    case 0xFEFF:
        return 1;
    default:
        return code_point >= 0x2000 && code_point <= 0x200A;
    }
}

/** The code point at text[index] of well-formed UTF-8, and how many bytes it takes. */
static uint32_t code_point_at(const unsigned char *text, size_t index, size_t *width) {
    unsigned char lead = text[index];
    if (lead < 0x80) {
        *width = 1;
        return lead;
    }
    size_t count = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : 2;
    uint32_t value = lead & (0x7F >> count);
    for (size_t offset = 1; offset < count; offset++) {
        value = (value << 6) | (text[index + offset] & 0x3F);
    }
    *width = count;
    return value;
}

static size_t last_code_point_start(const unsigned char *text, size_t end) {
    size_t start = end - 1;
    while (start > 0 && (text[start] & 0xC0) == 0x80) {
        start--;
    }
    return start;
}

char *apps_trim(const char *text) {
    const unsigned char *bytes = (const unsigned char *)text;
    size_t start = 0;
    size_t end = strlen(text);
    size_t width = 0;
    while (start < end && apps_is_javascript_space(code_point_at(bytes, start, &width))) {
        start += width;
    }
    while (end > start) {
        size_t last = last_code_point_start(bytes, end);
        if (!apps_is_javascript_space(code_point_at(bytes, last, &width))) {
            break;
        }
        end = last;
    }
    char *trimmed = malloc(end - start + 1);
    if (trimmed != NULL) {
        memcpy(trimmed, text + start, end - start);
        trimmed[end - start] = '\0';
    }
    return trimmed;
}

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

typedef struct shortest_digits {
    char digits[SHORTEST_ROUND_TRIP_DIGITS + 1];
    int point;
} shortest_digits;

/**
 * The fewest significant digits that read back as the same double, and where the decimal point
 * falls among them, which is what Number.prototype.toString prints from.
 */
static shortest_digits shortest_digits_of(double magnitude) {
    char printed[64];
    for (int precision = 1; precision <= SHORTEST_ROUND_TRIP_DIGITS; precision++) {
        snprintf(printed, sizeof(printed), "%.*e", precision - 1, magnitude);
        if (strtod(printed, NULL) == magnitude) {
            break;
        }
    }
    shortest_digits shortest;
    size_t count = 0;
    const char *cursor = printed;
    for (; *cursor != 'e'; cursor++) {
        if (*cursor >= '0' && *cursor <= '9') {
            shortest.digits[count++] = *cursor;
        }
    }
    while (count > 1 && shortest.digits[count - 1] == '0') {
        count--;
    }
    shortest.digits[count] = '\0';
    shortest.point = atoi(cursor + 1) + 1;
    return shortest;
}

static void append_zeros(char *out, int count) {
    size_t length = strlen(out);
    for (int index = 0; index < count; index++) {
        out[length++] = '0';
    }
    out[length] = '\0';
}

static char *javascript_number(double value) {
    if (value == 0) {
        return apps_copy("0");
    }
    if (isinf(value)) {
        return apps_copy(value > 0 ? "Infinity" : "-Infinity");
    }
    shortest_digits shortest = shortest_digits_of(fabs(value));
    int count = (int)strlen(shortest.digits);
    int point = shortest.point;
    char out[64] = "";
    if (value < 0) {
        strcat(out, "-");
    }
    if (count <= point && point <= LARGEST_PLAIN_EXPONENT) {
        strcat(out, shortest.digits);
        append_zeros(out, point - count);
    } else if (0 < point && point <= LARGEST_PLAIN_EXPONENT) {
        strncat(out, shortest.digits, (size_t)point);
        strcat(out, ".");
        strcat(out, shortest.digits + point);
    } else if (SMALLEST_PLAIN_EXPONENT < point && point <= 0) {
        strcat(out, "0.");
        append_zeros(out, -point);
        strcat(out, shortest.digits);
    } else {
        strncat(out, shortest.digits, 1);
        if (count > 1) {
            strcat(out, ".");
            strcat(out, shortest.digits + 1);
        }
        char exponent[16];
        snprintf(exponent, sizeof(exponent), "e%c%d", point - 1 >= 0 ? '+' : '-', abs(point - 1));
        strcat(out, exponent);
    }
    return apps_copy(out);
}

static char *javascript_array_join(const cJSON *array) {
    char *joined = apps_copy("");
    const cJSON *element = NULL;
    int first = 1;
    cJSON_ArrayForEach(element, array) {
        char *part = apps_is_nullish(element) ? apps_copy("") : apps_to_property_key(element);
        char *separated = joined == NULL || part == NULL ? NULL : apps_join(joined, first ? "" : ",");
        char *next = separated == NULL ? NULL : apps_join(separated, part);
        free(separated);
        free(part);
        free(joined);
        joined = next;
        first = 0;
    }
    return joined;
}

char *apps_to_property_key(const cJSON *value) {
    if (cJSON_IsString(value)) {
        return apps_copy(value->valuestring);
    }
    if (cJSON_IsNumber(value)) {
        return javascript_number(value->valuedouble);
    }
    if (cJSON_IsTrue(value)) {
        return apps_copy("true");
    }
    if (cJSON_IsFalse(value)) {
        return apps_copy("false");
    }
    if (cJSON_IsArray(value)) {
        return javascript_array_join(value);
    }
    if (cJSON_IsObject(value)) {
        return apps_copy("[object Object]");
    }
    return apps_copy("null");
}
