#!/usr/bin/env bash
#
# mayhem/test.sh — RUN this repo's OWN functional test suite (already built by mayhem/build.sh).
# exit 0 = pass. EDIT per repo. PATCH-grade oracle: after an agent patches the source, the grader
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

# Upstream's host test suite (test/CMakeLists.txt: ~900 gtest cases over the EPUB/CSS/OPF/ZIP/XTC/
# font/BMP/GIF/JSON/OPDS parsers plus python checks), built by mayhem/build.sh with normal flags.
TB="$SRC/mayhem/build-tests/b"
if [ ! -f "$TB/CTestTestfile.cmake" ]; then
  echo "test suite not built ($TB missing) — run mayhem/build.sh first" >&2
  emit_ctrf "cmake-ctest" 0 1 0
  exit 1
fi
log="$(mktemp)"
xmldir="$(mktemp -d)"
# Every gtest binary also writes its own XML report (GTEST_OUTPUT): a gtest case only counts as
# passed when its binary REPORTED it as run and passing — a test process that merely exits 0
# without running its assertions (e.g. a neutered binary) is a failure, not a pass. Serial (-j1):
# gtest picks unique report file names per binary, which races when ctest runs them in parallel.
(cd "$TB" && GTEST_OUTPUT="xml:$xmldir/" ctest -j1 --output-on-failure) >"$log" 2>&1
rc=$?
tail -20 "$log"
(cd "$TB" && ctest -N -V) >"$log.list" 2>&1
counts="$(python3 -I - "$xmldir" "$log" "$log.list" <<'PY'
import glob, re, sys
import xml.etree.ElementTree as ET
xmldir, log, listing = sys.argv[1:4]
lst = open(listing).read()
alltests = set(re.findall(r"Test\s+#(\d+):", lst))
gtests = {n for n, cmd in re.findall(r"^(\d+): Test command: (.*)$", lst, re.M) if "--gtest_filter" in cmd}
failed = set(re.findall(r"^\s*(\d+) - .*\((?:Failed|SEGFAULT|Subprocess aborted|Timeout|Not Run|Exception|Child aborted|ILLEGAL|BUS|OTHER_FAULT)", open(log).read(), re.M))
reported = 0
for f in glob.glob(xmldir + "/*.xml"):
    try:
        root = ET.parse(f).getroot()
    except ET.ParseError:
        continue
    for tc in root.iter("testcase"):
        if tc.get("status", "run") == "run" and tc.get("result", "completed") == "completed" \
                and tc.find("failure") is None:
            reported += 1
g_pass = min(len(gtests - failed), reported)
g_fail = len(gtests) - g_pass
scripts = alltests - gtests
s_fail = len(scripts & failed)
print(g_pass + len(scripts) - s_fail, g_fail + s_fail, len(gtests), reported)
PY
)"
rm -rf "$log" "$log.list" "$xmldir"
read -r passed failed ngtest nreported <<<"$counts"
if [ -z "${passed:-}" ]; then
  echo "could not compute test counts (ctest exit $rc)" >&2
  emit_ctrf "cmake-ctest+gtest-xml" 0 1 0
  exit 1
fi
echo "gtest cases: $ngtest registered with ctest, $nreported reported passing by gtest itself"
if [ "$rc" -ne 0 ] && [ "$failed" -eq 0 ]; then failed=1; fi
emit_ctrf "cmake-ctest+gtest-xml" "$passed" "$failed" 0
