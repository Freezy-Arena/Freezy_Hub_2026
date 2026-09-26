"""Check captured firmware timing lines; maxima are cumulative since boot."""
import argparse
import re
from pathlib import Path


def check(text, sample_us=10000, ws_ms=100):
    failures = []
    fms_lines = [line for line in text.splitlines() if "[FMS TIMING]" in line]
    ws_lines = [line for line in text.splitlines() if "[WS TIMING]" in line]
    if not fms_lines or not ws_lines:
        failures.append("Missing FMS or WebSocket timing diagnostics")
    for line in fms_lines:
        fields = dict(re.findall(r"(\w+)=(-?\d+)", line))
        for name, limit in (("sample_gap_us", sample_us), ("unretained", 0), ("FAULT", 0)):
            if name not in fields or int(fields[name]) > limit:
                failures.append(f"{name} exceeds {limit} or is missing: {line}")
    for line in ws_lines:
        fields = dict(re.findall(r"(\w+)=(-?\d+)", line))
        if "gap_ms" not in fields or int(fields["gap_ms"]) > ws_ms:
            failures.append(f"WebSocket gap exceeds {ws_ms} ms: {line}")
    return failures


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--sample-us", type=int, default=10000)
    parser.add_argument("--ws-ms", type=int, default=100)
    args = parser.parse_args()
    errors = check(args.capture.read_text(encoding="utf-8", errors="replace"), args.sample_us, args.ws_ms)
    print("\n".join(errors) if errors else "Timing criteria passed; verify delivery order against GPIO and mock-arena logs separately.")
    raise SystemExit(bool(errors))
