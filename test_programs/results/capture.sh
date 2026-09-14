#!/usr/bin/env bash
# Capture the reliability-test receiver (tube) output into a CSV file.
#
# Reads the serial port directly via pyserial (NOT `pio device monitor`, whose
# interactive miniterm console cannot run in the background / redirected to a
# file). This produces a clean CSV without the monitor banner.
#
# Usage:  capture.sh <seconds> <output.csv> [serial-port]
#   packet loss (5 min):   ./capture.sh 300   raw/packetloss_ideal.csv
#   long-term (10 h):      ./capture.sh 36000 raw/longterm_10h.csv
#   explicit port:         ./capture.sh 300   raw/x.csv /dev/cu.usbmodem1101
#
# <output.csv> is relative to this script's directory (test_programs/results).
# The firmware runs indefinitely; this script bounds the recording on the host
# side and stops after <seconds>.
set -euo pipefail

DURATION="${1:?usage: capture.sh <seconds> <output.csv> [serial-port]}"
OUT="${2:?usage: capture.sh <seconds> <output.csv> [serial-port]}"
PORT="${3:-}"   # optional; auto-detected if empty

# Locate a Python with pyserial (PlatformIO ships one; fall back to system python3).
PYTHON=""
for cand in "$HOME/.platformio/penv/bin/python" "$(command -v python3 || true)"; do
    [ -n "$cand" ] && [ -x "$cand" ] && PYTHON="$cand" && break
done
[ -z "$PYTHON" ] && { echo "Error: no python with pyserial found." >&2; exit 1; }

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"   # test_programs/results
OUT_ABS="$HERE/$OUT"
mkdir -p "$(dirname "$OUT_ABS")"

echo "Capturing up to ${DURATION}s -> $OUT"

"$PYTHON" - "$DURATION" "$OUT_ABS" "$PORT" <<'PY'
import sys, time
import serial
from serial.tools import list_ports

duration = float(sys.argv[1])
out_path = sys.argv[2]
port     = sys.argv[3] if len(sys.argv) > 3 and sys.argv[3] else None

def find_port():
    cands = list(list_ports.comports())
    # Prefer Espressif's USB vendor id (0x303A), then typical USB-serial names.
    for p in cands:
        if (p.vid == 0x303A) or ('usbmodem' in p.device) or ('usbserial' in p.device):
            return p.device
    return cands[0].device if cands else None

if not port:
    port = find_port()
if not port:
    sys.exit("No serial port found; pass it as the 3rd argument.")

print(f"# port={port} baud=115200")
ser = serial.Serial()
ser.port = port
ser.baudrate = 115200
ser.timeout = 1
ser.open()

# Best-effort reset so the one-time CSV header (printed in setup) is captured.
# Works on boards with a DTR/RTS reset circuit. On the S3's native USB this may
# do nothing - then simply press the RESET button on the tube to start a run.
try:
    ser.dtr = False   # IO0 high -> normal boot
    ser.rts = True    # EN low   -> assert reset
    time.sleep(0.1)
    ser.rts = False   # release reset
    time.sleep(0.05)
except Exception:
    pass
ser.reset_input_buffer()

print("# waiting for data... (press RESET on the tube if nothing appears)")
end = time.time() + duration
with open(out_path, "w") as f:
    while time.time() < end:
        raw = ser.readline()
        if not raw:
            continue
        line = raw.decode("utf-8", "replace")
        f.write(line)
        f.flush()
        sys.stdout.write(line)   # echo so progress is visible
        sys.stdout.flush()
ser.close()
print(f"\n# done -> {out_path}")
PY
