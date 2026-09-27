#!/usr/bin/env python3
"""
Unpacks the code of Conker's Bad Fur Day (US) from the ROM into the flat image
N64Recomp reads: the header, boot code and .init as they are, then .game
decompressed (its code and its data), then .debugger. It's the same image the
decompilation's extraction produces (conker/conker/conker.us.bin), without its
Python packages or tools: only the standard library.

The ROM stores .game compressed with Rare's "rzip": at ROM 0x42450, a table of
block offsets (XORed with 0x8039CCCA; the first word is skipped, a 0 ends it),
each block being a 4-byte length followed by a raw deflate stream of one page of
code, and at 0x145ED8 into it the data, compressed the same way as one block.

Then the code rewrites the recompiler needs (recomp/code_rewrites.txt: ROM
address and new word, from recomp/prepare_elf.py) are applied.

Usage: unpack_rom.py <baserom.us.z64> <out.bin> [code_rewrites.txt]
"""
import hashlib
import struct
import sys
import zlib

ROM_SHA1 = "4cbadd3c4e0729dec46af64ad018050eada4f47a"
IMAGE_SHA1 = "842e3d348e3c8ae0039e2ab367ad492f9b5266d8"  # conker/conker/conker.us.sha1

INIT_END = 0x2D4B0             # header, boot and .init end here
GAME_RZIP = (0x42450, 0x19EA88)  # compressed .game in the ROM
GAME_CODE_END = 0x144700       # end of the code blocks, in the compressed .game
GAME_DATA = (0x145ED8, 0x15A388)
GAME_XOR = 0x8039CCCA
DEBUGGER = (0x19EA88, 0x1A37E0)


def rareunzip(block):
    # A 4-byte uncompressed length, then a raw deflate stream.
    return zlib.decompressobj(wbits=-15).decompress(block[4:])


def unpack(rom):
    if hashlib.sha1(rom).hexdigest() != ROM_SHA1:
        sys.exit("unpack_rom: this isn't the US ROM of Conker's Bad Fur Day in big-endian .z64 "
                 f"format (SHA-1 {ROM_SHA1})")
    game = rom[GAME_RZIP[0]:GAME_RZIP[1]]
    starts = []
    index = 1
    while True:
        word, = struct.unpack_from(">I", game, index * 4)
        index += 1
        if word == 0:
            break
        starts.append(word ^ GAME_XOR)
    code = b"".join(rareunzip(game[a:b]) for a, b in zip(starts, starts[1:]))
    data = rareunzip(game[GAME_DATA[0]:GAME_DATA[1]])
    image = rom[:INIT_END] + code + data + rom[DEBUGGER[0]:DEBUGGER[1]]
    if hashlib.sha1(image).hexdigest() != IMAGE_SHA1:
        sys.exit("unpack_rom: the unpacked code doesn't have the expected SHA-1")
    return bytearray(image)


def apply_rewrites(image, path):
    count = 0
    for line in open(path, encoding="utf-8"):
        line = line.split("#")[0].strip()
        if not line:
            continue
        addr, word = (int(x, 16) for x in line.split()[:2])
        image[addr:addr + 4] = struct.pack(">I", word)
        count += 1
    return count


def main(rom_path, out_path, rewrites_path=None):
    image = unpack(open(rom_path, "rb").read())
    if rewrites_path:
        apply_rewrites(image, rewrites_path)
    open(out_path, "wb").write(image)


if __name__ == "__main__":
    if len(sys.argv) not in (3, 4):
        sys.exit(__doc__)
    main(*sys.argv[1:])
