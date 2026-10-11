#ifndef SSFCONV_I18N_H
#define SSFCONV_I18N_H

/**
 * @brief 根据当前语言环境选择英文或中文提示文本。
 * @param[in]  english 英文文本，可为 NULL。
 * @param[in]  chinese 中文文本，可为 NULL。
 * @return 选中的文本指针，指向调用者提供的字符串或空字符串。
 */
const char *localized_message(const char *english, const char *chinese);

#endif
