#!/bin/sh
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
mkdir -p build

PORT=18443
RECEIVED="$ROOT/build/received_tls_bodies.txt"
rm -f "$RECEIVED"
CERTDIR="$ROOT/tools/test_certs"

echo "== starting local test TLS server on 127.0.0.1:$PORT (self-signed cert) =="
python3 "$ROOT/tools/fake_tls_server.py" "$PORT" "$CERTDIR/server.crt" "$CERTDIR/server.key" "$RECEIVED" &
SERVER_PID=$!
trap 'kill $SERVER_PID 2>/dev/null' EXIT
sleep 0.7

echo "== building https_client test tool =="
gcc -std=gnu11 -Wall -Wextra -O2 \
    -I"$ROOT/../vendor/mbedtls/include" \
    -o build/test_https_client tools/test_https_client.c src/https_client.c \
    -L"$ROOT/../vendor/mbedtls/library" -lmbedtls -lmbedx509 -lmbedcrypto

echo "== test 1: successful POST with the correct CA pinned =="
./build/test_https_client "https://localhost:$PORT/report" '{"hello":"tls"}' "$CERTDIR/server.crt"
if ! grep -q '"hello":"tls"' "$RECEIVED"; then
    echo "FAIL: server did not receive the expected body"
    exit 1
fi

echo "== test 2: wrong CA pinned MUST be rejected (proves verification is not a no-op) =="
if ./build/test_https_client "https://localhost:$PORT/report" '{"x":1}' "$CERTDIR/wrong.crt"; then
    echo "FAIL: connection succeeded with the wrong CA - certificate verification is not working"
    exit 1
fi

echo "== test 3: no ca_cert_path configured MUST refuse to connect at all =="
if ./build/test_https_client "https://localhost:$PORT/report" '{"x":1}' ""; then
    echo "FAIL: connected without any CA configured - this must never succeed"
    exit 1
fi

echo "== test 4: http:// URL fed to https_post MUST fail cleanly (wrong function for that scheme) =="
if ./build/test_https_client "http://localhost:$PORT/report" '{"x":1}' "$CERTDIR/server.crt"; then
    echo "FAIL: https_post should not accept a plain http:// URL"
    exit 1
fi

echo "== test 5: full daemon, test_mode=0, https server_urls + ca_cert_path =="
make -C "$ROOT" >/dev/null
export PATH="$ROOT/tools/mock-bin:$PATH"

WORKDIR="$ROOT/build/testrun_tls"
rm -rf "$WORKDIR"
mkdir -p "$WORKDIR"
LIVE_LOG="$WORKDIR/logs/daemon.log"
cat > "$WORKDIR/config.txt" <<EOF
log_path=$LIVE_LOG
baseline_path=$WORKDIR/sim_baseline.txt
test_mode=0
poll_interval_sec=1
server_urls=https://localhost:$PORT/report
ca_cert_path=$CERTDIR/server.crt
EOF

( cd "$WORKDIR" \
  && MOCK_SIM_OPERATOR=43408 "$ROOT/build/guardd" --config config.txt --once \
  && MOCK_SIM_OPERATOR=77777 "$ROOT/build/guardd" --config config.txt --once )

if ! grep -q "\[LIVE\] reported to https://localhost:$PORT/report (HTTP 200)" "$LIVE_LOG"; then
    echo "FAIL: daemon log does not show a successful live TLS report"
    cat "$LIVE_LOG"
    exit 1
fi
if ! grep -q '"current":"77777"' "$RECEIVED"; then
    echo "FAIL: TLS server did not receive the live SIM-changed report body"
    exit 1
fi

echo "OK: all https_client assertions passed"
