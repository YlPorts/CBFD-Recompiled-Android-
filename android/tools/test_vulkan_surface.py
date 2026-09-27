#!/usr/bin/env python3
"""Test only the Android Vulkan WSI policy layered on top of PC 0.1.2."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

R=Path(__file__).resolve().parents[2]

def main():
    compiler=os.environ.get("CXX","c++")
    if not shutil.which(compiler):
        raise SystemExit("C++17 compiler required")
    includes=R/"tools/rt64/src/contrib/plume/contrib/Vulkan-Headers/include"
    if not (includes/"vulkan/vulkan.h").is_file():
        raise SystemExit("Pinned Plume Vulkan-Headers submodule required")

    with tempfile.TemporaryDirectory(prefix="conker-wsi-pc012-") as tmp:
        exe=Path(tmp)/"surface-policy"
        subprocess.run([
            compiler,"-std=c++17","-O1","-g","-Wall","-Wextra","-Werror",
            "-fsanitize=undefined","-fno-sanitize-recover=all",
            f"-I{includes}",str(R/"android/tests/vulkan_surface_test.cpp"),"-o",str(exe)
        ],check=True)
        subprocess.run([str(exe)],check=True)

    plume=R/"tools/rt64/src/contrib/plume/plume_vulkan.cpp"
    plume_h=R/"tools/rt64/src/contrib/plume/plume_vulkan.h"
    app=R/"tools/rt64/src/hle/rt64_application.cpp"
    shader=R/"tools/rt64/src/render/rt64_shader_library.cpp"
    interface=R/"tools/rt64/src/contrib/plume/plume_render_interface.h"

    source=plume.read_text()
    header=plume_h.read_text()
    app_text=app.read_text()
    shader_text=shader.read_text()
    interface_text=interface.read_text()

    checks=[
        ('wsi::choose_format' in source,"Android surface format negotiation missing"),
        ('wsi::choose_settings' in source,"Android swapchain capabilities policy missing"),
        ('if (vk == VK_NULL_HANDLE || textureIndex >= textures.size()) return false;' in source,
         "Android present stale-handle guard missing"),
        ('if (vk == VK_NULL_HANDLE || textures.empty() || !signalSemaphore || !textureIndex) return false;' in source,
         "Android acquire stale-handle guard missing"),
        ('bool surfaceReady = false;' in header,"Android deferred surface state missing"),
        ('virtual RenderFormat getFormat() const = 0;' in interface_text,"actual swapchain format contract missing"),
        ('!swapChain->resize()' in app_text,"RT64 must validate Android presentation surface before shaders"),
        ('shaderLibrary->swapChainFormat = swapChain->getFormat();' in app_text,
         "RT64 shader library does not receive negotiated Android format"),
        ('pipelineDesc.renderTargetFormat[0] = swapChainFormat;' in shader_text,
         "VI pipeline still assumes desktop BGRA"),
    ]
    for ok,label in checks:
        if not ok:
            raise AssertionError(label)

    if app_text.index('!swapChain->resize()') > app_text.index('shaderLibrary->setupCommonShaders'):
        raise AssertionError("Android surface validation occurs after shader setup")

    forbidden=("surfaceRegistry","surfaceRetryAfter","maliG57Compat","dualSrcBlendEffective",
               "mobile_metrics","mobile_render","texture-capture")
    combined="\n".join((source,header,app_text,shader_text))
    for marker in forbidden:
        if marker in combined:
            raise AssertionError(f"obsolete Android render/lifecycle experiment still active: {marker}")

    print(f"PASS: {len(checks)+2} direct-port Vulkan WSI integration checks; no GPU/gameplay claim")

if __name__=="__main__":
    main()
