#!/bin/sh
# For developers: regenerates the recompiler's committed inputs from the decompilation,
# then recompiles. Run from the repo root on Linux or in WSL, after building the
# decompilation (make -C conker/conker) and N64Recomp:
#
#   sh recomp/run.sh
#
# It updates recomp/conker.us.syms.toml, recomp/code_rewrites.txt and mods/syms/, which
# are committed: players build from those and their ROM alone (recomp/recompile.py).
# Commit them whenever this changes them.
set -e
# prepare_elf.py sizes the decomp's functions for N64Recomp, and overlays the code
# sections with the original game's bytes (conker/assets/*.us.bin), so decomp functions
# that don't match can't change the recompiled game's behaviour.
python3 recomp/prepare_elf.py conker/conker/build/conker.us.elf recomp/conker.us.recomp.elf \
    tools/N64Recomp/src/symbol_lists.cpp \
    --original .init=conker/assets/init.us.bin \
    --original .game=conker/assets/game.us.bin \
    --original .debugger=conker/assets/debugger.us.bin
# The prepared ELF's functions and data symbols, as N64Recomp sees them (the same
# configuration, reading the ELF instead of the symbols file).
mkdir -p recomp/build
sed -e 's|^symbols_file_path = .*|elf_path = "recomp/conker.us.recomp.elf"|' -e '/^rom_file_path = /d' \
    conker.toml > recomp/build/conker.elf.toml
# Paths in a config are relative to it: run from the repo root with the config there.
cp recomp/build/conker.elf.toml conker.elf.toml
./tools/N64Recomp/build/N64Recomp conker.elf.toml --dump-context > /dev/null
rm conker.elf.toml
python3 recomp/unpack_rom.py conker/baserom.us.z64 recomp/build/conker.us.original.bin
python3 recomp/make_syms.py recomp/conker.us.recomp.elf dump.toml recomp/build/conker.us.original.bin \
    recomp/conker.us.syms.toml recomp/code_rewrites.txt
# Symbol files for mods (RecompModTool): they must match this build exactly.
mkdir -p mods/syms
mv dump.toml mods/syms/conker.us.syms.toml
mv data_dump.toml mods/syms/conker.us.datasyms.toml
python3 recomp/recompile.py
