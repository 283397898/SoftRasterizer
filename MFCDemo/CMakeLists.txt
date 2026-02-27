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

if (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM")
    # 仅在 icx(IntelLLVM) 构建时复制 DLL
    add_custom_command(TARGET MFCDemo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SoftRenderer>
            $<TARGET_FILE_DIR:MFCDemo>
        COMMENT "Copying SoftRenderer.dll to MFCDemo output directory"
    )

    # 复制 Intel CPU 运行时，便于在未配置 oneAPI PATH 的环境直接运行
    get_filename_component(_intel_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(_intel_runtime_dlls
        libiomp5md.dll
        libmmd.dll
        svml_dispmd.dll
        libircmd.dll
    )

    foreach(_dll IN LISTS _intel_runtime_dlls)
        if (EXISTS "${_intel_bin_dir}/${_dll}")
            add_custom_command(TARGET MFCDemo POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_intel_bin_dir}/${_dll}"
                    "$<TARGET_FILE_DIR:MFCDemo>"
                COMMENT "Copying ${_dll} to MFCDemo output directory"
            )
        endif()
    endforeach()
endif()

# 将 example 文件夹复制到可执行文件目录
add_custom_command(TARGET MFCDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/example
        $<TARGET_FILE_DIR:MFCDemo>/example
    COMMENT "Copying example folder to MFCDemo output directory"
)
