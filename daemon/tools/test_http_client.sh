#!/bin/sh
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
mkdir -p build

PORT=18080
RECEIVED="$ROOT/build/received_bodies.txt"
rm -f "$RECEIVED"

echo "== starting local test HTTP server on 127.0.0.1:$PORT =="
python3 "$ROOT/tools/fake_server.py" "$PORT" "$RECEIVED" &
SERVER_PID=$!
trap 'kill $SERVER_PID 2>/dev/null' EXIT
sleep 0.5

echo "== building http_client test tool =="
gcc -std=gnu11 -Wall -Wextra -O2 -o build/test_http_client tools/test_http_client.c src/http_client.c

echo "== test 1: successful POST to a running server =="
./build/test_http_client "http://127.0.0.1:$PORT/report" '{"hello":"world"}'
if ! grep -q '"hello":"world"' "$RECEIVED"; then
    echo "FAIL: server did not receive the expected body"
    exit 1
fi

echo "== test 2: POST to a closed port must fail promptly, not hang =="
START=$(date +%s)
if ./build/test_http_client "http://127.0.0.1:1/report" '{"x":1}'; then
    echo "FAIL: expected failure connecting to a closed port"
    exit 1
fi
END=$(date +%s)
ELAPSED=$((END - START))
if [ "$ELAPSED" -gt 15 ]; then
    echo "FAIL: took ${ELAPSED}s to fail on a closed port - timeout logic looks broken"
    exit 1
fi

echo "== test 3: unsupported scheme (https) must fail cleanly, not crash =="
if ./build/test_http_client "https://example.test/report" '{"x":1}'; then
    echo "FAIL: https should not be supported yet (that is a separate follow-up step)"
    exit 1
fi

echo "== test 4: full daemon, test_mode=0, reporting a real SIM change to the local server =="
make -C "$ROOT" >/dev/null
export PATH="$ROOT/tools/mock-bin:$PATH"

WORKDIR="$ROOT/build/testrun_live"
rm -rf "$WORKDIR"
mkdir -p "$WORKDIR"
LIVE_LOG="$WORKDIR/logs/daemon.log"
cat > "$WORKDIR/config.txt" <<EOF
log_path=$LIVE_LOG
baseline_path=$WORKDIR/sim_baseline.txt
test_mode=0
poll_interval_sec=1
server_urls=http://127.0.0.1:$PORT/report
EOF

( cd "$WORKDIR" \
  && MOCK_SIM_OPERATOR=43408 "$ROOT/build/guardd" --config config.txt --once \
  && MOCK_SIM_OPERATOR=55555 "$ROOT/build/guardd" --config config.txt --once )

if ! grep -q "\[LIVE\] reported to http://127.0.0.1:$PORT/report (HTTP 200)" "$LIVE_LOG"; then
    echo "FAIL: daemon log does not show a successful live report"
    cat "$LIVE_LOG"
    exit 1
fi
if ! grep -q '"current":"55555"' "$RECEIVED"; then
    echo "FAIL: fake server did not receive the live SIM-changed report body"
    exit 1
fi

echo "OK: all http_client assertions passed"
