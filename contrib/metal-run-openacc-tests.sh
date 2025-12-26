#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: contrib/metal-run-openacc-tests.sh <builddir> [--jobs N] [--offload TARGET]

Runs libgomp OpenACC tests with the Metal offload target and prints a short
summary based on the generated *.sum file.

Examples:
  contrib/metal-run-openacc-tests.sh ../gcc-build/gcc --jobs 16
  contrib/metal-run-openacc-tests.sh ../gcc-metal-build/gcc

Notes:
  - Requires Apple Metal tools (xcrun metal, xcrun metallib) to actually build
    Metal offload images.
  - Uses RUNTESTFLAGS="--target_board=unix/-foffload=<TARGET>".
EOF
}

if [[ $# -lt 1 ]]; then
  usage
  exit 2
fi

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

builddir=$1
shift

jobs=""
offload="metal"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --jobs)
      jobs=$2
      shift 2
      ;;
    --offload)
      offload=$2
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage
      exit 2
      ;;
  esac
done

if [[ ! -d "$builddir" ]]; then
  echo "Build directory not found: $builddir" >&2
  exit 2
fi

if ! command -v xcrun >/dev/null 2>&1; then
  echo "xcrun not found; cannot run Metal offload tests." >&2
  exit 2
fi

if ! xcrun -sdk macosx -find metal >/dev/null 2>&1; then
  echo "xcrun metal not found (install Xcode/CLT with Metal tools)." >&2
  exit 2
fi

if ! xcrun -sdk macosx -find metallib >/dev/null 2>&1; then
  echo "xcrun metallib not found (install Xcode/CLT with Metal tools)." >&2
  exit 2
fi

make_args=()
if [[ -n "$jobs" ]]; then
  make_args+=("-j$jobs")
fi

echo "Running: make -C $builddir check-target-libgomp RUNTESTFLAGS=--target_board=unix/-foffload=$offload" >&2
make -C "$builddir" "${make_args[@]}" check-target-libgomp \
  RUNTESTFLAGS="--target_board=unix/-foffload=${offload}"

sum_file=$(
  find "$builddir" -type f -name 'libgomp.sum' -print0 \
    | xargs -0 ls -t 2>/dev/null \
    | head -n 1
)

if [[ -z "${sum_file:-}" || ! -f "$sum_file" ]]; then
  echo "Could not find libgomp.sum under $builddir" >&2
  exit 1
fi

echo "" >&2
echo "Summary file: $sum_file" >&2

awk '
  /^[A-Z][A-Z]*:/ {
    key=$1
    sub(/:$/, "", key)
    c[key]++
    c["TOTAL"]++
  }
  END {
    printf("TOTAL: %d\n", c["TOTAL"]+0)
    for (k in c) {
      if (k == "TOTAL") continue
      printf("%s: %d\n", k, c[k])
    }
  }
' "$sum_file" | sort -k1,1

echo "" >&2
echo "Top FAIL/XPASS (first 50):" >&2
awk '/^(FAIL|XPASS):/ {print}' "$sum_file" | head -n 50 || true
