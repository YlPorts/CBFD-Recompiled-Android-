#!/usr/bin/env python3
"""Prepare a build-local generated TU; do not modify or publish ROM-derived inputs."""
from pathlib import Path
import argparse,re
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('source',type=Path);p.add_argument('output',type=Path)
a=p.parse_args()
s=a.source.read_text()
pattern=r'RECOMP_FUNC void func_15123508\(uint8_t\* rdram, recomp_context\* ctx\)'
s,n=re.subn(pattern,'RECOMP_FUNC void conker_original_camera_input(uint8_t* rdram, recomp_context* ctx)',s)
if n!=1:raise SystemExit('Expected exactly one USA camera-input definition; refusing unknown generated code.')
a.output.parent.mkdir(parents=True,exist_ok=True)
if not a.output.exists() or a.output.read_text()!=s:a.output.write_text(s)
