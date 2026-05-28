#!/usr/bin/env python3
"""Convert Open Watcom linker MAP format into a minimal MS LINK-style MAP.

E2M 4.2 expects classic Microsoft LINK map headings and fields.
This tool derives equivalent data from Watcom map output.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path

GROUP_RE = re.compile(r"^DGROUP\s+([0-9A-Fa-f]{4}):([0-9A-Fa-f]{4})\s+[0-9A-Fa-f]{8}\s*$")
SEG_RE = re.compile(
    r"^(\S+)\s+(\S+)\s+(\S+)\s+([0-9A-Fa-f]{4}):([0-9A-Fa-f]{4})\s+([0-9A-Fa-f]{8})\s*$"
)
SYM_RE = re.compile(r"^([0-9A-Fa-f]{4}):([0-9A-Fa-f]{4})[+*]?\s+(\S+)\s*$")


def parse_wlink_map(lines: list[str]):
    dgroup = None
    segments = []
    symbols = []

    in_groups = False
    in_segments = False
    in_symbols = False

    for raw in lines:
        line = raw.rstrip("\n")
        striped = line.strip()

        if striped == "|   Groups   |":
            in_groups = True
            in_segments = False
            in_symbols = False
            continue
        if striped == "|   Segments   |":
            in_groups = False
            in_segments = True
            in_symbols = False
            continue
        if striped == "Address        Symbol":
            in_groups = False
            in_segments = False
            in_symbols = True
            continue

        if in_groups:
            m = GROUP_RE.match(striped)
            if m:
                dgroup = (int(m.group(1), 16), int(m.group(2), 16))
                continue

        if in_segments:
            m = SEG_RE.match(striped)
            if not m:
                continue
            name, cls, grp, seg, off, size = m.groups()
            seg_i = int(seg, 16)
            off_i = int(off, 16)
            size_i = int(size, 16)
            start = (seg_i << 4) + off_i
            stop = start + (size_i - 1 if size_i > 0 else 0)
            segments.append(
                {
                    "name": name,
                    "class": cls,
                    "group": grp,
                    "seg": seg_i,
                    "off": off_i,
                    "start": start,
                    "stop": stop,
                    "length": size_i,
                }
            )
            continue

        if in_symbols:
            m = SYM_RE.match(striped)
            if m:
                seg, off, sym = m.groups()
                symbols.append((int(seg, 16), int(off, 16), sym))

    return dgroup, segments, symbols


def ensure_required_symbols(segments, symbols):
    names = {name for _, _, name in symbols}

    if "_DATA" not in names:
        data_seg = next((s for s in segments if s["name"] == "_DATA"), None)
        if data_seg:
            symbols.append((data_seg["seg"], data_seg["off"], "_DATA"))

    if "_INIT_" not in names:
        init_seg = next((s for s in segments if s["name"] == "XI"), None)
        if init_seg is None:
            init_seg = next((s for s in segments if s["name"] == "_DATA"), None)
        if init_seg:
            symbols.append((init_seg["seg"], init_seg["off"], "_INIT_"))


def write_ms_like_map(out_path: Path, dgroup, segments, symbols):
    with out_path.open("w", newline="\n") as f:
        f.write(" Microsoft (R) Overlay Linker 3.60\n")
        f.write("\n")

        f.write(" Origin   Group\n")
        if dgroup is not None:
            f.write(f" {dgroup[0]:04X}:{dgroup[1]:04X}   DGROUP\n")
        else:
            f.write(" 0000:0000   DGROUP\n")
        f.write("\n")

        f.write(" Start  Stop   Length Name               Class\n")
        for s in segments:
            f.write(
                f" {s['start']:05X}H {s['stop']:05X}H {s['length']:05X}H "
                f"{s['name']:<18} {s['class']}\n"
            )
        f.write("\n")

        f.write(" Address         Publics by Value\n")
        for seg, off, sym in symbols:
            f.write(f" {seg:04X}:{off:04X}   {sym}\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input_map")
    parser.add_argument("output_map")
    args = parser.parse_args()

    lines = Path(args.input_map).read_text(errors="replace").splitlines(True)
    dgroup, segments, symbols = parse_wlink_map(lines)

    if not segments:
        raise SystemExit("No segments found in input map; refusing to write output")

    ensure_required_symbols(segments, symbols)
    write_ms_like_map(Path(args.output_map), dgroup, segments, symbols)


if __name__ == "__main__":
    main()
