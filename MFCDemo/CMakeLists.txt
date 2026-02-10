add_executable(MFCDemo
    src/RenderView.cpp
    src/WinMain.cpp
    src/HDRPresenter.cpp
)

target_link_libraries(MFCDemo PRIVATE SoftRenderer d3d12 dxgi)

target_include_directories(MFCDemo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if (MSVC)
    target_compile_definitions(MFCDemo PRIVATE _UNICODE UNICODE)
    target_compile_options(MFCDemo PRIVATE /utf-8)
    set_target_properties(MFCDemo PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

# 将 SoftRenderer.dll 复制到可执行文件目录
add_custom_command(TARGET MFCDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SoftRenderer>
        $<TARGET_FILE_DIR:MFCDemo>
    COMMENT "Copying SoftRenderer.dll to MFCDemo output directory"
)
