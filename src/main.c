#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include "ssf_archive.h"
#include "i18n.h"
#include "fcitx5_theme.h"
#include "fcitx5_config.h"

#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

static char *absolute_path(const char *path);

static void usage(const char *program)
{
    fprintf(stderr, "%s\n", localized_message("Usage:", "用法："));
    fprintf(stderr, "  %s [options] <input.ssf | directory>\n\n", program);
    fprintf(stderr, "%s\n", localized_message("Options:", "选项："));
    fprintf(stderr, "  -o, --output DIR  %s\n",
            localized_message("set the output directory", "指定输出目录"));
    fprintf(stderr, "  -u, --unpack      %s\n",
            localized_message("only unpack the SSF file", "仅解包 SSF 文件"));
    fprintf(stderr, "  -f, --force       %s\n",
            localized_message("overwrite an existing output", "覆盖已有输出"));
    fprintf(stderr, "  -v, --version     %s\n",
            localized_message("show version information", "显示版本信息"));
    fprintf(stderr, "  -h, --help        %s\n",
            localized_message("show this help", "显示帮助"));
}

static int is_directory(const char *path)
{
    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

static int path_exists(const char *path)
{
    struct stat info;
    return lstat(path, &info) == 0;
}

static int is_regular_file(const char *path)
{
    struct stat info;
    return stat(path, &info) == 0 && S_ISREG(info.st_mode);
}

static int paths_equal(const char *first, const char *second)
{
    char *resolved_first = realpath(first, NULL);
    char *resolved_second = realpath(second, NULL);
    int equal;
    if (resolved_first && resolved_second) {
        equal = strcmp(resolved_first, resolved_second) == 0;
    } else {
        equal = strcmp(first, second) == 0;
    }
    free(resolved_first);
    free(resolved_second);
    return equal;
}

static int path_is_inside(const char *parent, const char *child)
{
    char *absolute_parent = absolute_path(parent);
    char *absolute_child = absolute_path(child);
    size_t parent_length;
    int inside = 0;
    if (absolute_parent && absolute_child) {
        parent_length = strlen(absolute_parent);
        if (strncmp(absolute_child, absolute_parent, parent_length) == 0 &&
            absolute_child[parent_length] == '/') {
            inside = 1;
        }
    }
    free(absolute_parent);
    free(absolute_child);
    return inside;
}

static char *join_path(const char *directory, const char *name)
{
    size_t length = strlen(directory) + strlen(name) + 2;
    char *path = malloc(length);
    if (!path) {
        return NULL;
    }
    snprintf(path, length, "%s/%s", directory, name);
    return path;
}

static const char *base_name(const char *path);

static int mkdir_p(const char *path)
{
    char *copy = strdup(path);
    if (!copy) {
        return -1;
    }
    for (char *cursor = copy + 1; *cursor; ++cursor) {
        if (*cursor == '/') {
            *cursor = '\0';
            if (mkdir(copy, 0755) != 0 && errno != EEXIST) {
                free(copy);
                return -1;
            }
            *cursor = '/';
        }
    }
    if (mkdir(copy, 0755) != 0 && errno != EEXIST) {
        free(copy);
        return -1;
    }
    free(copy);
    return 0;
}

static int has_skin_config(const char *directory)
{
    char *lowercase_path = join_path(directory, "skin.ini");
    char *uppercase_path = join_path(directory, "Skin.ini");
    int found = 0;
    if (lowercase_path && is_regular_file(lowercase_path)) {
        found = 1;
    }
    if (uppercase_path && is_regular_file(uppercase_path)) {
        found = 1;
    }
    free(lowercase_path);
    free(uppercase_path);
    return found;
}

static int confirm_overwrite(const char *path)
{
    char answer[16] = {0};
    fprintf(stderr, "%s: %s [y/N] ",
            localized_message("output already exists, overwrite",
                              "输出路径已存在，是否覆盖"),
            path);
    fflush(stderr);
    if (!fgets(answer, sizeof(answer), stdin)) {
        return 0;
    }
    return answer[0] == 'y' || answer[0] == 'Y';
}

static int remove_tree(const char *path)
{
    DIR *directory = opendir(path);
    if (!directory) {
        return errno == ENOENT ? 0 : -1;
    }
    struct dirent *entry;
    int result = 0;
    while ((entry = readdir(directory)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        size_t length = strlen(path) + strlen(entry->d_name) + 2;
        char *child = malloc(length);
        if (!child) {
            result = -1;
            break;
        }
        snprintf(child, length, "%s/%s", path, entry->d_name);
        struct stat info;
        if (lstat(child, &info) != 0) {
            result = -1;
        } else if (S_ISDIR(info.st_mode)) {
            if (remove_tree(child) != 0) {
                result = -1;
            }
        } else if (unlink(child) != 0) {
            result = -1;
        }
        free(child);
        if (result != 0) {
            break;
        }
    }
    closedir(directory);
    if (result == 0 && rmdir(path) != 0) {
        result = -1;
    }
    return result;
}

static int remove_path(const char *path)
{
    struct stat info;
    if (lstat(path, &info) != 0) {
        return errno == ENOENT ? 0 : -1;
    }
    if (S_ISDIR(info.st_mode)) {
        return remove_tree(path);
    }
    return unlink(path);
}

static int copy_file(const char *source, const char *destination)
{
    FILE *input = fopen(source, "rb");
    if (!input) {
        return -1;
    }
    FILE *output = fopen(destination, "wb");
    if (!output) {
        fclose(input);
        return -1;
    }
    unsigned char buffer[65536];
    size_t size;
    int result = 0;
    while ((size = fread(buffer, 1, sizeof(buffer), input)) > 0) {
        if (fwrite(buffer, 1, size, output) != size) {
            result = -1;
            break;
        }
    }
    if (ferror(input)) {
        result = -1;
    }
    if (fclose(output) != 0) {
        result = -1;
    }
    fclose(input);
    return result;
}

static int copy_tree(const char *source, const char *destination)
{
    if (mkdir_p(destination) != 0) {
        return -1;
    }
    DIR *directory = opendir(source);
    if (!directory) {
        return -1;
    }
    struct dirent *entry;
    int result = 0;
    while ((entry = readdir(directory)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char *source_path = join_path(source, entry->d_name);
        char *destination_path = join_path(destination, entry->d_name);
        struct stat info;
        if (!source_path || !destination_path || lstat(source_path, &info) != 0) {
            result = -1;
        } else if (S_ISDIR(info.st_mode)) {
            result = copy_tree(source_path, destination_path);
        } else if (S_ISREG(info.st_mode)) {
            result = copy_file(source_path, destination_path);
        } else {
            result = -1;
        }
        free(source_path);
        free(destination_path);
        if (result != 0) {
            break;
        }
    }
    closedir(directory);
    return result;
}

static char *default_output(const char *input)
{
    char *copy = strdup(input);
    if (!copy) {
        return NULL;
    }
    char *slash = strrchr(copy, '/');
    char *name = slash ? slash + 1 : copy;
    char *dot = strrchr(name, '.');
    if (dot && strcasecmp(dot, ".ssf") == 0) {
        *dot = '\0';
    }
    return copy;
}

static char *default_theme_output(const char *input)
{
    const char *data_home = getenv("XDG_DATA_HOME");
    const char *home = getenv("HOME");
    char *theme_name = default_output(input);
    char *base = NULL;
    char *fcitx_directory = NULL;
    char *themes_directory = NULL;
    char *output = NULL;
    if (!theme_name || (!data_home && (!home || !*home))) {
        free(theme_name);
        return NULL;
    }
    if (data_home && *data_home) {
        base = strdup(data_home);
    } else {
        base = join_path(home, ".local/share");
    }
    fcitx_directory = base ? join_path(base, "fcitx5") : NULL;
    themes_directory = fcitx_directory ? join_path(fcitx_directory, "themes") : NULL;
    output = themes_directory ? join_path(themes_directory, base_name(theme_name)) : NULL;
    free(theme_name);
    free(base);
    free(fcitx_directory);
    free(themes_directory);
    return output;
}

static char *absolute_path(const char *path)
{
    if (!path || !*path) {
        return NULL;
    }
    if (path[0] == '/') {
        return strdup(path);
    }
    char *working_directory = getcwd(NULL, 0);
    if (!working_directory) {
        return strdup(path);
    }
    size_t length = strlen(working_directory) + strlen(path) + 2;
    char *absolute = malloc(length);
    if (absolute) {
        snprintf(absolute, length, "%s/%s", working_directory, path);
    }
    free(working_directory);
    return absolute;
}

static const char *base_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static int has_ssf_extension(const char *path)
{
    const char *name = base_name(path);
    const char *dot = strrchr(name, '.');
    return dot && strcasecmp(dot, ".ssf") == 0;
}

static int confirm_update_theme(const char *theme_name)
{
    char answer[16] = {0};
    fprintf(stderr, "%s: %s [y/N] ",
            localized_message("update Fcitx5 theme to", "是否将 Fcitx5 主题更新为"),
            theme_name);
    fflush(stderr);
    if (!fgets(answer, sizeof(answer), stdin)) {
        return 0;
    }
    return answer[0] == 'y' || answer[0] == 'Y';
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        usage(argv[0]);
        return 2;
    }
    int unpack_only = 0;
    int force = 0;
    const char *input = NULL;
    const char *output = NULL;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            printf("ssfconv %s\n", SSFCONV_VERSION);
            return 0;
        }
        if (strcmp(argv[i], "--unpack") == 0 || strcmp(argv[i], "-u") == 0) {
            unpack_only = 1;
            continue;
        }
        if (strcmp(argv[i], "--force") == 0 || strcmp(argv[i], "-f") == 0) {
            force = 1;
            continue;
        }
        if (strcmp(argv[i], "--output") == 0 || strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "%s\n",
                        localized_message("missing output directory",
                                          "缺少输出目录"));
                return 2;
            }
            ++i;
            output = argv[i];
            continue;
        }
        if (argv[i][0] == '-') {
            fprintf(stderr, "%s: %s\n", localized_message("unknown option", "未知选项"), argv[i]);
            return 2;
        }
        if (!input) {
            input = argv[i];
        } else {
            fprintf(stderr, "%s: %s\n", localized_message("unexpected argument", "意外参数"), argv[i]);
            return 2;
        }
    }
    if (!input) {
        usage(argv[0]);
        return 2;
    }
    int input_is_directory = is_directory(input);
    if (!input_is_directory && !is_regular_file(input)) {
        fprintf(stderr, "%s: %s\n",
                localized_message("input does not exist", "输入路径不存在"),
                input);
        return 1;
    }
    if (!input_is_directory && !has_ssf_extension(input)) {
        fprintf(stderr, "%s: %s\n",
                localized_message("input is not an SSF file", "输入文件不是 SSF 文件"),
                input);
        return 1;
    }
    if (input_is_directory && unpack_only) {
        fprintf(stderr, "%s\n",
                localized_message("--unpack can only be used with an SSF file",
                                  "--unpack 只能用于 SSF 文件"));
        return 2;
    }
    if (input_is_directory && !has_skin_config(input)) {
        fprintf(stderr, "%s: %s\n",
                localized_message("Skin.ini was not found in the input directory",
                                  "输入目录中未找到 Skin.ini"),
                input);
        return 1;
    }
    char *allocated_output = NULL;
    if (!output) {
        if (unpack_only) {
            if (!(allocated_output = default_output(input))) {
                return 1;
            }
            output = allocated_output;
        } else if (!(allocated_output = default_theme_output(input))) {
            return 1;
        } else {
            output = allocated_output;
        }
    }
    if (input_is_directory && output == input) {
        output = input;
    } else if (!output) {
        if (input_is_directory) {
            output = input;
        } else if (!(allocated_output = default_output(input))) {
            return 1;
        } else {
            output = allocated_output;
        }
    }
    int same_directory = input_is_directory && paths_equal(input, output);
    if (input_is_directory && !same_directory && path_is_inside(input, output)) {
        fprintf(stderr, "%s\n",
                localized_message("output directory cannot be inside the input directory",
                                  "输出目录不能位于输入目录内"));
        free(allocated_output);
        return 1;
    }
    int output_existed = path_exists(output);
    char *display_output = absolute_path(output);
    const char *display_path = display_output ? display_output : output;
    int overwrite_existing = force;
    if (output_existed && !force) {
        if (!confirm_overwrite(display_path)) {
            fprintf(stderr, "%s\n",
                    localized_message("operation cancelled",
                                      "操作已取消"));
            free(display_output);
            free(allocated_output);
            return 1;
        }
        overwrite_existing = 1;
    }
    if (!input_is_directory && paths_equal(input, output)) {
        fprintf(stderr, "%s\n",
                localized_message("input and output paths must be different",
                                  "输入路径和输出路径不能相同"));
        free(display_output);
        free(allocated_output);
        return 1;
    }
    if (!same_directory && output_existed && overwrite_existing && remove_path(output) != 0) {
        fprintf(stderr, "%s: %s\n",
                localized_message("failed to remove the existing output",
                                  "无法删除已有输出"),
                output);
        free(display_output);
        free(allocated_output);
        return 1;
    }
    int rc = 0;
    int update_theme = 0;
    if (input_is_directory) {
        if (!same_directory) {
            rc = copy_tree(input, output);
            if (rc != 0) {
                fprintf(stderr, "ssfconv: %s\n",
                        localized_message("failed to copy the unpacked skin",
                                          "复制已解包皮肤失败"));
            }
        }
        if (rc == 0) {
            rc = fcitx5_theme_write(output);
        }
        if (rc != 0) {
            fprintf(stderr, "ssfconv: %s\n",
                    localized_message("theme generation failed",
                                      "主题生成失败"));
        }
    } else {
        rc = ssf_archive_extract(input, output);
        if (rc != 0) {
            fprintf(stderr, "ssfconv: %s\n",
                    localized_message("SSF extraction failed",
                                      "SSF 解包失败"));
        }
        if (rc == 0 && !unpack_only) {
            rc = fcitx5_theme_write(output);
            if (rc != 0) {
                fprintf(stderr, "ssfconv: %s\n",
                        localized_message("theme generation failed",
                                          "主题生成失败"));
            }
        }
    }
    if (rc != 0) {
        if (!same_directory) {
            remove_tree(output);
        }
        fprintf(stderr, "ssfconv: %s\n", localized_message("operation failed", "操作失败"));
    } else if (unpack_only) {
        printf("%s %s -> %s\n%s%s\n",
               localized_message("Unpack succeeded:", "解包成功:"),
               base_name(input),
               base_name(output),
               localized_message("PATH: ", "PATH："),
               display_path);
    } else {
        const char *theme_name = base_name(output);
        printf("%s %s -> %s\n%s%s\n",
               localized_message("Conversion succeeded:", "转化成功:"),
               base_name(input),
               base_name(output),
               localized_message("PATH: ", "PATH："),
               display_path);
        fflush(stdout);
        if (confirm_update_theme(theme_name)) {
            if (fcitx5_config_set_theme(theme_name) != 0) {
                fprintf(stderr, "ssfconv: %s\n",
                        localized_message("failed to update Fcitx5 theme configuration",
                                          "更新 Fcitx5 主题配置失败"));
                update_theme = 1;
            } else {
                fprintf(stderr, "%s\n",
                        localized_message("Fcitx5 theme configuration updated",
                                          "Fcitx5 主题配置已更新"));
                if (fcitx5_config_restart() == 0) {
                    fprintf(stderr, "%s\n",
                            localized_message("Fcitx5 restarted",
                                              "Fcitx5 已重启"));
                } else {
                    fprintf(stderr, "%s\n",
                            localized_message("Fcitx5 could not be restarted; restart it manually",
                                              "无法重启 Fcitx5，请手动重启"));
                }
            }
        }
    }
    free(display_output);
    free(allocated_output);
    return rc == 0 && update_theme == 0 ? 0 : 1;
}
