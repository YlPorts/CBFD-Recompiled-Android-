#!/usr/bin/env python3
"""Compile/test the exact shared presentation policy. No ROM or GPU is used."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
R = Path(__file__).resolve().parents[2]
def main():
    compiler = os.environ.get('CXX', 'c++')
    if not shutil.which(compiler): raise SystemExit('C++17 compiler required')
    includes = R/'tools/rt64/src/contrib/plume/contrib/Vulkan-Headers/include'
    if not (includes/'vulkan/vulkan.h').is_file(): raise SystemExit('Pinned Plume Vulkan-Headers submodule required')
    with tempfile.TemporaryDirectory(prefix='conker-wsi-') as tmp:
        exe=Path(tmp)/'surface-policy'
        subprocess.run([compiler,'-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all',f'-I{includes}',str(R/'android/tests/vulkan_surface_test.cpp'),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
        # Compile the exact patched upstream methods, not a rewritten swapchain implementation.
        plume = R/'tools/rt64/src/contrib/plume'
        def struct(text, name):
            start = text.index('    struct ' + name)
            opening = text.index('{', start)
            level = 1
            end = opening + 1
            while level:
                if text[end] == '{': level += 1
                if text[end] == '}': level -= 1
                end += 1
            return text[start:end+1]
        base = struct((plume/'plume_render_interface.h').read_text(), 'RenderSwapChain')
        derived = struct((plume/'plume_vulkan.h').read_text(), 'VulkanSwapChain')
        source = (plume/'plume_vulkan.cpp').read_text()
        methods = source[source.index('    VulkanSwapChain::VulkanSwapChain'):source.index('    // VulkanFramebuffer')]
        harness = Path(tmp)/'swapchain.cpp'
        harness.write_text('#include "vulkan_swapchain_fixture.hpp"\nnamespace plume {\n' + base + '\n' + derived + '\n' + methods + '\n}\n' + (R/'android/tests/vulkan_swapchain_mock_main.cpp').read_text())
        mocked = Path(tmp)/'swapchain-mocked'
        subprocess.run([compiler,'-std=c++17','-O1','-g','-Wall','-Wextra','-Wno-address','-fsanitize=undefined','-fno-sanitize-recover=all',f'-I{includes}',f'-I{R}/android/tests',f'-I{R}/android/tests/stubs',str(harness),'-o',str(mocked)],check=True)
        subprocess.run([str(mocked)],check=True)
        app = (R/'tools/rt64/src/hle/rt64_application.cpp').read_text()
        shaders = (R/'tools/rt64/src/render/rt64_shader_library.cpp').read_text()
        assert app.index('!swapChain->resize()') < app.index('shaderLibrary->setupCommonShaders')
        assert 'shaderLibrary->swapChainFormat = swapChain->getFormat();' in app
        assert 'pipelineDesc.renderTargetFormat[0] = swapChainFormat;' in shaders
        print('PASS: 3 RT64 initialization/pipeline integration checks')
if __name__=='__main__': main()
