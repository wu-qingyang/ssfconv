# ssfconv

[English Version](README.en.md)

原生 C11 实现的搜狗输入法 SSF 皮肤转换器，将 SSF 皮肤转换为 Fcitx5 主题。

## 效果预览

本项目最初是因为想在 Linux 上使用《千恋万花》中的小丛雨皮肤而开始研究
SSF 转换。感谢皮肤作者B站UP主 [美有问题喵](https://space.bilibili.com/30532016)
创作并分享这款皮肤。

![《千恋万花》小丛雨皮肤效果预览](docs/congyu-skin-preview.png)

## 项目背景

本项目起始于在 Linux 环境下使用上游 `ssfconv` 转换 SSF 皮肤时遇到的错误。
经过排查发现，上游不同版本存在以下兼容性问题，导致部分 SSF 皮肤无法正常转换：

- 配置文件通常命名为 `Skin.ini`，但部分实现只查找小写的 `skin.ini`。
  Windows 文件系统通常不区分大小写，而 Linux 默认区分大小写，因此会出现找不到
  配置文件的问题；
- 部分实现混用 `pathlib.Path` 对象和字符串拼接路径，运行时会触发 `TypeError`，
  导致转换流程中断；
- 图片平均颜色计算使用了过小的 `uint8` 类型累加像素值，超过类型范围后会发生
  溢出，并在较新版本的 NumPy 中进一步触发 `OverflowError`。

这些问题并非 SSF 格式本身无法使用，而是上游实现对 Linux 文件系统行为、Python
路径类型以及图像数据类型处理不够兼容。`ssfconv` 因此改用原生 C11 重新实现转换
流程，并针对上述问题提供兼容处理。

## 使用

程序会根据输入路径自动选择处理流程：

```bash
# SSF 文件：解包并生成 Fcitx5 主题
ssfconv <input.ssf> -o <output-dir>

# 未指定输出目录：安装到 ~/.local/share/fcitx5/themes/<theme-name>
ssfconv <input.ssf>

# SSF 文件：只解包
ssfconv --unpack <input.ssf> -o <unpacked-dir>

# 已解包目录：检查 Skin.ini 后生成 Fcitx5 主题
ssfconv <unpacked-dir> -o <theme-dir>
```

未指定 `-o` 时，SSF 文件会安装到 `~/.local/share/fcitx5/themes/<主题名>`。

## 参数

长参数和短参数可以互换：

| 参数 | 说明 |
| --- | --- |
| `-o`, `--output DIR` | 指定输出目录 |
| `-u`, `--unpack` | 仅解包 SSF 文件，不生成主题 |
| `-f`, `--force` | 覆盖已有输出目录或文件 |
| `-v`, `--version` | 显示版本 |
| `-h`, `--help` | 显示帮助 |

使用 `--force` 或 `-f` 可以跳过询问并直接覆盖。`--unpack` 只适用于 SSF 文件。目录输入必须包含 `Skin.ini` 或 `skin.ini`。

## 构建

Arch Linux：

```bash
sudo pacman -S --needed base-devel cmake pkgconf openssl zlib libzip libpng
cmake -S . -B build
cmake --build build
```

运行时需要 OpenSSL、zlib、libzip 和 libpng。构建完成后可执行文件位于 `build/ssfconv`。

## 修复与改进

相较于上游实现，本项目修复或改进了以下主要问题：

- 使用原生 C11 实现，移除 Python 和 Qt 运行时依赖，使用 OpenSSL、zlib、libzip 和 libpng 处理；
- 同时支持普通 ZIP SSF 和 `Skin` 加密 SSF；
- 支持 UTF-8、UTF-8 BOM、UTF-16LE 和 UTF-16BE 配置文件；
- 将加密 SSF 中的 UTF-16LE 文件名转换为 UTF-8；
- 拒绝绝对路径和目录穿越路径，避免归档解包越界；
- 检查归档偏移、长度、压缩数据和解密数据边界；
- 解压缓冲区按需扩展，避免依赖固定输出大小；
- 解包和主题生成失败时返回明确错误，并清理本次产生的不完整输出；

## 致谢与版权

本项目保留并致谢以下工作：

- [VOID001/ssf2fcitx](https://github.com/VOID001/ssf2fcitx)：SSF 解包和早期转换实现；
- [fkxxyz/ssfconv](https://github.com/fkxxyz/ssfconv)：Python 转换器上游项目；
- [RadND/ssfconv](https://github.com/RadND/ssfconv)：后续 Python 实现和修复；
- KDE `kimtoy`：SSF 容器格式参考实现。

项目使用 [GNU General Public License v3.0](LICENSE) 授权。
