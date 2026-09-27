# SPDX-License-Identifier: GPL-2.0-only
# Core renderer sources from the pinned upstream src/CMakeLists.txt. No Mupen CPU,
# Qt launcher, external texture packs or desktop OSD are linked into this target.
get_filename_component(GLIDE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.deps/GLideN64" ABSOLUTE)
execute_process(COMMAND git -C "${GLIDE_ROOT}" rev-parse HEAD OUTPUT_VARIABLE GLIDE_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE GLIDE_RESULT)
if(NOT GLIDE_RESULT EQUAL 0 OR NOT GLIDE_COMMIT STREQUAL "41c7ba273a6c9afb43c0574cf3cf5d139182d070")
    message(FATAL_ERROR "Run android/tools/prepare.py --dependencies for the pinned GLideN64 renderer.")
endif()
set(GLIDE_SOURCE_NAMES
    3DMath.cpp
    Combiner.cpp
    CombinerKey.cpp
    Config.cpp
    convert.cpp
    CRC32.cpp
    DebugDump.cpp
    Debugger.cpp
    DepthBuffer.cpp
    DisplayWindow.cpp
    DisplayLoadProgress.cpp
    FrameBuffer.cpp
    FrameBufferInfo.cpp
    GBI.cpp
    gDP.cpp
    GLideN64.cpp
    GraphicsDrawer.cpp
    gSP.cpp
    N64.cpp
    PaletteTexture.cpp
    Performance.cpp
    PostProcessor.cpp
    RDP.cpp
    RSP.cpp
    RSP_LoadMatrix.cpp
    SoftwareRender.cpp
    TexrectDrawer.cpp
    TextureFilterHandler.cpp
    Textures.cpp
    VI.cpp
    ZlutTexture.cpp
    BufferCopy/BlueNoiseTexture.cpp
    BufferCopy/ColorBufferToRDRAM.cpp
    BufferCopy/DepthBufferToRDRAM.cpp
    BufferCopy/RDRAMtoColorBuffer.cpp
    DepthBufferRender/ClipPolygon.cpp
    DepthBufferRender/DepthBufferRender.cpp
    common/CommonAPIImpl_common.cpp
    Graphics/Context.cpp
    Graphics/ColorBufferReader.cpp
    Graphics/CombinerProgram.cpp
    Graphics/ObjectHandle.cpp
    Graphics/OpenGLContext/GLFunctions.cpp
    Graphics/OpenGLContext/ThreadedOpenGl/opengl_Command.cpp
    Graphics/OpenGLContext/ThreadedOpenGl/opengl_ObjectPool.cpp
    Graphics/OpenGLContext/ThreadedOpenGl/opengl_Wrapper.cpp
    Graphics/OpenGLContext/ThreadedOpenGl/opengl_WrappedFunctions.cpp
    Graphics/OpenGLContext/ThreadedOpenGl/RingBufferPool.cpp
    Graphics/OpenGLContext/opengl_Attributes.cpp
    Graphics/OpenGLContext/opengl_BufferedDrawer.cpp
    Graphics/OpenGLContext/opengl_BufferManipulationObjectFactory.cpp
    Graphics/OpenGLContext/opengl_CachedFunctions.cpp
    Graphics/OpenGLContext/opengl_ColorBufferReaderWithBufferStorage.cpp
    Graphics/OpenGLContext/opengl_ColorBufferReaderWithPixelBuffer.cpp
    Graphics/OpenGLContext/opengl_ColorBufferReaderWithReadPixels.cpp
    Graphics/OpenGLContext/opengl_ContextImpl.cpp
    Graphics/OpenGLContext/opengl_GLInfo.cpp
    Graphics/OpenGLContext/opengl_Parameters.cpp
    Graphics/OpenGLContext/opengl_TextureManipulationObjectFactory.cpp
    Graphics/OpenGLContext/opengl_UnbufferedDrawer.cpp
    Graphics/OpenGLContext/opengl_Utils.cpp
    Graphics/OpenGLContext/GLSL/glsl_CombinerInputs.cpp
    Graphics/OpenGLContext/GLSL/glsl_CombinerProgramBuilder.cpp
    Graphics/OpenGLContext/GLSL/glsl_CombinerProgramBuilderCommon.cpp
    Graphics/OpenGLContext/GLSL/glsl_CombinerProgramBuilderAccurate.cpp
    Graphics/OpenGLContext/GLSL/glsl_CombinerProgramBuilderFast.cpp
    Graphics/OpenGLContext/GLSL/glsl_CombinerProgramImpl.cpp
    Graphics/OpenGLContext/GLSL/glsl_CombinerProgramUniformFactory.cpp
    Graphics/OpenGLContext/GLSL/glsl_CombinerProgramUniformFactoryCommon.cpp
    Graphics/OpenGLContext/GLSL/glsl_CombinerProgramUniformFactoryAccurate.cpp
    Graphics/OpenGLContext/GLSL/glsl_CombinerProgramUniformFactoryFast.cpp
    Graphics/OpenGLContext/GLSL/glsl_FXAA.cpp
    Graphics/OpenGLContext/GLSL/glsl_ShaderStorage.cpp
    Graphics/OpenGLContext/GLSL/glsl_SpecialShadersFactory.cpp
    Graphics/OpenGLContext/GLSL/glsl_Utils.cpp
    uCodes/F3D.cpp
    uCodes/F3DBETA.cpp
    uCodes/F3DDKR.cpp
    uCodes/F3DEX.cpp
    uCodes/F3DEX095.cpp
    uCodes/F3DAM.cpp
    uCodes/F3DEX2.cpp
    uCodes/F3DEX3.cpp
    uCodes/F3DEX2ACCLAIM.cpp
    uCodes/F3DEX2CBFD.cpp
    uCodes/F3DZEX2.cpp
    uCodes/F3DFLX2.cpp
    uCodes/F3DGOLDEN.cpp
    uCodes/F3DTEXA.cpp
    uCodes/F3DPD.cpp
    uCodes/F3DSETA.cpp
    uCodes/F5Indi_Naboo.cpp
    uCodes/F5Rogue.cpp
    uCodes/L3D.cpp
    uCodes/L3DEX2.cpp
    uCodes/L3DEX.cpp
    uCodes/S2DEX2.cpp
    uCodes/S2DEX.cpp
    uCodes/T3DUX.cpp
    uCodes/Turbo3D.cpp
    uCodes/ZSort.cpp
    uCodes/ZSortBOSS.cpp
    TxFilterStub.cpp
    TextDrawerStub.cpp
    osal/osal_files_unix.c
    osal/osal_keys_unix.c
    mupenplus/MemoryStatus_mupenplus.cpp
)
if(ANDROID)
    list(REMOVE_ITEM GLIDE_SOURCE_NAMES 3DMath.cpp RSP_LoadMatrix.cpp CRC32.cpp)
    list(APPEND GLIDE_SOURCE_NAMES
        Neon/3DMathNeon.cpp Neon/gSPNeon.cpp Neon/RSP_LoadMatrixNeon.cpp Neon/CRC_OPT_NEON.cpp
        Graphics/OpenGLContext/opengl_ColorBufferReaderWithEGLImage.cpp
        Graphics/OpenGLContext/GraphicBuffer/PrivateApi/GraphicBuffer.cpp
        Graphics/OpenGLContext/GraphicBuffer/PrivateApi/libhardware.cpp
        Graphics/OpenGLContext/GraphicBuffer/PublicApi/android_hardware_buffer_compat.cpp
        Graphics/OpenGLContext/GraphicBuffer/GraphicBufferWrapper.cpp)
endif()
list(TRANSFORM GLIDE_SOURCE_NAMES PREPEND "${GLIDE_ROOT}/src/")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/Revision.h" "#define PLUGIN_REVISION \"41c7ba27-conker014\"\n")
add_library(conker_gles_core SHARED ${GLIDE_SOURCE_NAMES} gles_bridge.cpp gles_platform.cpp gles_display.cpp)
set_target_properties(conker_gles_core PROPERTIES CXX_STANDARD 17)
target_compile_features(conker_gles_core PRIVATE cxx_std_17)
target_compile_definitions(conker_gles_core PRIVATE MUPENPLUSAPI TXFILTER_LIB EGL EGL_EGLEXT_PROTOTYPES)
if(ANDROID)
    target_compile_definitions(conker_gles_core PRIVATE OS_ANDROID __NEON_OPT __VEC4_OPT)
    target_link_libraries(conker_gles_core PRIVATE android log dl)
    target_link_options(conker_gles_core PRIVATE "-Wl,-z,max-page-size=16384" "-Wl,--threads=1")
endif()
target_compile_options(conker_gles_core PRIVATE -fno-strict-aliasing -fvisibility=hidden)
target_include_directories(conker_gles_core PRIVATE "${GLIDE_ROOT}/src" "${GLIDE_ROOT}/src/inc" "${GLIDE_ROOT}/src/osal" "${CMAKE_CURRENT_BINARY_DIR}")
find_library(CONKER_EGL_LIBRARY NAMES EGL libEGL.so.1 REQUIRED)
target_link_libraries(conker_gles_core PRIVATE ${CONKER_EGL_LIBRARY})
