#!/usr/bin/env bash
# Rebuilds lib/V6/libapriltag.a (NAO V6) and lib/Linux/libapriltag.a (desktop Linux x86_64)
# from src/. Works from any working directory and needs no network.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
buildchain="$here/../Buildchain/V6/include"

if command -v llvm-ar >/dev/null 2>&1; then
  ar_tool=llvm-ar
else
  ar_tool=ar
fi

mapfile -t sources < <(cd "$here/src" && find . -name '*.c' | sort)

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

# build <name> <output archive> <compiler flags...>
build() {
  local name="$1" output="$2"
  shift 2
  local objdir="$tmp/$name"
  mkdir -p "$objdir" "$(dirname "$output")"
  local src obj objects=()
  for src in "${sources[@]}"; do
    obj="$objdir/${src#./}.o"
    mkdir -p "$(dirname "$obj")"
    clang "$@" -std=gnu99 -O3 -DNDEBUG -I"$here/include/apriltag" -I"$here/include/apriltag/common" -c "$here/src/${src#./}" -o "$obj"
    objects+=("$obj")
  done
  rm -f "$output"
  "$ar_tool" rcs "$output" "${objects[@]}"
  echo "built $output"
}

# NAO V6: same target and include setup as Make/CMake/NaoToolchain.cmake.
build v6 "$here/lib/V6/libapriltag.a" \
  --target=x86_64-linux-gnu -march=silvermont -mtune=silvermont -nostdinc \
  -isystem "$(clang -print-resource-dir)/include" \
  -isystem "$buildchain/x86_64-linux-gnu" \
  -isystem "$buildchain"

# Desktop Linux x86_64.
build linux "$here/lib/Linux/libapriltag.a" -fPIC
