cmake_minimum_required(VERSION 3.20)
project(SoftRasterizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM")
	set(_vs_libomp "")
	if (DEFINED ENV{VCToolsInstallDir})
		file(GLOB _vs_omp_candidates "$ENV{VCToolsInstallDir}/lib/x64/libomp.lib")
	endif()
	if (NOT _vs_omp_candidates)
		file(GLOB _vs_omp_candidates "C:/Program Files/Microsoft Visual Studio/2022/*/VC/Tools/MSVC/*/lib/x64/libomp.lib")
	endif()
	list(LENGTH _vs_omp_candidates _vs_omp_count)
	if (_vs_omp_count GREATER 0)
		list(GET _vs_omp_candidates 0 _vs_libomp)
	endif()

	add_library(OpenMP::OpenMP_CXX INTERFACE IMPORTED)
	set_target_properties(OpenMP::OpenMP_CXX PROPERTIES
		INTERFACE_COMPILE_OPTIONS "/Qopenmp"
	)
	if (_vs_libomp)
		set_property(TARGET OpenMP::OpenMP_CXX PROPERTY
			INTERFACE_LINK_LIBRARIES "${_vs_libomp}")
	endif()
	# IntelLLVM (DPC++) 默认启用 SYCL，禁用以避免 __sycl_register_lib 相关链接错误
	# 同时添加截图中的 AVX 优化选项
	add_compile_options(-fno-sycl /clang:-mavx)


else()
	find_package(OpenMP REQUIRED)
endif()

add_subdirectory(SoftRenderer)
add_subdirectory(MFCDemo)
