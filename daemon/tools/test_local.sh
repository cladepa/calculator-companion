#!/bin/sh
# Local smoke test loop: builds the daemon and runs it against a fake
# getprop, exercising the baseline-adoption and SIM-change-detection paths
# without any real Android device. Run from daemon/.
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORKDIR="$ROOT/build/testrun"
export PATH="$ROOT/tools/mock-bin:$PATH"

echo "== building =="
make -C "$ROOT" >/dev/null

rm -rf "$WORKDIR"
mkdir -p "$WORKDIR"
cd "$WORKDIR"

LOG="$WORKDIR/logs/daemon.log"
cat > config.txt <<EOF
log_path=$LOG
baseline_path=$WORKDIR/sim_baseline.txt
test_mode=1
poll_interval_sec=1
server_urls=https://server1.example.test/report, https://server2.example.test/report
EOF

echo "== run 1: no baseline yet, should adopt current SIM (43408) =="
MOCK_SIM_OPERATOR=43408 "$ROOT/build/guardd" --config config.txt --once

echo "== run 2: same SIM, should NOT log a SIM CHANGE =="
MOCK_SIM_OPERATOR=43408 "$ROOT/build/guardd" --config config.txt --once

echo "== run 3: different SIM, SHOULD log a SIM CHANGE (test_mode, no real action) =="
MOCK_SIM_OPERATOR=99999 "$ROOT/build/guardd" --config config.txt --once

echo "== run 4: different SIM again, now also roaming =="
MOCK_SIM_OPERATOR=11111 MOCK_SIM_ISROAMING=true,false "$ROOT/build/guardd" --config config.txt --once

echo
echo "== resulting log =="
cat "$LOG"

echo
echo "== assertions =="
CHANGE_COUNT=$(grep -c "SIM CHANGED" "$LOG")
if [ "$CHANGE_COUNT" -ne 2 ]; then
    echo "FAIL: expected exactly 2 'SIM CHANGED' lines, got $CHANGE_COUNT"
    exit 1
fi
if ! grep -q "adopting current SIM as baseline" "$LOG"; then
    echo "FAIL: baseline adoption message missing"
    exit 1
fi
if ! grep -q "roaming=1" "$LOG"; then
    echo "FAIL: roaming flag was not detected/logged on run 4"
    exit 1
fi
REPORT_COUNT=$(grep -c "would POST to" "$LOG")
if [ "$REPORT_COUNT" -ne 4 ]; then
    echo "FAIL: expected 4 'would POST to' lines (2 SIM changes x 2 servers), got $REPORT_COUNT"
    exit 1
fi
if ! grep -q "server1.example.test" "$LOG" || ! grep -q "server2.example.test" "$LOG"; then
    echo "FAIL: not all configured servers appear in the report log"
    exit 1
fi

echo "OK: all assertions passed"
