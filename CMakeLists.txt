cmake_minimum_required(VERSION 3.20)
project(SoftRasterizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (MSVC)
    add_compile_options(/utf-8)
endif()
if (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "IntelLLVM")
	# Intel ICX 必须链接 Intel 的 libiomp5md（而非 MSVC 的 libomp140），
	# 否则 MSVC 的 LLVM OpenMP 运行时在 schedule(guided) 等场景下
	# 会触发 kmp_dispatch.cpp 断言失败。
	set(_intel_omp_lib "")
	# 优先从 ICX 编译器安装路径查找
	get_filename_component(_icx_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
	get_filename_component(_icx_root "${_icx_bin_dir}" DIRECTORY)
	if (EXISTS "${_icx_root}/lib/libiomp5md.lib")
		set(_intel_omp_lib "${_icx_root}/lib/libiomp5md.lib")
	else()
		# 回退：从 oneAPI compiler 安装目录搜索
		file(GLOB _intel_omp_candidates
			"C:/Program Files (x86)/Intel/oneAPI/compiler/*/lib/libiomp5md.lib")
		if (_intel_omp_candidates)
			list(SORT _intel_omp_candidates ORDER DESCENDING)
			list(GET _intel_omp_candidates 0 _intel_omp_lib)
		endif()
	endif()

	if (NOT _intel_omp_lib)
		message(WARNING "Could not find Intel libiomp5md.lib, falling back to VS libomp.lib")
		# 回退到 MSVC libomp（可能有兼容性问题）
		set(_intel_omp_lib "")
		if (DEFINED ENV{VCToolsInstallDir})
			file(GLOB _vs_omp_candidates "$ENV{VCToolsInstallDir}/lib/x64/libomp.lib")
		endif()
		if (NOT _vs_omp_candidates)
			file(GLOB _vs_omp_candidates "C:/Program Files/Microsoft Visual Studio/2022/*/VC/Tools/MSVC/*/lib/x64/libomp.lib")
		endif()
		list(LENGTH _vs_omp_candidates _vs_omp_count)
		if (_vs_omp_count GREATER 0)
			list(GET _vs_omp_candidates 0 _intel_omp_lib)
		endif()
	else()
		message(STATUS "Using Intel OpenMP runtime: ${_intel_omp_lib}")
	endif()

	add_library(OpenMP::OpenMP_CXX INTERFACE IMPORTED)
	set_target_properties(OpenMP::OpenMP_CXX PROPERTIES
		INTERFACE_COMPILE_OPTIONS "/Qiopenmp"
	)
	if (_intel_omp_lib)
		set_property(TARGET OpenMP::OpenMP_CXX PROPERTY
			INTERFACE_LINK_LIBRARIES "${_intel_omp_lib}")
	endif()
	# IntelLLVM (DPC++) 默认启用 SYCL，禁用以避免 __sycl_register_lib 相关链接错误
	# 同时添加截图中的 AVX 优化选项
	add_compile_options(-fno-sycl /clang:-mavx2 /clang:-mfma)


else()
	find_package(OpenMP REQUIRED)
endif()

add_subdirectory(SoftRenderer)
add_subdirectory(MFCDemo)
