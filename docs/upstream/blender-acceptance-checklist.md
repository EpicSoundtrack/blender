# Blender/Cycles acceptance checklist for the MaterialX subsystem

Scope: this is an in-tree survey only. I did not use external Blender developer process documents, and this checkout has no `CONTRIBUTING.md` (file search found none). Any statement about Blender Foundation review process that is not directly evidenced by a file below is marked **UNVERIFIED**.

## Checklist

1. Optional features are declared as top-level `WITH_*` CMake options, then discovered/disabled in platform CMake.
   - Evidence: top-level options declare USD, MaterialX, Hydra, and Cycles at `CMakeLists.txt:434-441` and `CMakeLists.txt:595-607`.
   - Evidence: platform discovery checks the library and can turn the option off: Unix does `if(WITH_USD) find_package_wrapper(USD) ... set_and_warn_library_found("USD" USD_FOUND WITH_USD)` and the same for MaterialX at `build_files/cmake/platform/platform_unix.cmake:423-434`; Windows checks the MaterialX lib folder early at `build_files/cmake/platform/platform_win32.cmake:370-373`; macOS does `if(WITH_MATERIALX) find_package(MaterialX) ... set_and_warn_library_found("MaterialX" MaterialX_FOUND WITH_MATERIALX)` at `build_files/cmake/platform/platform_apple.cmake:95-104`.
   - Expected OFF behavior: the option is not just a compile definition; build files must omit sources/links or use optional dependency targets that are empty when OFF. `build_files/cmake/platform/dependency_targets.cmake:7-12` says optional targets always exist and are populated only when enabled, so consumers can link `bf::dependencies::optional::*` safely.

2. Release presets explicitly force optional features on/off.
   - Evidence: the full/release configurations force `WITH_USD`, `WITH_MATERIALX`, and `WITH_HYDRA` ON at `build_files/cmake/config/blender_release.cmake:63-67` and `build_files/cmake/config/blender_full.cmake:59-62`.
   - Evidence: the lite configuration forces `WITH_USD` and `WITH_MATERIALX` OFF at `build_files/cmake/config/blender_lite.cmake:61-64`.
   - Acceptance implication: any Cycles MaterialX code must configure and compile in both `WITH_MATERIALX=ON` and `WITH_MATERIALX=OFF` builds, because the in-tree presets exercise both shapes.

3. Cycles propagates enabled options as compile definitions near the Cycles root.
   - Evidence: `intern/cycles/CMakeLists.txt:207-210` defines `CCL_NAMESPACE_BEGIN`/`CCL_NAMESPACE_END` globally for Cycles. Feature defines are added similarly: `WITH_OSL` at `intern/cycles/CMakeLists.txt:223-225`, CUDA/HIP dynload defines at `intern/cycles/CMakeLists.txt:227-258`, `WITH_EMBREE` at `intern/cycles/CMakeLists.txt:283-308`, and `WITH_USD` at `intern/cycles/CMakeLists.txt:368-370`.
   - Current mismatch: this checkout has no equivalent `if(WITH_MATERIALX) add_definitions(-DWITH_MATERIALX)` in `intern/cycles/CMakeLists.txt`; only other Blender modules add it (`source/blender/io/usd/CMakeLists.txt:219-222`, `source/blender/nodes/shader/CMakeLists.txt:146-164`). If Cycles code needs a MaterialX compile define, it should follow the Cycles-root pattern instead of relying on incidental target state.

4. Hydra is the closest Cycles subsystem precedent: it is gated before adding its directory, not always built.
   - Evidence: `WITH_CYCLES_HYDRA_RENDER_DELEGATE` is declared OFF by default at `CMakeLists.txt:606`; if it is ON without USD, Cycles disables/warns with `set_and_warn_library_found("USD" WITH_USD WITH_CYCLES_HYDRA_RENDER_DELEGATE)` at `intern/cycles/CMakeLists.txt:372-374`.
   - Evidence: Cycles only adds the Hydra directory when `WITH_CYCLES_HYDRA_RENDER_DELEGATE` is ON or standalone+USD is ON: `intern/cycles/CMakeLists.txt:404-405`.
   - Current mismatch: MaterialX is always added by `add_subdirectory(materialx)` at `intern/cycles/CMakeLists.txt:394`; inside it, only the USD-dependent sources are conditional (`intern/cycles/materialx/CMakeLists.txt:26-39`). That does not match Hydra's subsystem-level gate.

5. Dependency declarations use central optional targets where available; direct library names are used only in modules that are already gated.
   - Evidence: the central optional USD target is declared at `build_files/cmake/platform/dependency_targets.cmake:149-156`; it adds `WITH_USD`, include dirs, and libraries only under `if(WITH_USD)`.
   - Evidence: Cycles Hydra links `bf::dependencies::optional::usd`, `bf::dependencies::optional::python`, and `bf::dependencies::optional::tbb` at `intern/cycles/hydra/CMakeLists.txt:102-110`.
   - Evidence: non-Cycles MaterialX modules gate their direct `MaterialXCore`/`MaterialXFormat` links behind `if(WITH_MATERIALX)` (`source/blender/io/usd/CMakeLists.txt:219-224`, `source/blender/nodes/shader/CMakeLists.txt:146-164`).
   - Current mismatch/unknown: in this checkout `intern/cycles/materialx/CMakeLists.txt:22-39` links USD/Python but does not link `MaterialXCore` or `MaterialXFormat`. If the intended branch links them, it should either add a central `bf::dependencies::optional::materialx` target analogous to USD, or keep all direct `MaterialXCore`/`MaterialXFormat` links and sources inside `if(WITH_MATERIALX)`.

6. Runtime dynamic loading has precedent in Cycles for GPU runtimes, but not for MaterialX in this checkout.
   - Evidence: CUDA dynload is a top-level option (`WITH_CUDA_DYNLOAD`) at `CMakeLists.txt:654-669`; when enabled, Cycles includes `extern/cuew` and defines `WITH_CUDA_DYNLOAD` at `intern/cycles/CMakeLists.txt:227-240`, and appends `extern_cuew` instead of `${CUDA_CUDA_LIBRARY}` at `intern/cycles/cmake/macros.cmake:133-139`.
   - Evidence: HIP dynload follows the same pattern at `intern/cycles/CMakeLists.txt:253-258`, `intern/cycles/cmake/macros.cmake:142-144`, and runtime initialization in `intern/cycles/device/hip/device.cpp:32-44`.
   - Evidence: direct `dlopen` exists in a device backend (`intern/cycles/device/hiprt/device_impl.cpp:82-85`).
   - Acceptance implication: a MaterialX runtime-load design would need the same explicit option/discovery/compile-define/runtime-fallback shape. I found no in-Cycles MaterialX runtime-load precedent in `intern/cycles/materialx` (search only found MaterialX text/comments and no `dlopen`, `LoadLibrary`, or `BLI_dynlib` there).

7. File headers, include order, and namespaces have visible Cycles conventions.
   - Evidence: representative Cycles test `intern/cycles/test/util_transform_test.cpp:1-10` uses the Apache SPDX block, external include (`<gtest/gtest.h>`) before local includes, then `CCL_NAMESPACE_BEGIN`.
   - Evidence: representative Hydra files use the Apache SPDX block with both NVIDIA and Blender copyrights (`intern/cycles/hydra/material.cpp:1-4`, `intern/cycles/hydra/material.h:1-4`), put the matching local header first (`intern/cycles/hydra/material.cpp:6-13`), then USD/system includes (`intern/cycles/hydra/material.cpp:15-29`), and use Hydra namespace macros from `intern/cycles/hydra/config.h:13-18`.
   - Current mismatch: `intern/cycles/materialx/graph.cpp:5-18` and `intern/cycles/materialx/usdshade_reader.cpp:5-29` put the local header first and use `CCL_NAMESPACE_BEGIN`, which is consistent, but `usdshade_reader.cpp` has USD includes after standard includes and before local Cycles includes, while Hydra keeps local Cycles headers before USD/system headers. This may be acceptable, but it is a visible style difference.

8. The two large MaterialX implementation files are far outside nearby Cycles scale.
   - Evidence: measured in this checkout: `intern/cycles/materialx/graph.cpp` has 26,687 lines and `intern/cycles/materialx/usdshade_reader.cpp` has 22,453 lines.
   - Evidence: representative upstream/neighbouring files read in this survey are much smaller: `intern/cycles/hydra/material.cpp` has 4,944 lines and `intern/cycles/hydra/material.h` has 101 lines; `intern/cycles/materialx/authority_pipeline.cpp` has 179 lines and `intern/cycles/materialx/authority.cpp` has 122 lines.
   - Acceptance implication: **UNVERIFIED as a formal policy**, but a reviewer will see the 22k/26k-line files before they see correctness. Splitting by NodeDef family or pipeline layer is the cheapest structural improvement with direct in-tree support from the much smaller Hydra/materialx support files.

9. Cycles gtests are registered by source lists, then a suite executable; Python Blender smoke tests are explicit CTest entries.
   - Evidence: `intern/cycles/test/CMakeLists.txt:30-56` lists C++ gtest sources; `intern/cycles/test/CMakeLists.txt:81-83` calls `blender_add_test_suite_executable(cycles "${SRC}" ...)`.
   - Evidence: the test helper is defined in `build_files/cmake/testing.cmake:302-304`; the lower-level gtest helper only emits tests under `if(WITH_GTESTS)` at `build_files/cmake/testing.cmake:49-83`.
   - Evidence: MaterialX+Blender smoke tests are explicit `add_test` calls gated by `if(WITH_CYCLES_BLENDER AND WITH_USD AND WITH_MATERIALX)` at `intern/cycles/test/CMakeLists.txt:122-201`.

10. The current MaterialX C++ tests are not shaped like existing Cycles tests.
    - Evidence: measured in this checkout, the largest existing non-MaterialX `intern/cycles/test/*.cpp` file is `render_graph_finalize_test.cpp` at 1,598 lines; most utility tests are under 500 lines (for example `intern/cycles/test/util_transform_test.cpp` is 41 lines, with its whole shape visible at `intern/cycles/test/util_transform_test.cpp:1-41`).
    - Evidence: the current MaterialX tests are 17,802 lines (`intern/cycles/test/materialx_graph_test.cpp`) and 25,575 lines (`intern/cycles/test/materialx_usdshade_reader_test.cpp`). Hydra's largest MaterialX-adjacent test in this checkout is `intern/cycles/hydra/hydra_materialx_mapping_test.cpp` at 5,002 lines.
    - Acceptance implication: **UNVERIFIED as a formal policy**, but a 27k-line single test file is not consistent with the nearby in-tree test layout. Split by feature family and keep each source independently listed/gated in `intern/cycles/test/CMakeLists.txt`.

## What would get rejected on sight, ordered by cheapness to fix

1. Split the giant test sources first. The cheapest high-signal change is to break `materialx_usdshade_reader_test.cpp` and `materialx_graph_test.cpp` into family-sized files and list them individually in `intern/cycles/test/CMakeLists.txt`, matching the source-list registration at `intern/cycles/test/CMakeLists.txt:30-66`.
2. Split the giant implementation files next. `graph.cpp` and `usdshade_reader.cpp` are 22k-26k lines; nearby precedent (`intern/cycles/hydra/material.cpp`, `intern/cycles/materialx/authority_pipeline.cpp`) is much smaller and organized by responsibility.
3. Add a true `WITH_MATERIALX` Cycles gate. MaterialX is currently always `add_subdirectory`'d from Cycles (`intern/cycles/CMakeLists.txt:394`) unlike Hydra (`intern/cycles/CMakeLists.txt:404-405`). Either gate the subdirectory or make every source/link safe when `WITH_MATERIALX=OFF`.
4. Normalize MaterialX dependency linkage. If Cycles links `MaterialXCore`/`MaterialXFormat`, keep it inside `if(WITH_MATERIALX)` as other modules do (`source/blender/nodes/shader/CMakeLists.txt:146-164`) or introduce `bf::dependencies::optional::materialx` analogous to USD (`build_files/cmake/platform/dependency_targets.cmake:149-156`).
5. Keep USD-dependent MaterialX reader code behind both dependency gates. Current tests use `if(WITH_USD AND WITH_MATERIALX)` (`intern/cycles/test/CMakeLists.txt:61-66`), while `intern/cycles/materialx/CMakeLists.txt:26-39` gates reader sources only on `WITH_USD`; if reader code also needs MaterialX libraries, this should become `WITH_USD AND WITH_MATERIALX`.
6. Document or remove runtime dynamic MaterialX loading. Cycles dynload precedent exists for CUDA/HIP with explicit options and shim libraries, not as an ad hoc MaterialX load path (`CMakeLists.txt:654-669`, `intern/cycles/cmake/macros.cmake:133-144`).
7. Make include ordering boring. The current MaterialX files are close to Cycles style, but Hydra's local-before-USD ordering (`intern/cycles/hydra/material.cpp:6-29`) is the clearest precedent; aligning the large new files removes a cheap review distraction.
8. Preserve OFF-build behavior in presets. `blender_lite.cmake` forces `WITH_MATERIALX=OFF` (`build_files/cmake/config/blender_lite.cmake:61-64`), so any acceptance checklist should include at least a configure/build check under that preset or an equivalent `-DWITH_MATERIALX=OFF` build.
9. Do not claim Blender process requirements from this repo. This checkout has no process document; any statement such as "Blender requires N reviewers" or "subsystems must be split this way" is **UNVERIFIED** unless confirmed from external Blender developer docs or maintainer feedback.
10. Keep comments factual and citeable. Several current comments in `graph.cpp`/`usdshade_reader.cpp` explain MaterialX semantics at length (for example `intern/cycles/materialx/graph.cpp:48-67` and `intern/cycles/materialx/usdshade_reader.cpp:60-76`). That is useful, but in a huge file it raises review cost; splitting code and tests will make those comments easier to verify.
