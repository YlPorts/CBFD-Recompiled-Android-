#!/usr/bin/env python3
"""Production render policy versus independent ordered-fragment reference; no ROM/GPU."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='conker-010-') as folder:
    output = Path(folder)/'render'
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++20', '-O2', '-Wall', '-Wextra',
        '-Werror', '-fsanitize=undefined', '-fno-sanitize-recover=all',
        '-I', str(ROOT/'android/native'), str(ROOT/'android/tests/render_010_test.cpp'),
        '-o', str(output)], check=True)
    subprocess.run([str(output)], check=True)

# Guard that the APK's render queue actually calls the policy exercised above.
queue = (ROOT/'tools/rt64/src/hle/rt64_workload_queue.cpp').read_text()
renderer = (ROOT/'tools/rt64/src/render/rt64_framebuffer_renderer.cpp').read_text()
shader = (ROOT/'tools/rt64/src/render/rt64_raster_shader.cpp').read_text()
assert 'resolutionMultiplier = conker::mobile::resolution_scale(viFbSize[1])' in queue
assert 'conker::mobile::coverage_batch(triangles.faceCount,' in renderer
assert 'updatesDepth, otherMode.zCmp() || depthDecal' in renderer
assert 'split.pixelShader = singleAlphaShader.get()' in shader
assert 'if (splitColor)' in shader and 'colorOnly || respv::Optimizer::run' in shader
print('PASS actual queue, coverage replay and shader-selection integration guards')
