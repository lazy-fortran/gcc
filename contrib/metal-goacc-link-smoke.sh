#!/bin/sh
# Smoke-test Metal OpenACC offload at *link* time (mkoffload + metallib).
#
# Usage (example for this repo's local layout):
#   contrib/metal-goacc-link-smoke.sh \
#     --host /Users/ert/code/gcc-dev/gcc-metal-v2-build/gcc/xgcc \
#     --hostb /Users/ert/code/gcc-dev/gcc-metal-v2-build/gcc \
#     --offload-path /Users/ert/code/gcc-dev/gcc-metal-v2-accel-install \
#     --libgomp-path /Users/ert/code/gcc-dev/gcc-metal-v2-build/aarch64-apple-darwin25.2.0/libgomp/.libs \
#     --limit 50
#
# Notes:
# - Filters out sources mentioning double/long double/_Complex (Metal doesn't
#   support double-precision in the shading language).
# - Only considers tests that appear to have a main() and so should link.

set -eu

usage() {
  cat <<'EOF' >&2
Usage: metal-goacc-link-smoke.sh --host XGCC --hostb DIR --offload-path DIR --libgomp-path DIR [--limit N] [--outdir DIR]
EOF
  exit 2
}

HOST=
HOSTB=
OFFLOAD_PATH=
LIBGOMP_PATH=
LIMIT=50
OUTDIR=${TMPDIR:-/tmp}/metal-goacc-link-smoke

while [ $# -gt 0 ]; do
  case "$1" in
    --host) HOST=$2; shift 2;;
    --hostb) HOSTB=$2; shift 2;;
    --offload-path) OFFLOAD_PATH=$2; shift 2;;
    --libgomp-path) LIBGOMP_PATH=$2; shift 2;;
    --limit) LIMIT=$2; shift 2;;
    --outdir) OUTDIR=$2; shift 2;;
    -h|--help) usage;;
    *) echo "unknown arg: $1" >&2; usage;;
  esac
done

test -n "$HOST" || usage
test -n "$HOSTB" || usage
test -n "$OFFLOAD_PATH" || usage
test -n "$LIBGOMP_PATH" || usage

if ! command -v rg >/dev/null 2>&1; then
  echo "error: ripgrep (rg) not found in PATH" >&2
  exit 1
fi

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd -P)
GOACC_DIR1=$ROOT/gcc/testsuite/c-c++-common/goacc
GOACC_DIR2=$ROOT/gcc/testsuite/gcc.dg/goacc
test -d "$GOACC_DIR1" || { echo "error: missing $GOACC_DIR1" >&2; exit 1; }
test -d "$GOACC_DIR2" || { echo "error: missing $GOACC_DIR2" >&2; exit 1; }

mkdir -p "$OUTDIR"
LOG=$OUTDIR/summary.log
: >"$LOG"

export GCC_OFFLOAD_PATH="$OFFLOAD_PATH"
export OFFLOAD_TARGET_DEFAULT="$OFFLOAD_PATH/bin/aarch64-apple-darwin25.2.0-accel-metal-none-none-gcc"
export LIBRARY_PATH="$LIBGOMP_PATH"

pass=0
fail=0
tried=0

echo "HOST=$HOST" | tee -a "$LOG"
echo "HOSTB=$HOSTB" | tee -a "$LOG"
echo "GCC_OFFLOAD_PATH=$GCC_OFFLOAD_PATH" | tee -a "$LOG"
echo "OFFLOAD_TARGET_DEFAULT=$OFFLOAD_TARGET_DEFAULT" | tee -a "$LOG"
echo "LIBRARY_PATH=$LIBRARY_PATH" | tee -a "$LOG"
echo | tee -a "$LOG"

stub_main=$OUTDIR/__metal_goacc_stub_main.c
cat >"$stub_main" <<'EOF'
int main(void) { return 0; }
EOF

# Candidate sources: seem to exercise OpenACC codegen, avoid double/long double/_Complex.
for f in $(rg -l --no-messages -g'*.c' '^[[:space:]]*#pragma[[:space:]]+acc[[:space:]]' "$GOACC_DIR1" "$GOACC_DIR2" | sort); do
  if rg -q --no-messages '\blong[[:space:]]+double\b' "$f"; then
    continue
  fi
  if rg -q --no-messages '\bdouble\b' "$f"; then
    continue
  fi
  if rg -q --no-messages '_Complex' "$f"; then
    continue
  fi
  # Skip tests that are explicitly checking for diagnostics.
  if rg -q --no-messages 'dg-(error|warning|bogus)' "$f"; then
    continue
  fi

  base=$(basename "$f")
  out=$OUTDIR/${base%.c}
  err=$OUTDIR/${base%.c}.err

  echo "=== $base" | tee -a "$LOG"
  if rg -q --no-messages --pcre2 '\bmain\s*\(' "$f" 2>/dev/null; then
    cmd="$HOST -B$HOSTB -fopenacc -foffload=metal -O0 -g0 -o $out $f"
  else
    cmd="$HOST -B$HOSTB -fopenacc -foffload=metal -O0 -g0 -o $out $stub_main $f"
  fi

  # shellcheck disable=SC2086
  if $cmd >"$out".stdout 2>"$err"; then
    echo "PASS $base" | tee -a "$LOG"
    pass=$((pass + 1))
  else
    if rg -q --no-messages "'double' is not supported in Metal" "$err"; then
      echo "SKIP $base (needs double)" | tee -a "$LOG"
    elif rg -q --no-messages 'Undefined symbols for architecture|symbol\\(s\\) not found' "$err"; then
      echo "SKIP $base (not standalone)" | tee -a "$LOG"
    else
      echo "FAIL $base (see $err)" | tee -a "$LOG"
      fail=$((fail + 1))
      sed -n '1,60p' "$err" | sed 's/^/  /' | tee -a "$LOG" || true
    fi
  fi

  echo | tee -a "$LOG"
  tried=$((tried + 1))
  if [ "$tried" -ge "$LIMIT" ]; then
    break
  fi
done

echo "SUMMARY tried=$tried pass=$pass fail=$fail" | tee -a "$LOG"
echo "Log: $LOG" >&2
