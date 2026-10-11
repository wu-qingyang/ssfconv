#ifndef SSFCONV_SKIN_IMAGE_H
#define SSFCONV_SKIN_IMAGE_H

/* 图片尺寸。 */
typedef struct {
    /* 图片宽度，单位为像素。 */
    unsigned width;
    /* 图片高度，单位为像素。 */
    unsigned height;
} SkinImageSize;

/**
 * @brief 读取 PNG 或 BMP 文件头并获取图片尺寸。
 * @param[in]  path 图片路径。
 * @param[out] size 接收图片尺寸的输出结构体。
 * @return 成功返回 0，文件不存在、格式不支持或参数无效时返回 -1。
 */
int skin_image_size(const char *path, SkinImageSize *size);

/**
 * @brief 计算 PNG 图片指定区域内的不透明像素平均颜色。
 * @param[in]  path   PNG 图片路径。
 * @param[in]  left   采样区域左边界。
 * @param[in]  right  采样区域右边界；非正值表示从图片右侧计算。
 * @param[in]  top    采样区域上边界。
 * @param[in]  bottom 采样区域下边界；非正值表示从图片底部计算。
 * @param[out] rgb    接收 RGB 三通道平均颜色的数组。
 * @return 成功返回 0，图片读取失败或参数无效时返回 -1。
 */
int skin_image_average(const char *path, int left, int right, int top, int bottom,
                  unsigned char rgb[3]);

/**
 * @brief 创建指定尺寸的全透明 PNG 图片。
 * @param[in]  path   输出图片路径。
 * @param[in]  width  图片宽度。
 * @param[in]  height 图片高度。
 * @return 成功返回 0，写入失败或参数无效时返回 -1。
 * @post 在 path 写入 PNG 文件。
 */
int skin_image_write_transparent_png(const char *path, unsigned width, unsigned height);

/**
 * @brief          写入指定方向的实心三角形 PNG 图片。
 * @param[in]      path      输出 PNG 文件路径。
 * @param[in]      width     图片宽度。
 * @param[in]      height    图片高度。
 * @param[in]      rgb       三角形颜色，包含三个颜色分量。
 * @param[in]      direction 方向，负值表示向左，非负值表示向右。
 * @return         成功返回 0，失败返回 -1。
 */
int skin_image_write_page_arrow_png(const char *path, unsigned width, unsigned height,
                                    const unsigned char rgb[3], int direction);

/**
 * @brief 创建指定尺寸的纯色不透明 PNG 图片。
 * @param[in]  path   输出图片路径。
 * @param[in]  width  图片宽度。
 * @param[in]  height 图片高度。
 * @param[in]  rgb    RGB 颜色数组。
 * @return 成功返回 0，写入失败或参数无效时返回 -1。
 * @post 在 path 写入 PNG 文件。
 */
int skin_image_write_solid_png(const char *path, unsigned width, unsigned height,
                          const unsigned char rgb[3]);

#endif
