"""Indirect-call targets the game reached that are not translated functions.

Reads "[ICALL] Failed to resolve VA 0x..." from ygo-apex/runs/last_err.txt
and adds each one inside a code section to analysis/semillas_runtime.json
(cumulative: earlier entries are kept). tools/regenerar.ps1 passes every
analysis/semillas_*.json to tools.disasm --seed-functions.

    py -3 tools/semillas_runtime.py
"""
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
LOG = ROOT / "ygo-apex" / "runs" / "last_err.txt"
OUT = ROOT / "analysis" / "semillas_runtime.json"
CODE_LO, CODE_HI = 0x00011000, 0x00423080      # .text .. XPP (below .rdata)


def main():
    have = {}
    if OUT.exists():
        for e in json.loads(OUT.read_text(encoding="utf-8")):
            have[int(e["start"], 16)] = e
    new = 0
    text = LOG.read_text(encoding="utf-8", errors="replace") if LOG.exists() else ""
    for m in re.finditer(r"Failed to resolve VA 0x([0-9A-Fa-f]{8}), call returns to 0x([0-9A-Fa-f]{8})", text):
        v, site = int(m.group(1), 16), m.group(2)
        if CODE_LO <= v < CODE_HI and v not in have:
            have[v] = {"start": "0x%08X" % v, "seen_from": "0x" + site.upper()}
            new += 1
    OUT.write_text(json.dumps([have[k] for k in sorted(have)], indent=1), encoding="utf-8")
    print("semillas_runtime: %d new, %d total" % (new, len(have)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
