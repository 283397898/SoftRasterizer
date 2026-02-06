add_library(SoftRenderer SHARED
    src/Asset/GLTFLoader.cpp
    src/Asset/JSONParser.cpp
    src/Asset/ImageDecoder.cpp
    src/Renderer.cpp
    src/Math/Vec2.cpp
    src/Math/Vec3.cpp
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
    src/Runtime/GPUScene.cpp
    src/Runtime/GPUSceneBuilder.cpp
    src/Scene/Scene.cpp
    src/Scene/SceneManager.cpp
    src/Scene/ObjectGroup.cpp
    src/Scene/LightGroup.cpp
    src/Scene/Mesh.cpp
    src/Scene/Transform.cpp
    src/Scene/Model.cpp
    src/Scene/RenderQueue.cpp
    src/Camera/OrbitCamera.cpp
    src/Material/PBRMaterial.cpp
)

target_include_directories(SoftRenderer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(SoftRenderer PRIVATE SOFTRENDERER_EXPORTS NOMINMAX)

target_link_libraries(SoftRenderer PRIVATE OpenMP::OpenMP_CXX)

if (CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM")
    set_property(TARGET SoftRenderer PROPERTY
    MSVC_RUNTIME_LIBRARY "$<$<CONFIG:Debug>:MultiThreadedDebugDLL>$<$<NOT:$<CONFIG:Debug>>:MultiThreadedDLL>")
else()
    set_property(TARGET SoftRenderer PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if (MSVC)
    target_compile_options(SoftRenderer PRIVATE
        $<$<CONFIG:Release>:/O2 /fp:fast /openmp /arch:AVX>
        $<$<CONFIG:Debug>:/Od /Zi /openmp /arch:AVX>
    )
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM")
    target_compile_options(SoftRenderer PRIVATE /clang:-mavx)
endif()
