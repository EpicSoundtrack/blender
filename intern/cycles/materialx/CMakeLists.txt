# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: Apache-2.0

set(INC
  ..
)

set(INC_SYS
)

set(SRC
  authority.cpp
  graph.cpp
)

set(SRC_HEADERS
  authority.h
  graph.h
)

set(LIB
  PUBLIC cycles_scene
)

if(WITH_USD)
  list(APPEND SRC
    authority_pipeline.cpp
    usdshade_reader.cpp
  )
  list(APPEND SRC_HEADERS
    authority_pipeline.h
    usdshade_reader.h
  )
  list(APPEND LIB
    PRIVATE bf::dependencies::optional::usd
    PRIVATE bf::dependencies::optional::python
  )
endif()

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

cycles_add_library(cycles_materialx "${LIB}" ${SRC} ${SRC_HEADERS})

if(WITH_USD)
  # MSVC's conformant preprocessor (/Zc:preprocessor) combined with a Debug
  # config's empty-valued _DEBUG macro breaks oneTBB's TBB_USE_DEBUG
  # auto-detection in tbb/version.h (included transitively via USD headers),
  # producing a "fatal error C1017: invalid integer constant expression".
  # Same known issue and same fix as intern/cycles/hydra/CMakeLists.txt.
  target_compile_definitions(cycles_materialx
    PRIVATE
    TBB_USE_DEBUG=$<CONFIG:DEBUG>
  )
endif()
