"""Function starts the disassembler missed, found in the data.

Every dword in .rdata/.data that points into a code section, is not a known
function start, and lands where a function can start (after int3/nop
padding, or a ret) becomes a seed for tools.disasm --seed-functions. These
are the vtable methods only ever reached through a vtable (Yu-Gi-Oh TDOD:
0x00197FD0, the memory manager's free, broke the boot).

    py -3 tools/semillas_vtables.py  -> analysis/semillas_vtables.json
"""
import json
import pathlib
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
XBE = ROOT / "xbe" / "default.xbe"
ANALYSIS = ROOT / "xbe" / "default_analysis.json"
FUNCS = ROOT / "analysis" / "disasm" / "functions.json"
OUT = ROOT / "analysis" / "semillas_vtables.json"

CODE = (".text", "D3D", "XNET", "XMV", "DSOUND", "D3DX", "XGRPH", "XPP")
DATA = (".rdata", ".data")


def main():
    x = XBE.read_bytes()
    an = json.loads(ANALYSIS.read_text(encoding="utf-8"))
    secs = an.get("sections") or an["xbe"]["sections"]

    def num(v):
        return int(v, 16) if isinstance(v, str) else int(v)

    sec = {}
    for s in secs:
        name = s.get("name")
        va = num(s["virtual_addr"])
        vs = num(s["virtual_size"])
        raw = num(s["raw_addr"])
        rs = num(s["raw_size"])
        sec[name] = (va, vs, raw, rs)

    def byte_at(va):
        for va0, vs, raw, rs in sec.values():
            if va0 <= va < va0 + min(vs, rs):
                return x[raw + va - va0]
        return None

    fj = json.loads(FUNCS.read_text(encoding="utf-8"))
    fl = fj if isinstance(fj, list) else fj.get("functions", [])
    starts, spans = set(), []
    for f in fl:
        s = num(f["start"])
        e = num(f.get("end", f["start"]))
        starts.add(s)
        spans.append((s, e))
    spans.sort()

    def inside(va):
        import bisect
        i = bisect.bisect_right(spans, (va, 0xFFFFFFFF)) - 1
        return i >= 0 and spans[i][0] < va < spans[i][1]

    def starts_here(v):
        prev = byte_at(v - 1)
        return prev in (0xCC, 0x90, 0xC3) or (byte_at(v - 3) == 0xC2 and prev == 0x00)

    code = [sec[n] for n in CODE if n in sec]
    seeds = {}
    for name in DATA:
        va0, vs, raw, rs = sec[name]
        for off in range(0, min(vs, rs) - 3, 4):
            v = struct.unpack_from("<I", x, raw + off)[0]
            if v in starts or not any(c[0] <= v < c[0] + c[1] for c in code):
                continue
            if inside(v) and not starts_here(v):
                continue                      # an interior label, not a start
            prev = byte_at(v - 1)
            first = byte_at(v)
            if prev in (0xCC, 0x90, 0xC3) or (prev == 0x00 and byte_at(v - 3) == 0xC2):
                if first not in (0xCC, 0x00):
                    seeds.setdefault(v, va0 + off)
    # Function addresses used as immediates in code (callbacks, SEH filters,
    # thread routines): the listing marks them "; XREF: 0x... (data_imm)"
    # just above the target. Yu-Gi-Oh TDOD: 0x002A8AA0, 0x00137570 ...
    import re
    asm_dir = ROOT / "analysis" / "disasm" / "asm"
    xref = re.compile(r";\s*XREF:.*\(data_imm\)")
    addr = re.compile(r"^\s+0x([0-9A-F]{8})\s")
    imm = 0
    for asm in sorted(asm_dir.glob("*.asm")):
        if asm.name.upper().startswith(("FONT_BIN", "DOLBY")):
            continue                          # data, not code
        pending = None
        with asm.open(encoding="utf-8", errors="replace") as fh:
            for line in fh:
                if xref.search(line):
                    m = re.search(r"XREF:\s*0x([0-9A-F]{8})", line)
                    pending = int(m.group(1), 16) if m else 0
                    continue
                m = addr.match(line)
                if m and pending is not None:
                    v = int(m.group(1), 16)
                    # Inside a detected body is fine when padding or a ret
                    # comes right before: the detector ran the previous
                    # function over its end (0x002A8AA0 after a nop).
                    if v not in starts and v not in seeds and (not inside(v) or starts_here(v)):
                        seeds[v] = pending
                        imm += 1
                    pending = None
    print("%d from data, %d from code immediates" % (len(seeds) - imm, imm))
    # Cumulative: a seed of an earlier pass is a function start in the next
    # functions.json, so it is not found again; dropping it from this file
    # would drop the function on the following regeneration (0x00197FD0 was
    # lost that way).
    if OUT.exists():
        for e in json.loads(OUT.read_text(encoding="utf-8")):
            seeds.setdefault(int(e["start"], 16), int(e.get("ref", "0x0"), 16))
    out = [{"start": "0x%08X" % v, "ref": "0x%08X" % r} for v, r in sorted(seeds.items())]
    OUT.write_text(json.dumps(out, indent=1), encoding="utf-8")
    print("%d seeds -> %s" % (len(out), OUT))
    for e in out[:15]:
        print("  ", e["start"], "from", e["ref"])
    return 0


if __name__ == "__main__":
    sys.exit(main())
