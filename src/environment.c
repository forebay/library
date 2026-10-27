#include "apps_internal.h"

#include <string.h>

static const char *const object_prototype_members[] = {
    "constructor",      "__defineGetter__",     "__defineSetter__", "hasOwnProperty",
    "__lookupGetter__", "__lookupSetter__",     "isPrototypeOf",    "propertyIsEnumerable",
    "toString",         "valueOf",              "__proto__",        "toLocaleString",
};

static int names_an_object_prototype_member(const char *name) {
    for (size_t index = 0; index < sizeof(object_prototype_members) / sizeof(object_prototype_members[0]); index++) {
        if (strcmp(object_prototype_members[index], name) == 0) {
            return 1;
        }
    }
    return 0;
}

char *apps_trimmed_environment(const apps_facts *facts, const char *name, apps_error *error) {
    const char *value = spisor_environment(facts->call, name);
    if (value != NULL) {
        char *trimmed = spisor_javascript_trim(value);
        if (trimmed == NULL) {
            apps_raise_out_of_memory(error);
        }
        return trimmed;
    }
    if (names_an_object_prototype_member(name)) {
        apps_raise(error, "basekit throws reading the environment variable ", name,
                   " while it is unset, because a JavaScript object answers that name with a member of its own",
                   "set the variable, or rename it in the registry");
        return NULL;
    }
    char *empty = apps_copy("");
    if (empty == NULL) {
        apps_raise_out_of_memory(error);
    }
    return empty;
}
