# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

if(WITH_GTESTS)
  # Otherwise we get warnings here that we can't fix in external projects
  remove_strict_flags()
endif()

set(INC
  ..
)

set(LIB
  cycles_kernel
  cycles_integrator
  cycles_materialx
  cycles_scene
  cycles_session
  cycles_bvh
  cycles_graph
  cycles_subd
  cycles_device
  cycles_util
)
cycles_external_libraries_append(LIB)

include_directories(${INC})

set(SRC
  integrator_adaptive_sampling_test.cpp
  integrator_render_scheduler_test.cpp
  integrator_tile_test.cpp
  kernel_camera_projection_test.cpp
  materialx_graph_test.cpp
  render_graph_finalize_test.cpp
  util_aligned_malloc_test.cpp
  util_boundbox_test.cpp
  util_cache_limiter_test.cpp
  util_half_test.cpp
  util_ies_test.cpp
  util_math_test.cpp
  util_math_fast_test.cpp
  util_math_float3_test.cpp
  util_math_float4_test.cpp
  util_rgbe_test.cpp
  util_md5_test.cpp
  util_path_test.cpp
  util_sha256_test.cpp
  util_simd_test.cpp
  util_string_test.cpp
  util_task_test.cpp
  util_time_test.cpp
  util_transform_test.cpp
  util_types_base_test.cpp
)

# USDShade is the authoritative serialized representation for the experimental
# MaterialX/Cycles path.  Keep this test out of builds that do not carry both
# optional dependencies.
if(WITH_USD AND WITH_MATERIALX)
  list(APPEND SRC
    materialx_usdshade_reader_test.cpp
  )
  list(APPEND LIB bf::dependencies::optional::usd bf::dependencies::optional::python)
endif()

# Disable AVX tests on macOS. Rosetta has problems running them, and other
# platforms should be enough to verify AVX operations are implemented correctly.
if(NOT APPLE)
  if(CXX_HAS_AVX2)
    list(APPEND SRC
      util_float8_avx2_test.cpp
      util_half_avx2_test.cpp
    )
    set_source_files_properties(util_float8_avx2_test.cpp PROPERTIES COMPILE_FLAGS "${CYCLES_AVX2_F16C_FLAGS}")
    set_source_files_properties(util_half_avx2_test.cpp PROPERTIES COMPILE_FLAGS "${CYCLES_AVX2_F16C_FLAGS}")
  endif()
endif()

if(WITH_GTESTS)
  set(INC_SYS "")
  blender_add_test_suite_executable(cycles "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

  if(WITH_USD AND WITH_MATERIALX)
    # Same MSVC /Zc:preprocessor + oneTBB TBB_USE_DEBUG auto-detection issue
    # worked around in intern/cycles/materialx/CMakeLists.txt and
    # intern/cycles/hydra/CMakeLists.txt -- this test binary also pulls in
    # USD headers (materialx_usdshade_reader_test.cpp), which transitively
    # include tbb/version.h and hit the same "invalid integer constant
    # expression" fatal error in Debug configs without this definition.
    target_compile_definitions(cycles_test
      PRIVATE
      TBB_USE_DEBUG=$<CONFIG:DEBUG>
    )

    # materialx_usdshade_reader_test.cpp constructs a real pxr::UsdStage.
    # USD's plugin registry looks for plugInfo.json next to the loading
    # usd_ms(_d).dll on first use; the full Blender install stages both
    # ${USD_LIBRARY_DIR}/usd (core plugins: ar, sdf, usd, ...) and
    # ${LIBDIR}/usd/plugin/usd (optional renderer plugins: hdStorm, hio*,
    # ...) into blender.shared/usd for exactly this reason (see
    # source/creator/CMakeLists.txt), but cycles_test never got the same
    # treatment. Without it, the first USD API call anywhere in this suite
    # (usually UsdStage::CreateInMemory()) fails to find any plugins and
    # USD's fatal-error path calls DebugBreak(), which is fine under a
    # debugger but crashes the process with STATUS_BREAKPOINT (exit code
    # -2147483645) when run standalone.
    add_custom_command(
      TARGET cycles_test POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${USD_LIBRARY_DIR}/usd
        $<TARGET_FILE_DIR:cycles_test>/usd
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/usd/plugin/usd
        $<TARGET_FILE_DIR:cycles_test>/usd
      COMMENT "Staging USD plugin resources next to cycles_test.exe"
    )
  endif()
endif()

if(WITH_CYCLES_BLENDER AND WITH_USD AND WITH_MATERIALX)
  add_test(
    NAME cycles_materialx_conversion_authority_blender
    COMMAND $<TARGET_FILE:blender>
      --background
      --factory-startup
      --python-exit-code 1
      --env-system-scripts ${CMAKE_SOURCE_DIR}/scripts
      --python ${CMAKE_CURRENT_SOURCE_DIR}/materialx_usdshade_blender_conversion_operator_test.py
  )
  set_tests_properties(cycles_materialx_conversion_authority_blender PROPERTIES
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    LABELS "cycles;materialx"
  )
  add_test(
    NAME cycles_materialx_authority_blender
    COMMAND $<TARGET_FILE:blender>
      --background
      --factory-startup
      --python-exit-code 1
      --env-system-scripts ${CMAKE_SOURCE_DIR}/scripts
      --python ${CMAKE_CURRENT_SOURCE_DIR}/materialx_usdshade_blender_smoke.py
  )
  set_tests_properties(cycles_materialx_authority_blender PROPERTIES
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    LABELS "cycles;materialx"
  )
  add_test(
    NAME cycles_materialx_texture_authority_blender
    COMMAND $<TARGET_FILE:blender>
      --background
      --factory-startup
      --python-exit-code 1
      --env-system-scripts ${CMAKE_SOURCE_DIR}/scripts
      --python ${CMAKE_CURRENT_SOURCE_DIR}/materialx_usdshade_blender_texture_smoke.py
  )
  set_tests_properties(cycles_materialx_texture_authority_blender PROPERTIES
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    LABELS "cycles;materialx"
  )
  add_test(
    NAME cycles_materialx_ramp_authority_blender
    COMMAND $<TARGET_FILE:blender>
      --background
      --factory-startup
      --python-exit-code 1
      --env-system-scripts ${CMAKE_SOURCE_DIR}/scripts
      --python ${CMAKE_CURRENT_SOURCE_DIR}/materialx_usdshade_blender_ramp_smoke.py
  )
  set_tests_properties(cycles_materialx_ramp_authority_blender PROPERTIES
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    LABELS "cycles;materialx"
  )
  add_test(
    NAME cycles_materialx_utility_battery_blender
    COMMAND $<TARGET_FILE:blender>
      --background
      --factory-startup
      --python-exit-code 1
      --env-system-scripts ${CMAKE_SOURCE_DIR}/scripts
      --python ${CMAKE_CURRENT_SOURCE_DIR}/materialx_usdshade_blender_utility_battery.py
  )
  set_tests_properties(cycles_materialx_utility_battery_blender PROPERTIES
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    LABELS "cycles;materialx"
  )
  add_test(
    NAME cycles_materialx_composed_cuda_authority_blender
    COMMAND $<TARGET_FILE:blender>
      --background
      --factory-startup
      --python-exit-code 1
      --env-system-scripts ${CMAKE_SOURCE_DIR}/scripts
      --python ${CMAKE_CURRENT_SOURCE_DIR}/materialx_usdshade_blender_composed_cuda_smoke.py
  )
  set_tests_properties(cycles_materialx_composed_cuda_authority_blender PROPERTIES
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    LABELS "cycles;materialx"
  )
endif()
