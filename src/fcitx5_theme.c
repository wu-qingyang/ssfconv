#define _GNU_SOURCE
#include "fcitx5_theme.h"
#include "skin_image.h"
#include "i18n.h"
#include "skin_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *theme_first_image(const char *value, char *buffer, size_t size)
{
    if (!value) {
        return NULL;
    }
    size_t n = strcspn(value, ",");
    if (n >= size) {
        return NULL;
    }
    memcpy(buffer, value, n);
    buffer[n] = '\0';
    return buffer;
}

static void path_join(char *out, size_t size, const char *dir, const char *name)
{
    snprintf(out, size, "%s/%s", dir, name);
}

static int skin_config_get_int(const SkinConfig *config, const char *section, const char *key, int fallback)
{
    const char *value = skin_config_get(config, section, key);
    return value ? atoi(value) : fallback;
}

static int skin_config_get_int_list4(const SkinConfig *config, const char *section, const char *key, int output[4])
{
    const char *value = skin_config_get(config, section, key);
    if (!value) {
        return -1;
    }
    char *copy = strdup(value);
    if (!copy) {
        return -1;
    }
    char *cursor = copy;
    for (int i = 0; i < 4; ++i) {
        char *part = strsep(&cursor, ",");
        if (!part) {
            free(copy);
            return -1;
        }
        output[i] = atoi(part);
    }
    free(copy);
    return 0;
}

static void theme_parse_color(const char *value, unsigned char rgb[3])
{
    unsigned long color = value ? strtoul(value, NULL, 16) : 0;
    rgb[0] = (unsigned char)(color & 255);
    rgb[1] = (unsigned char)((color >> 8) & 255);
    rgb[2] = (unsigned char)((color >> 16) & 255);
}

static void theme_format_color(char *out, size_t size, const unsigned char rgb[3])
{
    snprintf(out, size, "#%02x%02x%02x", rgb[0], rgb[1], rgb[2]);
}

int fcitx5_theme_write(const char *skin_dir)
{
    if (!skin_dir || !*skin_dir) {
        return -1;
    }
    char config_path[4096], path[4096], image_name[512];
    snprintf(config_path, sizeof(config_path), "%s/skin.ini", skin_dir);
    FILE *probe = fopen(config_path, "rb");
    if (!probe) {
        snprintf(config_path, sizeof(config_path), "%s/Skin.ini", skin_dir);
    } else {
        fclose(probe);
    }
    SkinConfig *config = skin_config_load(config_path);
    if (!config) {
        fprintf(stderr, "%s\n", localized_message("failed to read Skin.ini", "读取 Skin.ini 失败"));
        return -1;
    }
    const char *name = skin_config_get(config, "General", "skin_name");
    const char *version = skin_config_get(config, "General", "skin_version");
    const char *author = skin_config_get(config, "General", "skin_author");
    const char *description = skin_config_get(config, "General", "skin_info");
    if (!name || !skin_config_get(config, "Display", "font_size")) {
        skin_config_free(config);
        return -1;
    }
    unsigned char input_rgb[3], first_rgb[3], normal_rgb[3], background_rgb[3] = {0, 0, 0};
    theme_parse_color(skin_config_get(config, "Display", "pinyin_color"), input_rgb);
    theme_parse_color(skin_config_get(config, "Display", "zhongwen_first_color"), first_rgb);
    theme_parse_color(skin_config_get(config, "Display", "zhongwen_color"), normal_rgb);
    const char *background = skin_config_get(config, "Scheme_H1", "pic");
    if (background && theme_first_image(background, image_name, sizeof(image_name))) {
        path_join(path, sizeof(path), skin_dir, image_name);
        int layout[3] = {0, 0, 0};
        const char *layout_value = skin_config_get(config, "Scheme_H1", "layout_horizontal");
        if (layout_value) {
            sscanf(layout_value, "%d,%d,%d", &layout[0], &layout[1], &layout[2]);
        }
        if (skin_image_average(path,
                               layout[1],
                               -(layout[2]),
                               0,
                               0,
                               background_rgb) != 0) {
            background_rgb[0] = 0;
            background_rgb[1] = 0;
            background_rgb[2] = 0;
        }
    }
    char input_color[16], first_color[16], normal_color[16], background_color[16];
    theme_format_color(input_color, sizeof(input_color), input_rgb);
    theme_format_color(first_color, sizeof(first_color), first_rgb);
    theme_format_color(normal_color, sizeof(normal_color), normal_rgb);
    theme_format_color(background_color, sizeof(background_color), background_rgb);
    int font_size = skin_config_get_int(config, "Display", "font_size", 14);
    int pinyin[4] = {0}, candidate[4] = {0};
    if (skin_config_get_int_list4(config, "Scheme_H1", "pinyin_marge", pinyin) != 0 ||
        skin_config_get_int_list4(config, "Scheme_H1", "zhongwen_marge", candidate) != 0) {
        skin_config_free(config);
        return -1;
    }
    int text_bottom = (pinyin[1] + candidate[0]) / 2;
    int text_top = pinyin[1] + candidate[0] - text_bottom;
    int content_top = pinyin[0] - text_top;
    SkinImageSize background_size = {0, 0};
    if (background) {
        theme_first_image(background, image_name, sizeof(image_name));
        path_join(path, sizeof(path), skin_dir, image_name);
        skin_image_size(path, &background_size);
    }
    int content_bottom = (int)background_size.height - content_top - text_top -
                         font_size - text_bottom - text_top - font_size -
                         text_bottom;
    if (content_bottom < 0) {
        content_bottom = 0;
    }
    path_join(path, sizeof(path), skin_dir, "highlight.png");
    skin_image_write_transparent_png(path, 38, 23);
    path_join(path, sizeof(path), skin_dir, "menu_highlight.png");
    skin_image_write_transparent_png(path, 38, 23);
    path_join(path, sizeof(path), skin_dir, "next.png");
    skin_image_write_page_arrow_png(path, 16, 20, normal_rgb, 1);
    path_join(path, sizeof(path), skin_dir, "prev.png");
    skin_image_write_page_arrow_png(path, 16, 20, normal_rgb, -1);
    path_join(path, sizeof(path), skin_dir, "radio.png");
    skin_image_write_solid_png(path, 24, 24, normal_rgb);
    path_join(path, sizeof(path), skin_dir, "arrow.png");
    skin_image_write_solid_png(path, 6, 12, normal_rgb);
    path_join(path, sizeof(path), skin_dir, "theme.conf");
    FILE *out = fopen(path, "w");
    if (!out) {
        skin_config_free(config);
        return -1;
    }
    fprintf(out, "[Metadata]\nName=%s\nVersion=%s\nAuthor=%s\nDescription=%s\nScaleWithDPI=False\n\n", name, version ? version : "1.0", author ? author : "", description ? description : "");
    fprintf(out, "[InputPanel]\nFont=Sans %d\nNormalColor=%s\nHighlightCandidateColor=%s\nHighlightColor=%s\nHighlightBackgroundColor=%s\nSpacing=3\n\n", font_size, normal_color, first_color, input_color, background_color);
    fprintf(out, "[InputPanel/TextMargin]\nLeft=5\nRight=5\nTop=%d\nBottom=%d\n\n", text_top, text_bottom);
    fprintf(out, "[InputPanel/ContentMargin]\nLeft=%d\nRight=%d\nTop=%d\nBottom=%d\n\n", (pinyin[2] > candidate[2] ? pinyin[2] : candidate[2]) - 5, (pinyin[3] > candidate[3] ? pinyin[3] : candidate[3]) - 5, content_top, content_bottom);
    fprintf(out, "[InputPanel/Background]\nImage=%s\n\n", background ? image_name : "");
    int h[3] = {0};
    int v[3] = {0};
    const char *hv = skin_config_get(config, "Scheme_H1", "layout_horizontal");
    const char *vv = skin_config_get(config, "Scheme_H1", "layout_vertical");
    if (hv) {
        sscanf(hv, "%d,%d,%d", &h[0], &h[1], &h[2]);
    }
    if (vv) {
        sscanf(vv, "%d,%d,%d", &v[0], &v[1], &v[2]);
    }
    fprintf(out, "[InputPanel/Background/Margin]\nLeft=%d\nRight=%d\nTop=%d\nBottom=%d\n\n", h[1], h[2], v[1], v[2]);
    fprintf(out, "[InputPanel/Highlight]\nImage=highlight.png\n\n[InputPanel/Highlight/Margin]\nLeft=5\nRight=5\nTop=5\nBottom=5\n\n[InputPanel/PrevPage]\nImage=prev.png\n\n[InputPanel/PrevPage/ClickMargin]\nLeft=5\nRight=5\nTop=4\nBottom=4\n\n[InputPanel/NextPage]\nImage=next.png\n\n[InputPanel/NextPage/ClickMargin]\nLeft=5\nRight=5\nTop=4\nBottom=4\n\n");
    const char *vertical = skin_config_get(config, "Scheme_V1", "pic");
    if (vertical) {
        theme_first_image(vertical, image_name, sizeof(image_name));
        int vh[3] = {0};
        int vv2[3] = {0};
        const char *vhs = skin_config_get(config, "Scheme_V1", "layout_horizontal");
        const char *vvs = skin_config_get(config, "Scheme_V1", "layout_vertical");
        if (vhs) {
            sscanf(vhs, "%d,%d,%d", &vh[0], &vh[1], &vh[2]);
        }
        if (vvs) {
            sscanf(vvs, "%d,%d,%d", &vv2[0], &vv2[1], &vv2[2]);
        }
        fprintf(out, "[Menu/Background]\nImage=%s\n\n[Menu/Background/Margin]\nLeft=%d\nRight=%d\nTop=%d\nBottom=%d\n\n[Menu/ContentMargin]\nLeft=%d\nRight=%d\nTop=%d\nBottom=%d\n\n", image_name, vh[1], vh[2], vv2[1], vv2[2], candidate[2], candidate[2], pinyin[0] + pinyin[1] + candidate[0], candidate[1]);
    }
    fprintf(out, "[Menu/Highlight]\nImage=menu_highlight.png\n\n[Menu/Highlight/Margin]\nLeft=10\nRight=10\nTop=5\nBottom=5\n\n[Menu/Separator]\nColor=%s\n\n[Menu/CheckBox]\nImage=radio.png\n\n[Menu/SubMenu]\nImage=arrow.png\n\n[Menu/TextMargin]\nLeft=5\nRight=5\nTop=5\nBottom=5\n", normal_color);
    fclose(out);
    skin_config_free(config);
    return 0;
}
