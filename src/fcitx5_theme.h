#ifndef SSFCONV_FCITX5_THEME_H
#define SSFCONV_FCITX5_THEME_H

/**
 * @brief 读取已解包的 SSF 皮肤并生成 Fcitx5 主题文件。
 * @param[in]  skin_dir 已解包皮肤目录路径，不能为空。
 * @return 成功返回 0，失败返回 -1。
 * @post 在 skin_dir 中生成 theme.conf 和主题图片文件。
 */
int fcitx5_theme_write(const char *skin_dir);

#endif
