#ifndef APPS_INTERNAL_H
#define APPS_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "cJSON.h"
#include "spisor/json_kind.h"

#if defined(_WIN32)
#define APPS_WINDOWS 1
#else
#define APPS_WINDOWS 0
#endif

/** The facts one call reads, all taken from its context; nothing here comes from the process. */
typedef struct apps_facts {
    const spisor_call *call;
    const char *home;
    const cJSON *arguments;
} apps_facts;

/**
 * Where basekit would throw. The message and fix become the call's failure object.
 *
 * @implNote Only the first raise is kept, because basekit stops at its first throw.
 */
typedef struct apps_error {
    int raised;
    int out_of_memory;
    char *message;
    const char *fix;
} apps_error;

void apps_raise(apps_error *error, const char *prefix, const char *subject, const char *suffix, const char *fix);
void apps_raise_out_of_memory(apps_error *error);
void apps_clear_error(apps_error *error);

char *apps_copy(const char *text);
char *apps_join(const char *first, const char *second);

/**
 * The registry text as basekit's readJson hands it to JSON.parse: decoded the way Node decodes
 * UTF-8 (each maximal invalid subpart becomes U+FFFD), then stripped of every whole-line comment
 * as basekit's multiline regex strips it, and re-encoded as UTF-8.
 *
 * @implNote The regex's \s is JavaScript's, so it also eats a BOM or a no-break space in front of
 * the comment, and its ^ also holds after a CR, U+2028 or U+2029, while the comment itself runs to
 * the next LF only.
 */
char *apps_registry_text(const uint8_t *bytes, size_t length, size_t *text_length);

/**
 * True when JSON.parse accepts the text's grammar.
 *
 * @implNote cJSON is laxer than JSON.parse: it skips a BOM and every byte up to 0x20 as
 * whitespace, takes raw control characters inside a string, and reads numbers such as 01 and 1.
 * through strtod. Each of those makes JSON.parse throw, so basekit reads such a registry as empty,
 * and this check is what keeps cJSON from reading it at all.
 */
int apps_is_json_parse_text(const char *text, size_t length);

/** JavaScript's WhiteSpace and LineTerminator, which both \s and String.prototype.trim match. */
int apps_is_javascript_space(uint32_t code_point);

/** JavaScript's String.prototype.trim, on UTF-8. */
char *apps_trim(const char *text);

/** The registry as basekit's readRaw answers it: always an object, empty when absent or malformed. */
cJSON *apps_read_registry(const apps_facts *facts);

int apps_is_nullish(const cJSON *value);
int apps_is_truthy(const cJSON *value);

/** JavaScript's String(value), which is what a value used as a property key becomes. */
char *apps_to_property_key(const cJSON *value);

/**
 * basekit's trimmed(env[name]), answering a copy, or an empty copy when the variable is absent.
 *
 * @implNote basekit indexes a plain object, so a name such as toString that is not set answers the
 * member Object.prototype holds, and trimming that throws; this raises in the same place.
 */
char *apps_trimmed_environment(const apps_facts *facts, const char *name, apps_error *error);

/** The resolved descriptors in Object.entries order, or NULL when basekit's getApps throws. */
cJSON *apps_build(const apps_facts *facts, apps_error *error);

/** basekit's resolveHome for one resolved descriptor, or NULL when it throws. */
char *apps_resolve_home(const cJSON *descriptor, const apps_facts *facts, apps_error *error);

/** basekit's expandHome over every candidate, or NULL when one is not a string, which throws there. */
char **apps_expand_candidates(const cJSON *descriptor, const apps_facts *facts, size_t *count, apps_error *error);
void apps_free_strings(char **strings, size_t count);

const cJSON *apps_home_member(const cJSON *descriptor, const char *name);
const char *apps_descriptor_id(const cJSON *descriptor);

/** basekit's currentAppId, or NULL when it throws. */
char *apps_current_id(const apps_facts *facts, apps_error *error);

#endif
