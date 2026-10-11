#ifndef SSFCONV_SSF_ARCHIVE_H
#define SSFCONV_SSF_ARCHIVE_H

/**
 * @brief 解包普通 ZIP 或 Skin 加密格式的 SSF 文件。
 * @param[in]  input_path SSF 文件路径。
 * @param[in]  output_dir 解包输出目录路径。
 * @return 成功返回 0，解包失败或参数无效时返回 -1。
 * @post 在 output_dir 写入解包后的皮肤文件。
 */
int ssf_archive_extract(const char *input_path, const char *output_dir);

#endif
