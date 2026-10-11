cmake_minimum_required(VERSION 3.16)
project(ssfconv VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(OPENSSL REQUIRED IMPORTED_TARGET openssl)
pkg_check_modules(LIBPNG REQUIRED IMPORTED_TARGET libpng)
pkg_check_modules(ZLIB REQUIRED IMPORTED_TARGET zlib)
pkg_check_modules(LIBZIP REQUIRED IMPORTED_TARGET libzip)

add_executable(ssfconv
    src/main.c
    src/fcitx5_config.c
    src/i18n.c
    src/ssf_archive.c
    src/skin_config.c
    src/skin_image.c
    src/fcitx5_theme.c
)
target_include_directories(ssfconv PRIVATE src)
target_compile_definitions(ssfconv PRIVATE SSFCONV_VERSION="${PROJECT_VERSION}")
target_link_libraries(ssfconv PRIVATE
    PkgConfig::OPENSSL
    PkgConfig::LIBPNG
    PkgConfig::ZLIB
    PkgConfig::LIBZIP
)
target_compile_options(ssfconv PRIVATE -Wall -Wextra -Wpedantic)
