#include "apps_internal.h"

#include <stdlib.h>
#include <string.h>

#include "spisor/javascript.h"

/** True when the code point ending just before index is a JavaScript LineTerminator, which is where a multiline ^ holds. */
static int follows_line_terminator(const unsigned char *text, size_t index) {
    if (index == 0) {
        return 1;
    }
    unsigned char last = text[index - 1];
    if (last == '\n' || last == '\r') {
        return 1;
    }
    return index >= 3 && text[index - 3] == 0xE2 && text[index - 2] == 0x80 && (last == 0xA8 || last == 0xA9);
}

/** Where a comment match starting at index ends, or index itself when no match starts there. */
static size_t comment_match_end(const char *text, size_t length, size_t index) {
    if (!follows_line_terminator((const unsigned char *)text, index)) {
        return index;
    }
    size_t cursor = index;
    while (cursor < length) {
        size_t next = cursor;
        if (!spisor_is_javascript_space(spisor_utf8_next(text, &next))) {
            break;
        }
        cursor = next;
    }
    if (cursor + 1 >= length || text[cursor] != '/' || text[cursor + 1] != '/') {
        return index;
    }
    const char *line_feed = memchr(text + cursor, '\n', length - cursor);
    return line_feed == NULL ? length : (size_t)(line_feed - text);
}

char *apps_strip_comment_lines(const char *text, size_t length, size_t *stripped_length) {
    char *kept = malloc(length + 1);
    if (kept == NULL) {
        return NULL;
    }
    size_t kept_length = 0;
    size_t index = 0;
    while (index < length) {
        size_t end = comment_match_end(text, length, index);
        if (end > index) {
            index = end;
            continue;
        }
        size_t next = index;
        spisor_utf8_next(text, &next);
        memcpy(kept + kept_length, text + index, next - index);
        kept_length += next - index;
        index = next;
    }
    kept[kept_length] = '\0';
    *stripped_length = kept_length;
    return kept;
}
