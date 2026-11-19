#!/usr/bin/env bash
#
# mayhem/test.sh — known-answer functional suite for basis_universal, run with the normal-flags basisu
# tool that mayhem/build.sh built (build/mayhem-test/basisu-test). Upstream ships no unit-test runner,
# so each case decodes/encodes a committed test_files/ input and compares the SHA-256 of every output
# file against mayhem/test/golden.sha256 (one test per output file). PATCH-grade oracle: after an agent patches the source, the grader
# rebuilds (build.sh) then runs this. DELETE this file if the repo has no meaningful tests.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}


B="$SRC/build/mayhem-test/basisu-test"
GOLDEN="$SRC/mayhem/test/golden.sha256"
[ -x "$B" ] || { echo "test.sh: $B missing — run mayhem/build.sh first" >&2; emit_ctrf basisu-golden 0 1; exit 1; }
[ -f "$GOLDEN" ] || [ "${BASISU_TEST_GENERATE:-0}" = 1 ] || { echo "test.sh: $GOLDEN missing" >&2; emit_ctrf basisu-golden 0 1; exit 1; }

W="$(mktemp -d "${TMPDIR:-/tmp}/basisu-test.XXXXXX")"
trap 'rm -rf "$W"' EXIT

# Produce the outputs: unpack (transcode to every format + PNGs) four KTX2 files, and encode one PNG
# to ETC1S and UASTC LDR KTX2 single-threaded (deterministic).
mkdir -p "$W/out"
for f in kodim23.ktx2 kodim01.ktx2 base_xuastc_zstd.ktx2 base_xuastc_arith.ktx2; do
  "$B" -unpack -no_multithreading -output_path "$W/out" "$SRC/test_files/$f" > "$W/log.txt" 2>&1 \
    || echo "test.sh: unpack of $f exited non-zero" >&2
done
mkdir -p "$W/enc_etc1s" "$W/enc_uastc"
"$B" -no_multithreading -ktx2 -q 64 -output_path "$W/enc_etc1s" "$SRC/test_files/xmen_200.png" > "$W/log.txt" 2>&1 || true
"$B" -no_multithreading -ktx2 -uastc -output_path "$W/enc_uastc" "$SRC/test_files/xmen_200.png" > "$W/log.txt" 2>&1 || true

if [ "${BASISU_TEST_GENERATE:-0}" = 1 ]; then
  (cd "$W" && find out enc_etc1s enc_uastc -type f | LC_ALL=C sort | xargs sha256sum) > "$GOLDEN"
  echo "test.sh: wrote $(wc -l < "$GOLDEN") golden hashes to $GOLDEN"
fi

passed=0; failed=0
while read -r sum path; do
  [ -n "$path" ] || continue
  if [ -f "$W/$path" ] && [ "$(sha256sum "$W/$path" | cut -d' ' -f1)" = "$sum" ]; then
    passed=$((passed + 1))
  else
    failed=$((failed + 1)); echo "FAIL: $path" >&2
  fi
done < "$GOLDEN"
# Any output the golden list does not know about is also a failure (behavior changed).
extra=$(cd "$W" && find out enc_etc1s enc_uastc -type f | LC_ALL=C sort | comm -23 - <(cut -d' ' -f3- "$GOLDEN" | LC_ALL=C sort) | wc -l)
[ "$extra" -eq 0 ] || { echo "FAIL: $extra unexpected output files" >&2; failed=$((failed + extra)); }
[ "$passed" -gt 0 ] || failed=$((failed + 1))
emit_ctrf basisu-golden "$passed" "$failed" 0
