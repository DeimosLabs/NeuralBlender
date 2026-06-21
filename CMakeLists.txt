cmake_minimum_required(VERSION 3.10)

project(neuralblender
  VERSION 0.1.0
  DESCRIPTION "Amp modeling plugin based on RTNeural and NAM"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(STANDALONE     "Build standalone JACK client" ON)
option(LV2            "Build LV2 plugin" ON)
option(GUI            "Enable GUI" OFF)
option(DEBUG          "Debug build" OFF)
option(TRACE          "Debug (mostly) UI code" OFF)
option(NOASAN         "Disable asan on debug build" ON)

find_package(PkgConfig REQUIRED)
find_package(Eigen3 REQUIRED)

if (TRACE)
  add_compile_definitions(CMDLINE_DEBUG)
endif()

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Timestamp header
set(TIMESTAMP_FILE ${CMAKE_CURRENT_BINARY_DIR}/timestamp.h)
set_source_files_properties(${TIMESTAMP_FILE} PROPERTIES GENERATED TRUE)

add_custom_target(generate_timestamp ALL
  COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/timestamp.sh" "${TIMESTAMP_FILE}"
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  COMMENT "Updating build timestamp"
)

add_custom_target(generate_inline_data ALL
  COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/data/generate_inline_data.sh"
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  COMMENT "Generating inline data"
)

execute_process (COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/data/generate_inline_data.sh
                 WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

execute_process (COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/timestamp.sh
                 WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_library(neuralblender_core OBJECT
  src/neuralblender.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/data/data.cpp
  
  external/NAM/get_dsp.cpp
  external/NAM/dsp.cpp
  external/NAM/container.cpp
  external/NAM/convnet.cpp
  external/NAM/lstm.cpp
  external/NAM/linear.cpp
  external/NAM/conv1d.cpp
  external/NAM/activations.cpp
  external/NAM/ring_buffer.cpp
  external/NAM/util.cpp
  external/NAM/wavenet/model.cpp
  external/NAM/wavenet/a2_fast.cpp
  external/NAM/wavenet/slimmable.cpp
)

#add_library(xputty STATIC
#  external/libxputty/xputty/xwidget.c
#  external/libxputty/xputty/xputty.c
#)
#
#target_include_directories(xputty PUBLIC
#  ${PROJECT_SOURCE_DIR}/external/libxputty
#  ${PROJECT_SOURCE_DIR}/external/libxputty/xputty
#)
#
#target_link_libraries(xputty PUBLIC
#  ${X11_LIBRARIES}
#  cairo
#)


set_target_properties(neuralblender_core PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)

add_dependencies(neuralblender_core generate_timestamp)
add_dependencies(neuralblender_core generate_inline_data)

target_include_directories(neuralblender_core PUBLIC
  ${CMAKE_CURRENT_BINARY_DIR}
  ${PROJECT_SOURCE_DIR}/external
  ${PROJECT_SOURCE_DIR}/external/RTNeural
  ${PROJECT_SOURCE_DIR}/external/NAM
  ${PROJECT_SOURCE_DIR}/external/nlohmann
  ${PROJECT_SOURCE_DIR}
)

target_compile_definitions(neuralblender_core PUBLIC
  NEURALBLENDER_VERSION="${PROJECT_VERSION}"
  NAM_SAMPLE_FLOAT
  DSP_SAMPLE_FLOAT
)

target_link_libraries(neuralblender_core PUBLIC Eigen3::Eigen)

if (DEBUG)
  message(STATUS "Debug enabled")
  target_compile_definitions(neuralblender_core PUBLIC DEBUG)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)

  if (NOT NOASAN)
    target_compile_options(neuralblender_core PUBLIC
      -fsanitize=address
      -fno-omit-frame-pointer
      -g
    )
    target_link_options(neuralblender_core PUBLIC
      -fsanitize=address
    )
  endif()
else()
  target_compile_definitions(neuralblender_core PUBLIC NDEBUG)
endif()

if (STANDALONE)
  pkg_check_modules(JACK jack)

  if (JACK_FOUND)
    message(STATUS "JACK: found")

    add_executable(neuralblender
      src/standalone.cpp
    )

    target_include_directories(neuralblender PRIVATE ${JACK_INCLUDE_DIRS})
    target_link_libraries(neuralblender PRIVATE
      neuralblender_core
      ${JACK_LIBRARIES}
    )

    target_compile_definitions(neuralblender PRIVATE STANDALONE)

    install(TARGETS neuralblender RUNTIME DESTINATION bin)
  else()
    message(WARNING "JACK development files not found; standalone target disabled")
  endif()
endif()

if (LV2)
  add_custom_target(generate_ttl ALL
    COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/lv2/generate_ttl.sh" "$<BOOL:${GUI}>"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "Generating TTL files"
  )

  add_library(neuralblender_lv2 MODULE
    src/lv2.cpp
  )

  target_compile_options(neuralblender_lv2 PRIVATE
    -fpermissive
  )
  
  target_link_libraries(neuralblender_lv2 PRIVATE neuralblender_core)
  target_compile_definitions(neuralblender_lv2 PRIVATE LV2)
  add_dependencies(neuralblender_lv2 generate_ttl)

  set_target_properties(neuralblender_lv2 PROPERTIES
    PREFIX ""
    OUTPUT_NAME "neuralblender"
  )

  install(TARGETS neuralblender_lv2
    LIBRARY DESTINATION lib/lv2/neuralblender.lv2
  )

  install(FILES
     ${CMAKE_CURRENT_BINARY_DIR}/lv2/manifest.ttl
     ${CMAKE_CURRENT_BINARY_DIR}/lv2/neuralblender.ttl
    DESTINATION lib/lv2/neuralblender.lv2
  )

  if (GUI)
    pkg_check_modules(X11 REQUIRED x11)
    message(STATUS "X11: found")

    add_library(neuralblender_ui MODULE
      src/ui.cpp
    )
    
    set_target_properties(neuralblender_ui PROPERTIES
      PREFIX ""
      OUTPUT_NAME "neuralblender_ui"
    )

    #target_link_libraries(neuralblender_ui PRIVATE xputty)
    target_compile_definitions(neuralblender_ui PRIVATE LV2_UI)
    target_include_directories(neuralblender_ui PRIVATE ${X11_INCLUDE_DIRS})
    target_link_libraries(neuralblender_ui PRIVATE ${X11_LIBRARIES})

    install(TARGETS neuralblender_ui
      LIBRARY DESTINATION lib/lv2/neuralblender.lv2
    )
  endif()
endif()
