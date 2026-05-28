#!/usr/bin/env python3
"""Generate WORDLELX.DAT — 5-bit-packed binary word list for Wordle LX.

Usage:
    python3 make_dat.py [output_path]

Fetches the official Wordle word lists from cfreshman's GitHub Gists:
  Answers: https://gist.github.com/cfreshman/a03ef2cba789d8cf00c08f767e0fad7b
  Guesses: https://gist.github.com/cfreshman/cdcdf777450c5b5301e439061d29694c

DAT file format (magic "WRD5"):
  [0-3]  magic "WRD5"
  [4-5]  answer_count  (uint16 little-endian)
  [6-7]  guess_count   (uint16 little-endian)
  [8..]  answer words  5 bits/letter, MSB-first bitstream, padded to byte boundary
  [...]  guess words   same encoding, independent byte boundary

Each letter A-Z is encoded as 0-25.  Padding bits at the end of each section
are zero.  The two sections are byte-aligned independently so the decoder can
fseek directly to the guess section.

Compression: ~5 bytes/word (raw) -> 3.125 bytes/word (packed) = ~37% smaller.
"""

import sys
import struct
import json
import urllib.request
import os

ANSWER_GIST = "a03ef2cba789d8cf00c08f767e0fad7b"
GUESS_GIST  = "cdcdf777450c5b5301e439061d29694c"


def fetch_gist_words(gist_id):
    url = "https://api.github.com/gists/" + gist_id
    req = urllib.request.Request(url, headers={"User-Agent": "wordlelx-make-dat"})
    with urllib.request.urlopen(req) as r:
        data = json.loads(r.read())
    content = list(data["files"].values())[0]["content"]
    return sorted(set(
        w.strip().upper() for w in content.split("\n")
        if len(w.strip()) == 5 and w.strip().isalpha()
    ))


def pack_words(words):
    """Pack list of 5-letter uppercase words into 5-bit/letter bitstream."""
    bits = []
    for w in words:
        for ch in w:
            val = ord(ch) - ord('A')
            for i in range(4, -1, -1):   # MSB first
                bits.append((val >> i) & 1)
    while len(bits) % 8:                 # pad to byte boundary
        bits.append(0)
    data = bytearray()
    for i in range(0, len(bits), 8):
        byte = 0
        for j in range(8):
            byte = (byte << 1) | bits[i + j]
        data.append(byte)
    return bytes(data)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "WORDLELX.DAT"

    print("Fetching answer list...")
    answers = fetch_gist_words(ANSWER_GIST)
    print("  {} answers".format(len(answers)))

    print("Fetching guess list...")
    guesses = fetch_gist_words(GUESS_GIST)
    print("  {} guesses".format(len(guesses)))

    ans_packed   = pack_words(answers)
    guess_packed = pack_words(guesses)

    with open(out, "wb") as f:
        f.write(b"WRD5")
        f.write(struct.pack("<H", len(answers)))
        f.write(struct.pack("<H", len(guesses)))
        f.write(ans_packed)
        f.write(guess_packed)

    size = os.path.getsize(out)
    raw  = len(answers) * 5 + len(guesses) * 5
    print("Written: {} ({} bytes, vs {} bytes uncompressed, {:.0f}% savings)".format(
        out, size, raw + 8, 100.0 * (1.0 - size / (raw + 8))))


if __name__ == "__main__":
    main()
