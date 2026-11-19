#!/usr/bin/env bash
#
# mayhem/build.sh — crosspoint-reader fuzz harnesses + upstream host test suite.
#
# The firmware only builds for ESP32 under PlatformIO, so — like upstream's own host tests in
# test/ — each harness compiles the lib/ parser sources it fuzzes on the host, against the
# memory-backed stubs upstream ships in test/<suite>/stubs (HalStorage/HalFile, Logging, Arduino…).
# Each harness is described by mayhem/harnesses/<name>.conf (bash arrays SRCS/INCS/DEFS, paths
# relative to the repo root; TP_PREFIX/TP_EXTRA relax one UBSan check on a vendored third-party
# source only); every listed project source is compiled with $SANITIZER_FLAGS.
#
# Outputs: /mayhem/<name> (libFuzzer) and /mayhem/<name>-standalone (run-once reproducer).
# Test suite: upstream test/CMakeLists.txt, normal flags, in mayhem/build-tests/ (gitignored),
# googletest/ArduinoJson/AnimatedGIF resolved offline from /opt/toolchains/deps.
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
: "${OUT:=$SRC}"
: "${GTEST_SRC:=/opt/toolchains/deps/googletest-1.17.0/src}"
: "${ARDUINOJSON_SRC:=/opt/toolchains/deps/arduinojson-7.4.2/src}"
: "${ANIMATEDGIF_SRC:=/opt/toolchains/deps/animatedgif-67cfacafe12ef4926c7713342c57d2a4af040e5e/src}"
: "${JPEGDEC_SRC:=/opt/toolchains/deps/jpegdec-86282979224c8a32fd51e091ed5a35b0c699a52b/src}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS
cd "$SRC"

FB="$SRC/mayhem/build-fuzz"
mkdir -p "$FB"
# shellcheck disable=SC2086
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o "$FB/lsan_off.o"
# shellcheck disable=SC2086
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$FB/standalone_main.o"

# A pinned, patched AnimatedGIF copy (upstream patches it in place with scripts/patch_animatedgif.py).
AGIF="$SRC/mayhem/build-tests/animatedgif"
if [ ! -f "$AGIF/src/AnimatedGIF.cpp" ]; then
  mkdir -p "$SRC/mayhem/build-tests"
  rm -rf "$AGIF"
  cp -r "$ANIMATEDGIF_SRC" "$AGIF"
  chmod -R u+w "$AGIF"
fi
python3 scripts/patch_animatedgif.py "$AGIF"

# A pinned JPEGDEC copy with upstream's scripts/jpegdec_patches applied (the firmware build does
# the same through scripts/patch_jpegdec.py). Re-created from the cache on every run, so the
# patches always apply to pristine sources.
JDEC="$SRC/mayhem/build-tests/jpegdec"
rm -rf "$JDEC"
mkdir -p "$SRC/mayhem/build-tests"
cp -r "$JPEGDEC_SRC" "$JDEC"
chmod -R u+w "$JDEC"
for p in scripts/jpegdec_patches/*.patch; do patch -s -p1 -d "$JDEC" < "$p"; done

build_one() {   # build_one <conf>
  local conf="$1" NAME SRCS=() INCS=() DEFS=() CXXSTD="-std=c++20" EXTRA=() TP_PREFIX=() TP_EXTRA=()
  # shellcheck disable=SC1090
  source "$conf"
  local od="$FB/$NAME" objs=() f o
  mkdir -p "$od"
  local iflags=() dflags=()
  for f in "${INCS[@]}"; do iflags+=("-I$f"); done
  for f in "${DEFS[@]}"; do dflags+=("-D$f"); done
  # Coverage instrumentation on the project sources too (not only the harness), so libFuzzer and
  # Mayhem see edges in the parser being fuzzed.
  local cov="-fsanitize=fuzzer-no-link"
  for f in "${SRCS[@]}"; do
    # TP_EXTRA: sanitizer relaxations for vendored third-party sources only (TP_PREFIX paths).
    local fx=() p
    for p in "${TP_PREFIX[@]}"; do case "$f" in "$p"*) fx=("${TP_EXTRA[@]}") ;; esac; done
    o="$od/$(echo "$f" | tr '/' '_').o"
    case "$f" in
      *.c) $CC $SANITIZER_FLAGS $DEBUG_FLAGS $cov -O1 "${dflags[@]}" "${iflags[@]}" "${EXTRA[@]}" "${fx[@]}" -c "$f" -o "$o" ;;
      *)   $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $cov -O1 $CXXSTD "${dflags[@]}" "${iflags[@]}" "${EXTRA[@]}" "${fx[@]}" -c "$f" -o "$o" ;;
    esac
    objs+=("$o")
  done
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS -O1 $CXXSTD "${dflags[@]}" "${iflags[@]}" "${EXTRA[@]}" \
    $cov -c "mayhem/harnesses/$NAME.cpp" -o "$od/harness.o"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$od/harness.o" "${objs[@]}" "$FB/lsan_off.o" \
    -lm -o "$OUT/$NAME"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS "$od/harness.o" "${objs[@]}" "$FB/lsan_off.o" "$FB/standalone_main.o" \
    -lm -o "$OUT/$NAME-standalone"
  echo "built $OUT/$NAME"
}

export -f build_one
export FB OUT
ls mayhem/harnesses/*.conf | xargs -P "$MAYHEM_JOBS" -I{} bash -c 'set -euo pipefail; build_one "$@"' _ {}

# Upstream host test suite (normal flags), the oracle mayhem/test.sh runs.
TB="$SRC/mayhem/build-tests/b"
cmake -S test -B "$TB" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$GTEST_SRC" \
  -DFETCHCONTENT_SOURCE_DIR_ARDUINOJSON="$ARDUINOJSON_SRC" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON \
  -DANIMATEDGIF_SOURCE_DIR="$AGIF" \
  -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS"
cmake --build "$TB" -j"$MAYHEM_JOBS"
