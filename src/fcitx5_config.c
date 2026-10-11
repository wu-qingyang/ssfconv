#define _POSIX_C_SOURCE 200809L
#include "fcitx5_config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

static int replace_theme_line(char *line, const char *key, const char *theme_name)
{
    size_t key_length = strlen(key);
    if (strncmp(line, key, key_length) != 0 || line[key_length] != '=') {
        return 0;
    }
    (void)theme_name;
    return 1;
}

static int write_config(const char *config_path, const char *theme_name)
{
    FILE *input = fopen(config_path, "r");
    char *temporary_path = NULL;
    FILE *output;
    int theme_found = 0;
    int dark_theme_found = 0;
    if (!input) {
        if (errno != ENOENT) {
            return -1;
        }
    }
    size_t length = strlen(config_path) + 16;
    temporary_path = malloc(length);
    if (!temporary_path) {
        if (input) {
            fclose(input);
        }
        return -1;
    }
    snprintf(temporary_path, length, "%s.ssfconv.tmp", config_path);
    output = fopen(temporary_path, "w");
    if (!output) {
        free(temporary_path);
        if (input) {
            fclose(input);
        }
        return -1;
    }
    if (input) {
        char *line = NULL;
        size_t capacity = 0;
        ssize_t read;
        while ((read = getline(&line, &capacity, input)) >= 0) {
            int replaced = replace_theme_line(line, "Theme", theme_name);
            if (replaced < 0) {
                free(line);
                fclose(input);
                fclose(output);
                unlink(temporary_path);
                free(temporary_path);
                return -1;
            }
            if (replaced > 0) {
                fprintf(output, "Theme=%s\n", theme_name);
                theme_found = 1;
                continue;
            }
            replaced = replace_theme_line(line, "DarkTheme", theme_name);
            if (replaced < 0) {
                free(line);
                fclose(input);
                fclose(output);
                unlink(temporary_path);
                free(temporary_path);
                return -1;
            }
            if (replaced > 0) {
                fprintf(output, "DarkTheme=%s\n", theme_name);
                dark_theme_found = 1;
                continue;
            }
            fputs(line, output);
        }
        free(line);
        fclose(input);
    }
    if (!theme_found) {
        fprintf(output, "Theme=%s\n", theme_name);
    }
    if (!dark_theme_found) {
        fprintf(output, "DarkTheme=%s\n", theme_name);
    }
    if (fclose(output) != 0 || rename(temporary_path, config_path) != 0) {
        unlink(temporary_path);
        free(temporary_path);
        return -1;
    }
    free(temporary_path);
    return 0;
}

int fcitx5_config_set_theme(const char *theme_name)
{
    const char *config_home = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");
    char *base = NULL;
    char *fcitx_directory = NULL;
    char *conf_directory = NULL;
    char *config_path = NULL;
    int result;
    if (!theme_name || !*theme_name || (!config_home && (!home || !*home))) {
        return -1;
    }
    if (config_home && *config_home) {
        base = strdup(config_home);
    } else {
        base = join_path(home, ".config");
    }
    if (!base) {
        return -1;
    }
    fcitx_directory = join_path(base, "fcitx5");
    conf_directory = fcitx_directory ? join_path(fcitx_directory, "conf") : NULL;
    config_path = conf_directory ? join_path(conf_directory, "classicui.conf") : NULL;
    free(base);
    free(fcitx_directory);
    if (!conf_directory || !config_path || mkdir_p(conf_directory) != 0) {
        free(conf_directory);
        free(config_path);
        return -1;
    }
    result = write_config(config_path, theme_name);
    free(conf_directory);
    free(config_path);
    return result;
}

int fcitx5_config_restart(void)
{
    return system("fcitx5 -r -d > /dev/null 2>&1") == 0 ? 0 : -1;
}
