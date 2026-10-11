#ifndef SSFCONV_SKIN_CONFIG_H
#define SSFCONV_SKIN_CONFIG_H

/* 配置项链表节点。 */
typedef struct Entry {
    /* 配置项键名。 */
    char *key;
    /* 配置项值。 */
    char *value;
    /* 同一配置节中的下一个配置项。 */
    struct Entry *next;
} Entry;

/* 配置节链表节点，例如 [General] 或 [Display]。 */
typedef struct Section {
    /* 配置节名称。 */
    char *name;
    /* 当前配置节的配置项链表。 */
    Entry *entries;
    /* 下一个配置节。 */
    struct Section *next;
} Section;

/* 已解析的皮肤配置。 */
typedef struct SkinConfig {
    /* 配置节链表。 */
    Section *sections;
} SkinConfig;

/**
 * @brief 读取并解析 skin.ini 或 Skin.ini 配置文件。
 * @param[in]  path 配置文件路径，不能为空。
 * @return 成功返回新建的 SkinConfig，失败返回 NULL。返回对象由
 *         skin_config_free() 释放。
 */
SkinConfig *skin_config_load(const char *path);

/**
 * @brief 释放配置对象及其包含的所有配置节和配置项。
 * @param[in]  config 配置对象，由 skin_config_load() 返回，允许为 NULL。
 */
void skin_config_free(SkinConfig *config);

/**
 * @brief 按配置节名称和键名查找配置值。
 * @param[in]  config  配置对象。
 * @param[in]  section 配置节名称。
 * @param[in]  key     配置项键名。
 * @return 配置值的只读指针；未找到或参数无效时返回 NULL。返回指针由
 *         config 所有，不得单独释放。
 */
const char *skin_config_get(const SkinConfig *config, const char *section, const char *key);

#endif
