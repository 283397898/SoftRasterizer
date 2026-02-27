add_library(SoftRenderer SHARED
    src/Asset/GLTFLoader.cpp
    src/Asset/JSONParser.cpp
    src/Asset/ImageDecoder.cpp
    src/Asset/EXRDecoder.cpp
    src/Renderer.cpp
    src/Math/Vec2.cpp
    src/Math/Vec4.cpp
    src/Math/Mat4.cpp
    src/Core/Framebuffer.cpp
    src/Core/DepthBuffer.cpp
    src/Core/Texture.cpp
    src/Render/FrameContextBuilder.cpp
    src/Render/RenderQueueBuilder.cpp
    src/Render/GPUSceneRenderQueueBuilder.cpp
    src/Render/RenderPipeline.cpp
    src/Render/RendererConfig.cpp
    src/Pipeline/VertexShader.cpp
    src/Pipeline/GeometryProcessor.cpp
    src/Pipeline/Clipper.cpp
    src/Pipeline/Rasterizer.cpp
    src/Pipeline/FragmentShader.cpp
    src/Pipeline/EnvironmentMap.cpp
    src/Pipeline/MaterialTable.cpp
    src/Pipeline/OpaquePass.cpp
    src/Pipeline/PassBuilder.cpp
    src/Utils/Compression.cpp
    src/Runtime/GPUScene.cpp
    src/Runtime/GPUSceneBuilder.cpp
    src/Scene/Scene.cpp
    src/Scene/ObjectGroup.cpp
    src/Scene/LightGroup.cpp
    src/Scene/Mesh.cpp
    src/Scene/Transform.cpp
    src/Scene/Model.cpp
    src/Scene/RenderQueue.cpp
    src/Camera/OrbitCamera.cpp
)

target_include_directories(SoftRenderer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(SoftRenderer PRIVATE SOFTRENDERER_EXPORTS NOMINMAX)
target_compile_definitions(SoftRenderer PRIVATE
    $<$<CXX_COMPILER_ID:IntelLLVM>:SR_INTEL_OMP>
)

target_link_libraries(SoftRenderer PRIVATE OpenMP::OpenMP_CXX)

if (CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM")
    set_property(TARGET SoftRenderer PROPERTY
    MSVC_RUNTIME_LIBRARY "$<$<CONFIG:Debug>:MultiThreadedDebugDLL>$<$<NOT:$<CONFIG:Debug>>:MultiThreadedDLL>")
else()
    set_property(TARGET SoftRenderer PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if (MSVC)
    target_compile_options(SoftRenderer PRIVATE /utf-8)
    target_compile_options(SoftRenderer PRIVATE
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/fp:fast>
        $<$<CONFIG:Release>:/arch:AVX2>
        $<$<CONFIG:Release>:$<$<CXX_COMPILER_ID:IntelLLVM>:/Qiopenmp>>
        $<$<CONFIG:Release>:$<$<NOT:$<CXX_COMPILER_ID:IntelLLVM>>:/openmp>>
        $<$<CONFIG:Debug>:/Od>
        $<$<CONFIG:Debug>:/Zi>
        $<$<CONFIG:Debug>:/arch:AVX2>
        $<$<CONFIG:Debug>:$<$<CXX_COMPILER_ID:IntelLLVM>:/Qiopenmp>>
        $<$<CONFIG:Debug>:$<$<NOT:$<CXX_COMPILER_ID:IntelLLVM>>:/openmp>>
    )
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM")
    target_compile_options(SoftRenderer PRIVATE /clang:-mavx2 /clang:-mfma)
endif()
