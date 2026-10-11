# ssfconv

[中文文档](README.md)

Native C11 converter for Sogou SSF input-method skins to Fcitx5 themes.

## Preview

This project began because I wanted to use the Kogoyoru skin from *Senren
Banka* on Linux. Credit and thanks to the skin creator bilibili uploader
[美有问题喵](https://space.bilibili.com/30532016), for creating and sharing it.

![Kogoyoru skin from Senren＊Banka](docs/congyu-skin-preview.png)

## Background

This project started after the upstream `ssfconv` failed to convert an SSF
skin. The issue was not that Fcitx5 targets Windows. SSF is a Sogou Windows
input-method skin format, and its filenames and text encoding reflect Windows
conventions that become visible on Linux:

- the configuration file is commonly named `Skin.ini`, while early code only
  looked for lowercase `skin.ini`. Windows filesystems are commonly
  case-insensitive, while Linux filesystems are case-sensitive by default;
  this caused the first `skin.ini` not found error;
- after renaming the file, the configuration could still be UTF-16LE while the
  parser expected UTF-8, causing the subsequent configuration read failure.

ssfconv now accepts both `Skin.ini` and `skin.ini`, supports UTF-8, UTF-8 BOM,
UTF-16LE, and UTF-16BE configuration files, and converts the skin to a Fcitx5
theme.

## Usage

The input path selects the workflow automatically:

```sh
ssfconv <input.ssf> -o <output-dir>
ssfconv <input.ssf>
ssfconv --unpack <input.ssf> -o <unpacked-dir>
ssfconv <unpacked-dir> -o <theme-dir>
```

When `-o` is omitted for an SSF file, the theme is installed under
`~/.local/share/fcitx5/themes/<theme-name>`.

## Options

| Option | Description |
| --- | --- |
| `-o`, `--output DIR` | Set the output directory |
| `-u`, `--unpack` | Only unpack an SSF file |
| `-f`, `--force` | Overwrite an existing output |
| `-v`, `--version` | Show the version |
| `-h`, `--help` | Show help |

Use `--force` or `-f` to skip the overwrite prompt. `--unpack` only accepts
SSF files. Directory input must contain `Skin.ini` or `skin.ini`.

## Build

```sh
sudo pacman -S --needed base-devel cmake pkgconf openssl zlib libzip libpng
cmake -S . -B build
cmake --build build
```

## Fixes and Improvements

- Native C11 implementation without Python or Qt runtime dependencies, using OpenSSL, zlib, libzip, and libpng for format handling;
- support for regular ZIP and `Skin`-encrypted SSF files;
- UTF-8, UTF-8 BOM, UTF-16LE, and UTF-16BE configuration support;
- UTF-16LE archive filename conversion to UTF-8;
- archive traversal and boundary checks;
- dynamically sized decompression buffers;
- clear extraction and theme-generation errors with incomplete-output cleanup;

## Attribution and License

This project preserves and credits [VOID001/ssf2fcitx](https://github.com/VOID001/ssf2fcitx), [fkxxyz/ssfconv](https://github.com/fkxxyz/ssfconv), [RadND/ssfconv](https://github.com/RadND/ssfconv), and KDE `kimtoy`.

Licensed under the [GNU General Public License v3.0](LICENSE).
