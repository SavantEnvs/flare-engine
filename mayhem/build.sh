#!/usr/bin/env bash
# mayhem/build.sh — build flare-engine's fuzz harnesses and the functional self-test.
#
# flare ships a single executable, no library. We run upstream's CMake build of `flare` twice:
#   build/mayhem-fuzz : $SANITIZER_FLAGS + SanCov (-fsanitize=fuzzer-no-link) + DWARF 3
#   build/mayhem-test : upstream's normal Release flags (functional oracle)
# then archive every object except main.cpp's into a static lib and link the harnesses
# (mayhem/harness/*.cpp) and the self-test (mayhem/tests/flare_selftest.cpp) against it.
# build/ is gitignored upstream, so `git clean -ffdX` removes both trees.
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export CC CXX
cd "$SRC"

SDL_CFLAGS="$(pkg-config --cflags sdl2 SDL2_image SDL2_mixer SDL2_ttf)"
SDL_LIBS="$(pkg-config --libs sdl2 SDL2_image SDL2_mixer SDL2_ttf)"

# Archive every compiled engine object except main.cpp.o (which owns main() and the
# GameSwitcher global) into <dir>/libflare_engine.a.
archive_objs() {
  local dir="$1"
  rm -f "$dir/libflare_engine.a"
  find "$dir/CMakeFiles/flare.dir/src" -name '*.o' ! -name 'main.cpp.o' -print0 \
    | sort -z | xargs -0 ar rcs "$dir/libflare_engine.a"
}

# 1) Sanitized + coverage-instrumented engine build.
FUZZ_CXXFLAGS="$SANITIZER_FLAGS -fsanitize=fuzzer-no-link $DEBUG_FLAGS"
cmake -S . -B build/mayhem-fuzz -DCMAKE_BUILD_TYPE=None \
  -DCMAKE_CXX_FLAGS="$FUZZ_CXXFLAGS" -DCMAKE_C_FLAGS="$FUZZ_CXXFLAGS" \
  -DCMAKE_EXE_LINKER_FLAGS="$SANITIZER_FLAGS -fsanitize=fuzzer-no-link"
cmake --build build/mayhem-fuzz --target flare -j"$MAYHEM_JOBS"
archive_objs build/mayhem-fuzz

HARNESS_FLAGS="-std=c++11 -fno-exceptions -Isrc -Imayhem/harness $SDL_CFLAGS -DFLARE_DEFAULT_MOD_DIR=\"$SRC/mods/default\""
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o build/mayhem-fuzz/lsan_off.o
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o build/mayhem-fuzz/standalone_main.o

for h in mayhem/harness/fuzz_*.cpp; do
  name="$(basename "$h" .cpp)"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $HARNESS_FLAGS -c "$h" -o "build/mayhem-fuzz/$name.o"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "build/mayhem-fuzz/$name.o" \
    build/mayhem-fuzz/lsan_off.o build/mayhem-fuzz/libflare_engine.a $SDL_LIBS -o "$SRC/$name"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS "build/mayhem-fuzz/$name.o" build/mayhem-fuzz/standalone_main.o \
    build/mayhem-fuzz/lsan_off.o build/mayhem-fuzz/libflare_engine.a $SDL_LIBS -o "$SRC/$name-standalone"
done

# 2) Functional self-test, upstream's normal Release flags (no sanitizers).
cmake -S . -B build/mayhem-test -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS" -DCMAKE_EXE_LINKER_FLAGS="$COVERAGE_FLAGS"
cmake --build build/mayhem-test --target flare -j"$MAYHEM_JOBS"
archive_objs build/mayhem-test
$CXX -O2 $COVERAGE_FLAGS -std=c++11 -Isrc -Imayhem/harness $SDL_CFLAGS -DFLARE_DEFAULT_MOD_DIR="\"$SRC/mods/default\"" \
  mayhem/tests/flare_selftest.cpp build/mayhem-test/libflare_engine.a $SDL_LIBS \
  -o build/mayhem-test/flare_selftest
