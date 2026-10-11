#include "i18n.h"

#include <stdlib.h>
#include <string.h>

static int is_chinese_locale(void)
{
    const char *locale = getenv("LC_ALL");
    if (!locale || !*locale) {
        locale = getenv("LC_MESSAGES");
    }
    if (!locale || !*locale) {
        locale = getenv("LANG");
    }
    return locale && strncmp(locale, "zh", 2) == 0;
}

const char *localized_message(const char *english, const char *chinese)
{
    if (!english) {
        english = "";
    }
    if (!chinese) {
        chinese = english;
    }
    return is_chinese_locale() ? chinese : english;
}
