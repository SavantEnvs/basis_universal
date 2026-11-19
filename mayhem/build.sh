#!/usr/bin/env bash
#
# mayhem/build.sh — build basis_universal's fuzz harnesses, the basisu CLI target, and the
# normal-flags basisu tool used by mayhem/test.sh. Runs as `mayhem` in /mayhem (see the base image
# for CC/CXX/SANITIZER_FLAGS/LIB_FUZZING_ENGINE/STANDALONE_FUZZ_MAIN). No network needed: every
# dependency (zstd, wuffs, tinyexr, tinydds, miniz) is vendored in the tree. Re-runnable: both CMake
# trees live under the gitignored build/ directory and are reused incrementally.
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
export CC CXX

cd "${SRC:-/mayhem}"
SRC="$PWD"
OUT="$SRC/build/mayhem-out"
mkdir -p "$OUT"

# SanCov on the library too, so libFuzzer sees edges in the codec, not only in the harness. The
# coverage callbacks are provided by the sanitizer runtime, so only add it when sanitizers are on.
SANCOV=""
[ -n "$SANITIZER_FLAGS" ] && SANCOV="-fsanitize=fuzzer-no-link"
# UBSan's alignment check is relaxed (only that check; ASan and the rest of UBSan stay on and halting):
# the codec reads packed block structs (uastc_block, jpgd coeff_buf) at unaligned offsets by design,
# which trips on the first input of the compress/jpgd targets. Upstream's own BASISU_SAN CMake
# option disables exactly this check (-fno-sanitize=alignment) for the same reason.
[ -n "$SANITIZER_FLAGS" ] && SANCOV="$SANCOV -fno-sanitize=alignment"
FUZZ_FLAGS="-O1 $SANITIZER_FLAGS $SANCOV $DEBUG_FLAGS"

# LeakSanitizer off for every binary (fuzzers, standalones and the basisu CLI target).
"$CXX" $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$OUT/lsan_off.o"

# 1) The project (libbasisu_encoder.a = encoder + transcoder + zstd, and the basisu tool),
#    sanitized + instrumented. CMAKE_BUILD_TYPE=None so only our flags apply (Release would add -O3).
cmake -S "$SRC" -B "$SRC/build/mayhem-fuzz" -G Ninja \
  -DCMAKE_BUILD_TYPE=None -DBASISU_EXAMPLES=OFF -DBASISU_SSE=OFF \
  -DCMAKE_C_FLAGS="$FUZZ_FLAGS" -DCMAKE_CXX_FLAGS="$FUZZ_FLAGS" \
  -DCMAKE_EXE_LINKER_FLAGS="$OUT/lsan_off.o"
cmake --build "$SRC/build/mayhem-fuzz" -j"$MAYHEM_JOBS" --target basisu_encoder
LIB="$SRC/build/mayhem-fuzz/libbasisu_encoder.a"

# 1b) The basisu CLI target (Mayhemfile_basisu, `afl: true`): the same sanitized project built with
#     afl-clang-fast so Mayhem gets AFL edge coverage on the file-input tool (a SanCov-only CLI binary
#     made Mayhem's black-box fuzzer restart-loop with 0 edges). Its own CMake tree under build/.
AFL_FLAGS="-O1 $SANITIZER_FLAGS $DEBUG_FLAGS"
[ -n "$SANITIZER_FLAGS" ] && AFL_FLAGS="$AFL_FLAGS -fno-sanitize=alignment"
CC=afl-clang-fast CXX=afl-clang-fast++ cmake -S "$SRC" -B "$SRC/build/mayhem-afl" -G Ninja \
  -DCMAKE_BUILD_TYPE=None -DBASISU_EXAMPLES=OFF -DBASISU_SSE=OFF \
  -DCMAKE_C_FLAGS="$AFL_FLAGS" -DCMAKE_CXX_FLAGS="$AFL_FLAGS" \
  -DCMAKE_EXE_LINKER_FLAGS="$OUT/lsan_off.o"
cmake --build "$SRC/build/mayhem-afl" -j"$MAYHEM_JOBS" --target basisu
# Upstream CMake writes the tool to $SRC/bin/basisu for every build tree; move it out so the two
# trees never share (or skip relinking) that path on a re-run.
mv -f "$SRC/bin/basisu" "$SRC/basisu-fuzz"

# 2) Harnesses: each linked twice — libFuzzer binary and run-once standalone reproducer.
"$CC" $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$OUT/standalone_main.o"
DEFS="-DBASISU_SUPPORT_SSE=0 -DBASISU_USE_WUFFS_LDR_LOADER=1 -DBASISD_SUPPORT_KTX2_ZSTD=1"
HARNESSES="ktx2 basis pvpng jpgd exr tga rgbe ldr_image astc dds compress"
build_one() {  # name source
  local name="$1" src="$2"
  "$CXX" $FUZZ_FLAGS $DEFS -std=c++17 -I"$SRC" -I"$SRC/transcoder" -I"$SRC/encoder" \
    -c "$src" -o "$OUT/$name.o"
  "$CXX" $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$OUT/$name.o" "$LIB" "$OUT/lsan_off.o" \
    -lpthread -lm -o "$SRC/fuzz_$name"
  "$CXX" $SANITIZER_FLAGS $DEBUG_FLAGS "$OUT/$name.o" "$OUT/standalone_main.o" "$LIB" "$OUT/lsan_off.o" \
    -lpthread -lm -o "$SRC/fuzz_$name-standalone"
}
for h in $HARNESSES; do build_one "$h" "$SRC/mayhem/harnesses/fuzz_$h.cpp"; done
build_one floor_log2i "$SRC/mayhem/fuzz_floor_log2i.cpp"

# 3) The basisu tool with the project's NORMAL flags (Release), for mayhem/test.sh.
env -u CFLAGS -u CXXFLAGS -u LDFLAGS cmake -S "$SRC" -B "$SRC/build/mayhem-test" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBASISU_EXAMPLES=OFF \
  -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS"
cmake --build "$SRC/build/mayhem-test" -j"$MAYHEM_JOBS" --target basisu
mv -f "$SRC/bin/basisu" "$SRC/build/mayhem-test/basisu-test"
echo "build.sh: done"
