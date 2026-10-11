#define _GNU_SOURCE
#include "skin_config.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *trim(char *text)
{
    while (isspace((unsigned char)*text)) ++text;
    char *end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) --end;
    *end = '\0';
    return text;
}

static char *dup_string(const char *text)
{
    size_t n = strlen(text) + 1;
    char *copy = malloc(n);
    if (copy) {
        memcpy(copy, text, n);
    }
    return copy;
}

static int append_utf8(char **out, size_t *length, size_t *capacity, uint32_t c)
{
    unsigned char bytes[4];
    size_t count;
    if (c < 0x80) {
        bytes[0] = (unsigned char)c;
        count = 1;
    } else if (c < 0x800) {
        bytes[0] = (unsigned char)(0xc0 | (c >> 6));
        bytes[1] = (unsigned char)(0x80 | (c & 63));
        count = 2;
    } else if (c < 0x10000) {
        bytes[0] = (unsigned char)(0xe0 | (c >> 12));
        bytes[1] = (unsigned char)(0x80 | ((c >> 6) & 63));
        bytes[2] = (unsigned char)(0x80 | (c & 63));
        count = 3;
    } else {
        bytes[0] = (unsigned char)(0xf0 | (c >> 18));
        bytes[1] = (unsigned char)(0x80 | ((c >> 12) & 63));
        bytes[2] = (unsigned char)(0x80 | ((c >> 6) & 63));
        bytes[3] = (unsigned char)(0x80 | (c & 63));
        count = 4;
    }
    if (*length + count + 1 > *capacity) {
        while (*length + count + 1 > *capacity) {
            *capacity *= 2;
        }
        char *grown = realloc(*out, *capacity);
        if (!grown) {
            return -1;
        }
        *out = grown;
    }
    memcpy(*out + *length, bytes, count);
    *length += count;
    (*out)[*length] = '\0';
    return 0;
}

static char *decode_text(const unsigned char *data, size_t size)
{
    char *out = malloc(size * 2 + 1);
    size_t length = 0;
    size_t capacity = size * 2 + 1;
    if (!out) {
        return NULL;
    }
    out[0] = '\0';
    size_t start = 0;
    if (size >= 3 && data[0] == 0xef && data[1] == 0xbb && data[2] == 0xbf) {
        start = 3;
    }
    if (size >= 2 && data[0] == 0xff && data[1] == 0xfe) {
        start = 2;
        for (size_t i = start; i + 1 < size; i += 2) {
            uint32_t c = (uint32_t)data[i] | ((uint32_t)data[i + 1] << 8);
            if (c == '\r') {
                continue;
            }
            if (append_utf8(&out, &length, &capacity, c) != 0) {
                free(out);
                return NULL;
            }
        }
    } else if (size >= 2 && data[0] == 0xfe && data[1] == 0xff) {
        start = 2;
        for (size_t i = start; i + 1 < size; i += 2) {
            uint32_t c = ((uint32_t)data[i] << 8) | data[i + 1];
            if (c == '\r') {
                continue;
            }
            if (append_utf8(&out, &length, &capacity, c) != 0) {
                free(out);
                return NULL;
            }
        }
    } else {
        if (length + size + 1 > capacity) {
            char *grown = realloc(out, length + size + 1);
            if (!grown) {
                free(out);
                return NULL;
            }
            out = grown;
        }
        memcpy(out + length, data + start, size - start);
        out[length + size - start] = '\0';
    }
    return out;
}

static Section *find_section(SkinConfig *config, const char *name, int create)
{
    for (Section *s = config->sections; s; s = s->next) {
        if (strcmp(s->name, name) == 0) {
            return s;
        }
    }
    if (!create) {
        return NULL;
    }
    Section *section = calloc(1, sizeof(*section));
    if (!section || !(section->name = dup_string(name))) {
        free(section);
        return NULL;
    }
    section->next = config->sections;
    config->sections = section;
    return section;
}

SkinConfig *skin_config_load(const char *path)
{
    if (!path || !*path) {
        return NULL;
    }
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    long size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    unsigned char *data = malloc((size_t)size + 1);
    if (!data || fread(data, 1, (size_t)size, file) != (size_t)size) {
        free(data);
        fclose(file);
        return NULL;
    }
    fclose(file);
    char *text = decode_text(data, (size_t)size);
    free(data);
    if (!text) {
        return NULL;
    }
    SkinConfig *config = calloc(1, sizeof(*config));
    Section *section = NULL;
    char *line = text;
    while (line && *line) {
        char *next = strchr(line, '\n');
        if (next) {
            *next++ = '\0';
        }
        char *value = trim(line);
        if (*value && *value != ';' && *value != '#') {
            size_t n = strlen(value);
            if (value[0] == '[' && n > 2 && value[n - 1] == ']') {
                value[n - 1] = '\0';
                section = find_section(config, trim(value + 1), 1);
            }
            else if (section) {
                char *equal = strchr(value, '=');
                if (equal) {
                    *equal = '\0';
                    char *key = trim(value);
                    char *val = trim(equal + 1);
                    Entry *entry = calloc(1, sizeof(*entry));
                    if (entry && (entry->key = dup_string(key)) &&
                        (entry->value = dup_string(val))) {
                        entry->next = section->entries;
                        section->entries = entry;
                    } else {
                        free(entry);
                    }
                }
            }
        }
        line = next;
    }
    free(text);
    return config;
}

const char *skin_config_get(const SkinConfig *config, const char *section_name, const char *key)
{
    if (!config || !section_name || !*section_name || !key || !*key) {
        return NULL;
    }
    for (Section *s = config->sections; s; s = s->next) {
        if (strcmp(s->name, section_name) == 0) {
            for (Entry *e = s->entries; e; e = e->next) {
                if (strcmp(e->key, key) == 0) {
                    return e->value;
                }
            }
        }
    }
    return NULL;
}

void skin_config_free(SkinConfig *config)
{
    if (!config) {
        return;
    }
    Section *s = config->sections;
    while (s) {
        Section *next = s->next;
        Entry *e = s->entries;
        while (e) {
            Entry *en = e->next;
            free(e->key);
            free(e->value);
            free(e);
            e = en;
        }
        free(s->name);
        free(s);
        s = next;
    }
    free(config);
}
