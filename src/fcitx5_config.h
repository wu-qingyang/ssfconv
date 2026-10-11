#ifndef SSFCONV_FCITX5_CONFIG_H
#define SSFCONV_FCITX5_CONFIG_H

/**
 * @brief 将 Fcitx5 Classic UI 的主题配置更新为指定主题。
 * @param[in] theme_name 主题目录名称。
 * @return 成功返回 0，用户配置目录不可用或写入失败时返回 -1。
 */
int fcitx5_config_set_theme(const char *theme_name);

/**
 * @brief 请求运行中的 Fcitx5 重新加载配置。
 * @return 成功返回 0，Fcitx5 未运行或重启请求失败时返回 -1。
 */
int fcitx5_config_restart(void);

#endif
