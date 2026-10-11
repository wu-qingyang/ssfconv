#define _POSIX_C_SOURCE 200809L
#include "ssf_archive.h"

#include <errno.h>
#include <fcntl.h>
#include <openssl/evp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <zip.h>
#include <zlib.h>

static const unsigned char aes_key[32] = {
    0x52, 0x36, 0x46, 0x1a, 0xd3, 0x85, 0x03, 0x66,
    0x90, 0x45, 0x16, 0x28, 0x79, 0x03, 0x36, 0x23,
    0xdd, 0xbe, 0x6f, 0x03, 0xff, 0x04, 0xe3, 0xca,
    0xd5, 0x7f, 0xfc, 0xa3, 0x50, 0xe4, 0x9e, 0xd9};
static const unsigned char aes_iv[16] = {
    0xe0, 0x7a, 0xad, 0x35, 0xe0, 0x90, 0xaa, 0x03,
    0x8a, 0x51, 0xfd, 0x05, 0xdf, 0x8c, 0x5d, 0x0f};

static uint32_t u32le(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int mkdir_p(const char *path)
{
    char *copy = strdup(path);
    if (!copy) {
        return -1;
    }
    for (char *p = copy + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(copy, 0755) != 0 && errno != EEXIST) {
                free(copy);
                return -1;
            }
            *p = '/';
        }
    }
    if (mkdir(copy, 0755) != 0 && errno != EEXIST) {
        free(copy);
        return -1;
    }
    free(copy);
    return 0;
}

static int safe_join(const char *root, const char *name, char **result)
{
    if (!name || name[0] == '/' || strstr(name, "../") || strcmp(name, "..") == 0) {
        return -1;
    }
    size_t n = strlen(root) + strlen(name) + 2;
    char *path = malloc(n);
    if (!path) {
        return -1;
    }
    snprintf(path, n, "%s/%s", root, name);
    *result = path;
    return 0;
}

static int write_file(const char *path, const unsigned char *data, size_t size)
{
    FILE *file = fopen(path, "wb");
    if (!file) {
        return -1;
    }
    int ok = fwrite(data, 1, size, file) == size;
    if (fclose(file) != 0) {
        ok = 0;
    }
    return ok ? 0 : -1;
}

static int extract_zip(const char *input, const char *output)
{
    int error = 0;
    zip_t *archive = zip_open(input, ZIP_RDONLY, &error);
    if (!archive) {
        return -1;
    }
    if (mkdir_p(output) != 0) {
        zip_close(archive);
        return -1;
    }
    zip_int64_t count = zip_get_num_entries(archive, 0);
    for (zip_uint64_t i = 0; i < (zip_uint64_t)count; ++i) {
        const char *name = zip_get_name(archive, i, 0);
        if (!name) {
            zip_close(archive);
            return -1;
        }
        char *path = NULL;
        if (safe_join(output, name, &path) != 0) {
            zip_close(archive);
            return -1;
        }
        size_t name_len = strlen(name);
        if (name_len && name[name_len - 1] == '/') {
            int rc = mkdir_p(path);
            free(path);
            if (rc != 0) {
                zip_close(archive);
                return -1;
            }
            continue;
        }
        char *slash = strrchr(path, '/');
        if (slash) {
            *slash = '\0';
            if (mkdir_p(path) != 0) {
                free(path);
                zip_close(archive);
                return -1;
            }
            *slash = '/';
        }
        zip_file_t *entry = zip_fopen_index(archive, i, 0);
        if (!entry) {
            free(path);
            zip_close(archive);
            return -1;
        }
        FILE *file = fopen(path, "wb");
        if (!file) {
            zip_fclose(entry);
            free(path);
            zip_close(archive);
            return -1;
        }
        unsigned char buffer[65536];
        zip_int64_t got;
        int failed = 0;
        while ((got = zip_fread(entry, buffer, sizeof(buffer))) > 0) {
            if (fwrite(buffer, 1, (size_t)got, file) != (size_t)got) {
                failed = 1;
                break;
            }
        }
        if (got < 0) {
            failed = 1;
        }
        if (fclose(file) != 0) {
            failed = 1;
        }
        zip_fclose(entry);
        free(path);
        if (failed) {
            zip_close(archive);
            return -1;
        }
    }
    return zip_close(archive) == 0 ? 0 : -1;
}

static int decrypt_payload(const unsigned char *input, size_t input_size,
                           unsigned char **output, size_t *output_size)
{
    if (input_size < 8 || (input_size - 8) % 16 != 0) {
        return -1;
    }
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return -1;
    }
    int len = 0, final_len = 0;
    size_t payload_size = input_size - 8;
    unsigned char *buffer = malloc(payload_size + 16);
    if (!buffer || EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, aes_key, aes_iv) != 1 ||
        EVP_DecryptUpdate(ctx, buffer, &len, input + 8, (int)payload_size) != 1 ||
        EVP_DecryptFinal_ex(ctx, buffer + len, &final_len) != 1) {
        free(buffer);
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    EVP_CIPHER_CTX_free(ctx);
    *output = buffer;
    *output_size = (size_t)(len + final_len);
    return 0;
}

static int inflate_payload(const unsigned char *input, size_t input_size,
                           unsigned char **output, size_t *output_size)
{
    size_t capacity = input_size * 4 + 4096;
    for (;;) {
        unsigned char *buffer = realloc(*output, capacity);
        if (!buffer) {
            return -1;
        }
        *output = buffer;
        uLongf size = (uLongf)capacity;
        int rc = uncompress(buffer, &size, input, (uLong)input_size);
        if (rc == Z_OK) {
            *output_size = (size_t)size;
            return 0;
        }
        if (rc != Z_BUF_ERROR || capacity > 1024 * 1024 * 1024) {
            return -1;
        }
        capacity *= 2;
    }
}

static int extract_encrypted(const char *input, const char *output)
{
    FILE *file = fopen(input, "rb");
    if (!file) {
        return -1;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return -1;
    }
    long length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return -1;
    }
    unsigned char *source = malloc((size_t)length);
    if (!source || fread(source, 1, (size_t)length, file) != (size_t)length) {
        free(source);
        fclose(file);
        return -1;
    }
    fclose(file);
    unsigned char *decrypted = NULL, *raw = NULL;
    size_t decrypted_size = 0, raw_size = 0;
    int rc = decrypt_payload(source, (size_t)length, &decrypted, &decrypted_size);
    free(source);
    if (rc != 0 || decrypted_size < 4 ||
        inflate_payload(decrypted + 4, decrypted_size - 4, &raw, &raw_size) != 0) {
        free(decrypted);
        free(raw);
        return -1;
    }
    free(decrypted);
    if (raw_size < 8) {
        free(raw);
        return -1;
    }
    uint32_t offsets_size = u32le(raw + 4);
    if (offsets_size > raw_size - 8 || offsets_size % 4 != 0) {
        free(raw);
        return -1;
    }
    if (mkdir_p(output) != 0) {
        free(raw);
        return -1;
    }
    for (uint32_t pos = 0; pos < offsets_size; pos += 4) {
        uint32_t offset = u32le(raw + 8 + pos);
        if (offset > raw_size - 4) {
            free(raw);
            return -1;
        }
        uint32_t name_len = u32le(raw + offset);
        if (name_len > raw_size - offset - 4 || name_len % 2 != 0) {
            free(raw);
            return -1;
        }
        size_t name_chars = name_len / 2;
        char *name = malloc(name_chars * 3 + 1);
        if (!name) {
            free(raw);
            return -1;
        }
        size_t out = 0;
        for (size_t i = 0; i < name_chars; ++i) {
            uint16_t c = (uint16_t)raw[offset + 4 + i * 2] |
                         ((uint16_t)raw[offset + 5 + i * 2] << 8);
            if (c < 0x80) {
                name[out++] = (char)c;
            } else if (c < 0x800) {
                name[out++] = (char)(0xc0 | (c >> 6));
                name[out++] = (char)(0x80 | (c & 63));
            } else {
                name[out++] = (char)(0xe0 | (c >> 12));
                name[out++] = (char)(0x80 | ((c >> 6) & 63));
                name[out++] = (char)(0x80 | (c & 63));
            }
        }
        name[out] = '\0';
        size_t content_pos = offset + 4 + name_len;
        if (content_pos > raw_size - 4) {
            free(name);
            free(raw);
            return -1;
        }
        uint32_t content_len = u32le(raw + content_pos);
        content_pos += 4;
        char *path = NULL;
        if (content_len > raw_size - content_pos || safe_join(output, name, &path) != 0) {
            free(name);
            free(raw);
            return -1;
        }
        char *slash = strrchr(path, '/');
        if (slash) {
            *slash = '\0';
            if (mkdir_p(path) != 0) {
                free(path);
                free(name);
                free(raw);
                return -1;
            }
            *slash = '/';
        }
        rc = write_file(path, raw + content_pos, content_len);
        free(path);
        free(name);
        if (rc != 0) {
            free(raw);
            return -1;
        }
    }
    free(raw);
    return 0;
}

int ssf_archive_extract(const char *input_path, const char *output_dir)
{
    if (!input_path || !*input_path || !output_dir || !*output_dir) {
        return -1;
    }
    FILE *file = fopen(input_path, "rb");
    if (!file) {
        return -1;
    }
    unsigned char header[4] = {0};
    size_t got = fread(header, 1, sizeof(header), file);
    fclose(file);
    if (got != sizeof(header)) {
        return -1;
    }
    if (memcmp(header, "Skin", 4) == 0) {
        return extract_encrypted(input_path, output_dir);
    }
    return extract_zip(input_path, output_dir);
}

