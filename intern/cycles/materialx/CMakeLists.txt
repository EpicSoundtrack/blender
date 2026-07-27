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
