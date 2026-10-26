#include "apps_internal.h"

#include <stdlib.h>

#define REPLACEMENT_CHARACTER 0xFFFDu

typedef struct code_points {
    uint32_t *values;
    size_t count;
} code_points;

typedef struct sequence_rule {
    size_t continuations;
    uint32_t initial;
    uint8_t lower;
    uint8_t upper;
} sequence_rule;

/** The WHATWG UTF-8 decoder's table: how many bytes follow a lead, and the range the next one must fall in. */
static int sequence_rule_for(uint8_t lead, sequence_rule *rule) {
    rule->lower = 0x80;
    rule->upper = 0xBF;
    if (lead >= 0xC2 && lead <= 0xDF) {
        rule->continuations = 1;
        rule->initial = lead & 0x1Fu;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
        rule->continuations = 2;
        rule->initial = lead & 0x0Fu;
        rule->lower = lead == 0xE0 ? 0xA0 : 0x80;
        rule->upper = lead == 0xED ? 0x9F : 0xBF;
    } else if (lead >= 0xF0 && lead <= 0xF4) {
        rule->continuations = 3;
        rule->initial = lead & 0x07u;
        rule->lower = lead == 0xF0 ? 0x90 : 0x80;
        rule->upper = lead == 0xF4 ? 0x8F : 0xBF;
    } else {
        return 0;
    }
    return 1;
}

/**
 * Node's UTF-8 decoding: each maximal invalid subpart is one U+FFFD, and the byte that ends it is
 * read again as the start of what follows.
 */
static code_points decode_replacing(const uint8_t *bytes, size_t length) {
    code_points decoded = {malloc((length == 0 ? 1 : length) * sizeof(uint32_t)), 0};
    if (decoded.values == NULL) {
        return decoded;
    }
    size_t index = 0;
    while (index < length) {
        uint8_t lead = bytes[index++];
        sequence_rule rule;
        if (lead < 0x80) {
            decoded.values[decoded.count++] = lead;
            continue;
        }
        if (!sequence_rule_for(lead, &rule)) {
            decoded.values[decoded.count++] = REPLACEMENT_CHARACTER;
            continue;
        }
        uint32_t value = rule.initial;
        size_t seen = 0;
        while (seen < rule.continuations && index < length && bytes[index] >= rule.lower &&
               bytes[index] <= rule.upper) {
            value = (value << 6) | (bytes[index++] & 0x3Fu);
            rule.lower = 0x80;
            rule.upper = 0xBF;
            seen++;
        }
        decoded.values[decoded.count++] = seen == rule.continuations ? value : REPLACEMENT_CHARACTER;
    }
    return decoded;
}

static int is_line_terminator(uint32_t code_point) {
    return code_point == '\n' || code_point == '\r' || code_point == 0x2028 || code_point == 0x2029;
}

/** Where a comment match starting at index ends, or index itself when no match starts there. */
static size_t comment_match_end(const code_points *text, size_t index) {
    if (index > 0 && !is_line_terminator(text->values[index - 1])) {
        return index;
    }
    size_t cursor = index;
    while (cursor < text->count && apps_is_javascript_space(text->values[cursor])) {
        cursor++;
    }
    if (cursor + 1 >= text->count || text->values[cursor] != '/' || text->values[cursor + 1] != '/') {
        return index;
    }
    cursor += 2;
    while (cursor < text->count && text->values[cursor] != '\n') {
        cursor++;
    }
    return cursor;
}

/**
 * @implNote A match's ^ is tested against the ORIGINAL text, as String.prototype.replace scans the
 * string it was given, so a comment ending in CR (a CRLF file) lets the next match start at its LF.
 */
static void strip_comment_lines(code_points *text) {
    uint32_t *kept = malloc((text->count == 0 ? 1 : text->count) * sizeof(uint32_t));
    if (kept == NULL) {
        free(text->values);
        text->values = NULL;
        return;
    }
    size_t kept_count = 0;
    size_t index = 0;
    while (index < text->count) {
        size_t end = comment_match_end(text, index);
        if (end > index) {
            index = end;
            continue;
        }
        kept[kept_count++] = text->values[index++];
    }
    free(text->values);
    text->values = kept;
    text->count = kept_count;
}

static size_t encode(uint32_t code_point, char *out) {
    if (code_point < 0x80) {
        out[0] = (char)code_point;
        return 1;
    }
    if (code_point < 0x800) {
        out[0] = (char)(0xC0 | (code_point >> 6));
        out[1] = (char)(0x80 | (code_point & 0x3F));
        return 2;
    }
    if (code_point < 0x10000) {
        out[0] = (char)(0xE0 | (code_point >> 12));
        out[1] = (char)(0x80 | ((code_point >> 6) & 0x3F));
        out[2] = (char)(0x80 | (code_point & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (code_point >> 18));
    out[1] = (char)(0x80 | ((code_point >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((code_point >> 6) & 0x3F));
    out[3] = (char)(0x80 | (code_point & 0x3F));
    return 4;
}

char *apps_registry_text(const uint8_t *bytes, size_t length, size_t *text_length) {
    code_points text = decode_replacing(bytes, length);
    if (text.values != NULL) {
        strip_comment_lines(&text);
    }
    if (text.values == NULL) {
        return NULL;
    }
    char *encoded = malloc(text.count * 4 + 1);
    if (encoded != NULL) {
        size_t written = 0;
        for (size_t index = 0; index < text.count; index++) {
            written += encode(text.values[index], encoded + written);
        }
        encoded[written] = '\0';
        *text_length = written;
    }
    free(text.values);
    return encoded;
}
