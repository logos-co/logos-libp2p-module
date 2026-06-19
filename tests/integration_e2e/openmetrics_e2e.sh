#!/usr/bin/env bash
# Runs openmetrics + libp2p_module under logoscore, scrapes /metrics, asserts.
# Required env: LIBP2P_LGX_DIR OPENMETRICS_LGX_DIR
# Optional env: LOGOSCORE_BIN (default: logoscore), LGPM_BIN (default: lgpm)

set -euo pipefail
shopt -s nullglob

for v in LIBP2P_LGX_DIR OPENMETRICS_LGX_DIR; do
    [[ -n "${!v:-}" ]] || { echo "missing env: $v" >&2; exit 1; }
done
: "${LOGOSCORE_BIN:=logoscore}"
: "${LGPM_BIN:=lgpm}"

pick_lgx() {
    local dir="$1" matches=("$1"/*.lgx)
    if (( ${#matches[@]} != 1 )); then
        echo "expected exactly 1 .lgx in $dir, found ${#matches[@]}" >&2
        exit 1
    fi
    printf '%s\n' "${matches[0]}"
}
LIBP2P_LGX="$(pick_lgx "$LIBP2P_LGX_DIR")"
OPENMETRICS_LGX="$(pick_lgx "$OPENMETRICS_LGX_DIR")"

WORK="$(mktemp -d)"
DAEMON_PID=""
PORT=$(( 49152 + RANDOM % 16384 ))

cleanup() {
    if [[ -n "$DAEMON_PID" ]]; then
        "$LOGOSCORE_BIN" call openmetrics stop >/dev/null 2>&1 || true
        "$LOGOSCORE_BIN" stop              >/dev/null 2>&1 || true
        kill "$DAEMON_PID" 2>/dev/null || true
        wait "$DAEMON_PID" 2>/dev/null || true
    fi
    rm -rf "$WORK"
}
trap cleanup EXIT

dump_daemon_log() {
    if [[ -z "$DAEMON_PID" ]]; then
        echo "daemon never started" >&2
    elif kill -0 "$DAEMON_PID" 2>/dev/null; then
        echo "daemon (pid $DAEMON_PID) still alive" >&2
    else
        echo "daemon (pid $DAEMON_PID) NOT alive" >&2
    fi
    echo "----- daemon log tail -----" >&2
    if [[ -f "$WORK/logs.txt" ]]; then
        tail -n 80 "$WORK/logs.txt" >&2
    fi
    echo "---------------------------" >&2
}

wait_until() {
    local desc="$1"; shift
    local deadline=$(( SECONDS + 30 ))
    while (( SECONDS < deadline )); do
        if "$@" >/dev/null 2>&1; then return 0; fi
        sleep 0.1
    done
    echo "timed out waiting for: $desc" >&2
    echo "----- last attempt output -----" >&2
    "$@" >&2 || true
    dump_daemon_log
    return 1
}

cd "$WORK"
mkdir -p modules

"$LGPM_BIN" --modules-dir ./modules --allow-unsigned install --file "$LIBP2P_LGX"
"$LGPM_BIN" --modules-dir ./modules --allow-unsigned install --file "$OPENMETRICS_LGX"
"$LGPM_BIN" --modules-dir ./modules list

"$LOGOSCORE_BIN" -D -m ./modules > logs.txt 2>&1 &
DAEMON_PID=$!

wait_until "logoscore daemon ready" \
    "$LOGOSCORE_BIN" load-module libp2p_module
"$LOGOSCORE_BIN" load-module openmetrics
"$LOGOSCORE_BIN" call openmetrics start "{\"port\":$PORT,\"modules\":[\"libp2p_module\"]}"

wait_until "scraper on :$PORT" \
    curl -sSf --max-time 2 "http://127.0.0.1:$PORT/metrics"

out="$(curl -sS --max-time 15 "http://127.0.0.1:$PORT/metrics")"
echo "----- /metrics -----"
echo "$out"
echo "--------------------"

# openmetrics renders `_total` on samples but strips it from HELP/TYPE.
fail=0
need_line()   { grep -qxF -- "$1" <<<"$out" || { echo "FAIL: missing line: $1"   >&2; fail=1; }; }
need_substr() { grep -qF  -- "$1" <<<"$out" || { echo "FAIL: missing substr: $1" >&2; fail=1; }; }

need_line   '# TYPE libp2p_peers gauge'
need_substr 'libp2p_peers{module="libp2p_module"}'
need_line   '# TYPE libp2p_open_streams gauge'
need_substr 'libp2p_open_streams{module="libp2p_module"}'
need_line   '# TYPE libp2p_successful_dials counter'
need_substr 'libp2p_successful_dials_total{module="libp2p_module"}'
need_line   '# TYPE libp2p_failed_dials counter'
need_substr 'libp2p_failed_dials_total{module="libp2p_module"}'
need_line   '# TYPE libp2p_dial_attempts counter'
need_substr 'libp2p_dial_attempts_total{module="libp2p_module"}'
need_line   '# EOF'

if [[ "$fail" -ne 0 ]]; then
    echo "----- daemon log tail -----" >&2
    tail -n 50 logs.txt >&2 || true
    exit 1
fi
echo "PASS"
