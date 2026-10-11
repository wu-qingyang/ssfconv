#include "skin_image.h"

#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int skin_image_size(const char *path, SkinImageSize *size)
{
    if (!path || !*path || !size) {
        return -1;
    }
    FILE *file = fopen(path, "rb");
    if (!file) {
        return -1;
    }
    unsigned char header[32];
    size_t got = fread(header, 1, sizeof(header), file);
    fclose(file);
    if (got >= 24 && png_sig_cmp(header, 0, 8) == 0) {
        size->width = ((unsigned)header[16] << 24) | ((unsigned)header[17] << 16) | ((unsigned)header[18] << 8) | header[19];
        size->height = ((unsigned)header[20] << 24) | ((unsigned)header[21] << 16) | ((unsigned)header[22] << 8) | header[23];
        return 0;
    }
    if (got >= 26 && header[0] == 'B' && header[1] == 'M') {
        size->width = (unsigned)header[18] | ((unsigned)header[19] << 8) | ((unsigned)header[20] << 16) | ((unsigned)header[21] << 24);
        size->height = (unsigned)header[22] | ((unsigned)header[23] << 8) | ((unsigned)header[24] << 16) | ((unsigned)header[25] << 24);
        return size->width && size->height ? 0 : -1;
    }
    return -1;
}

int skin_image_average(const char *path, int left, int right, int top, int bottom, unsigned char rgb[3])
{
    if (!path || !*path || !rgb) {
        return -1;
    }
    png_image image;
    memset(&image, 0, sizeof(image));
    image.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_file(&image, path)) {
        return -1;
    }
    image.format = PNG_FORMAT_RGBA;
    size_t bytes = PNG_IMAGE_SIZE(image);
    unsigned char *pixels = malloc(bytes);
    if (!pixels || !png_image_finish_read(&image, NULL, pixels, 0, NULL)) {
        free(pixels);
        png_image_free(&image);
        return -1;
    }
    int x1 = left < 0 ? 0 : left, y1 = top < 0 ? 0 : top;
    int x2 = right <= 0 ? (int)image.width + right : right;
    int y2 = bottom <= 0 ? (int)image.height + bottom : bottom;
    if (x2 > (int)image.width) {
        x2 = (int)image.width;
    }
    if (y2 > (int)image.height) {
        y2 = (int)image.height;
    }
    if (x1 >= x2 || y1 >= y2) {
        x1 = y1 = 0;
        x2 = (int)image.width;
        y2 = (int)image.height;
    }
    unsigned long long total[3] = {0, 0, 0};
    unsigned long long count = 0;
    for (int y = y1; y < y2; ++y) {
        for (int x = x1; x < x2; ++x) {
            unsigned char *p = pixels + ((size_t)y * image.width + x) * 4;
            if (p[3]) {
                total[0] += p[0];
                total[1] += p[1];
                total[2] += p[2];
                ++count;
            }
        }
    }
    if (!count) {
        count = 1;
    }
    rgb[0] = (unsigned char)(total[0] / count);
    rgb[1] = (unsigned char)(total[1] / count);
    rgb[2] = (unsigned char)(total[2] / count);
    free(pixels);
    png_image_free(&image);
    return 0;
}

static int write_png(const char *path, unsigned width, unsigned height, const unsigned char *pixels)
{
    png_image image;
    memset(&image, 0, sizeof(image));
    image.version = PNG_IMAGE_VERSION;
    image.width = width;
    image.height = height;
    image.format = PNG_FORMAT_RGBA;
    int ok = png_image_write_to_file(&image, path, 0, pixels, 0, NULL);
    return ok ? 0 : -1;
}

int skin_image_write_transparent_png(const char *path, unsigned width, unsigned height)
{
    if (!path || !*path || width == 0 || height == 0) {
        return -1;
    }
    size_t size = (size_t)width * height * 4;
    unsigned char *pixels = calloc(1, size);
    if (!pixels) {
        return -1;
    }
    int rc = write_png(path, width, height, pixels);
    free(pixels);
    return rc;
}

static int point_in_triangle(double px, double py,
                             double ax, double ay,
                             double bx, double by,
                             double cx, double cy)
{
    double ab = (bx - ax) * (py - ay) - (by - ay) * (px - ax);
    double bc = (cx - bx) * (py - by) - (cy - by) * (px - bx);
    double ca = (ax - cx) * (py - cy) - (ay - cy) * (px - cx);
    return (ab >= 0 && bc >= 0 && ca >= 0) ||
           (ab <= 0 && bc <= 0 && ca <= 0);
}

int skin_image_write_page_arrow_png(const char *path, unsigned width, unsigned height,
                                    const unsigned char rgb[3], int direction)
{
    if (!path || !*path || width == 0 || height == 0 || !rgb) {
        return -1;
    }
    size_t size = (size_t)width * height * 4;
    unsigned char *pixels = calloc(1, size);
    if (!pixels) {
        return -1;
    }
    double arrow_width = width < 6 ? (double)width : 6.0;
    double arrow_height = height < 10 ? (double)height : 10.0;
    double left = ((double)width - arrow_width) / 2.0;
    double right = left + arrow_width - 1.0;
    double top = ((double)height - arrow_height) / 2.0;
    double bottom = top + arrow_height - 1.0;
    double center_y = (top + bottom) / 2.0;
    double base_x = direction < 0 ? right : left;
    double tip_x = direction < 0 ? left : right;
    for (unsigned y = 0; y < height; ++y) {
        for (unsigned x = 0; x < width; ++x) {
            if (!point_in_triangle((double)x, (double)y,
                                   base_x, top, base_x, bottom, tip_x, center_y)) {
                continue;
            }
            size_t offset = ((size_t)y * width + x) * 4;
            pixels[offset] = rgb[0];
            pixels[offset + 1] = rgb[1];
            pixels[offset + 2] = rgb[2];
            pixels[offset + 3] = 255;
        }
    }
    int rc = write_png(path, width, height, pixels);
    free(pixels);
    return rc;
}

int skin_image_write_solid_png(const char *path, unsigned width, unsigned height, const unsigned char rgb[3])
{
    if (!path || !*path || width == 0 || height == 0 || !rgb) {
        return -1;
    }
    size_t size = (size_t)width * height * 4;
    unsigned char *pixels = malloc(size);
    if (!pixels) {
        return -1;
    }
    for (size_t i = 0; i < size; i += 4) {
        pixels[i] = rgb[0];
        pixels[i + 1] = rgb[1];
        pixels[i + 2] = rgb[2];
        pixels[i + 3] = 255;
    }
    int rc = write_png(path, width, height, pixels);
    free(pixels);
    return rc;
}
