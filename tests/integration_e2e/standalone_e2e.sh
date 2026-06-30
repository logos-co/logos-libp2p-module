#!/usr/bin/env bash
# Runs libp2p_module standalone under logoscore and asserts the standalone
# commands (createNode / start / getNodeInfo) and their outputs.
# Required env: LIBP2P_LGX_DIR
# Optional env: LOGOSCORE_BIN (default: logoscore), LGPM_BIN (default: lgpm)

set -euo pipefail
shopt -s nullglob

[[ -n "${LIBP2P_LGX_DIR:-}" ]] || { echo "missing env: LIBP2P_LGX_DIR" >&2; exit 1; }
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

WORK="$(mktemp -d)"
DAEMON_PID=""
PORT=$(( 49152 + RANDOM % 16384 ))

cleanup() {
    local rc=$?
    if [[ "$rc" -ne 0 && -f "$WORK/logs.txt" ]]; then
        echo "----- daemon log tail -----" >&2
        tail -n 200 "$WORK/logs.txt" >&2 || true
    fi
    if [[ -n "$DAEMON_PID" ]]; then
        "$LOGOSCORE_BIN" stop >/dev/null 2>&1 || true
        kill "$DAEMON_PID" 2>/dev/null || true
        wait "$DAEMON_PID" 2>/dev/null || true
    fi
    rm -rf "$WORK"
}
trap cleanup EXIT

dump_daemon_log() {
    echo "----- daemon log tail -----" >&2
    tail -n 80 "$WORK/logs.txt" >&2 2>/dev/null || true
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
    dump_daemon_log
    return 1
}

cd "$WORK"
mkdir -p modules

"$LGPM_BIN" --modules-dir ./modules --allow-unsigned install --file "$LIBP2P_LGX"
"$LGPM_BIN" --modules-dir ./modules list

"$LOGOSCORE_BIN" -D -m ./modules > logs.txt 2>&1 &
DAEMON_PID=$!

wait_until "logoscore daemon ready" \
    "$LOGOSCORE_BIN" load-module libp2p_module

# `call` exits 0 even when the module reports failure; the outcome lives in
# the wrapped `.result.{success,value}`, not the process exit code.
call() { "$LOGOSCORE_BIN" call libp2p_module "$@"; }
info() { call getNodeInfo "$1" | jq -r '.result.value // empty'; }
rejected() { [[ "$(call "$@" | jq -r '.result.success')" == "false" ]]; }

fail=0
check() {
    local desc="$1" got="$2" want="$3"
    if [[ "$got" == "$want" ]]; then
        echo "ok: $desc ($got)"
    else
        echo "FAIL: $desc: got [$got] want [$want]" >&2
        fail=1
    fi
}
check_nonempty() {
    local desc="$1" got="$2"
    if [[ -n "$got" ]]; then
        echo "ok: $desc ($got)"
    else
        echo "FAIL: $desc empty" >&2
        fail=1
    fi
}

echo "----- createNode (bind tcp/$PORT) + start -----"
call createNode "{\"addrs\":[\"/ip4/127.0.0.1/tcp/$PORT\"]}"
call start

echo "----- getNodeInfo outputs -----"
check        "getNodeInfo Version"      "$(info Version)"      "1.0.0"
check        "getNodeInfo MyBoundPorts" "$(call getNodeInfo MyBoundPorts | jq -r '.result.value[0] // empty')" "$PORT"
check_nonempty "getNodeInfo PeerId"     "$(info PeerId)"
check_nonempty "getNodeInfo Multiaddrs" "$(call getNodeInfo Multiaddrs | jq -r '.result.value[0] // empty')"

echo "----- unknown getNodeInfo field is rejected -----"
if rejected getNodeInfo Nonexistent; then
    echo "ok: getNodeInfo Nonexistent rejected"
else
    echo "FAIL: getNodeInfo Nonexistent should fail" >&2
    fail=1
fi

echo "----- invalid createNode config is rejected and logged -----"
if rejected createNode '{not valid json'; then
    echo "ok: createNode malformed JSON rejected"
else
    echo "FAIL: createNode with malformed JSON should fail" >&2
    fail=1
fi
if grep -q "createNode: invalid config" logs.txt; then
    echo "ok: daemon logged the createNode failure reason"
else
    echo "FAIL: daemon log missing createNode failure reason" >&2
    fail=1
fi

# The node rejected the bad config before tearing down, so it stays up.
check "getNodeInfo Version after rejected reconfigure" "$(info Version)" "1.0.0"

call stop

if [[ "$fail" -ne 0 ]]; then
    exit 1
fi
echo "PASS"
