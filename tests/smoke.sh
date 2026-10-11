#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "用法：$0 path/to/skin.ssf" >&2
    exit 2
fi

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
input=$(CDPATH= cd -- "$(dirname -- "$1")" && pwd)/$(basename -- "$1")
build=${TMPDIR:-/tmp}/ssfconv-smoke-build
output=${TMPDIR:-/tmp}/ssfconv-smoke-output
unpacked=${TMPDIR:-/tmp}/ssfconv-smoke-unpacked
directory_output=${TMPDIR:-/tmp}/ssfconv-smoke-directory-output

rm -rf "$build" "$output" "$unpacked" "$directory_output"
cmake -S "$root" -B "$build"
cmake --build "$build"
"$build/ssfconv" "$input" -o "$output"
"$build/ssfconv" --unpack "$input" -o "$unpacked"
"$build/ssfconv" "$unpacked" -o "$directory_output"

test -f "$output/theme.conf"
test -f "$directory_output/theme.conf"
echo "冒烟测试通过：$input"
