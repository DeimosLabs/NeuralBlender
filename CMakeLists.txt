cmake_minimum_required(VERSION 3.10)

project(neuralblender
  VERSION 0.1.0
  DESCRIPTION "Amp modeling plugin based on RTNeural and NAM"
  LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(STANDALONE     "Build standalone JACK client" ON)
option(LV2            "Build LV2 plugin" ON)
option(GUI            "Enable GUI" ON)
option(DEBUG          "Debug build" OFF)
option(TRACE          "Debug (mostly) UI code" OFF)
option(NOASAN         "Disable asan on debug build" ON)

find_package(PkgConfig REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(Threads REQUIRED)

include(cmake/NeuralBlenderGui.cmake)

if (TRACE)
  add_compile_definitions(CMDLINE_DEBUG)
endif()

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Timestamp header
set(TIMESTAMP_FILE ${CMAKE_CURRENT_BINARY_DIR}/timestamp.h)
set_source_files_properties(${TIMESTAMP_FILE} PROPERTIES GENERATED TRUE)
set(INLINE_DATA_CPP ${CMAKE_CURRENT_BINARY_DIR}/data/data.cpp)
set(INLINE_DATA_H ${CMAKE_CURRENT_BINARY_DIR}/data/data.h)
set_source_files_properties(${INLINE_DATA_CPP} ${INLINE_DATA_H} PROPERTIES GENERATED TRUE)

add_custom_target(generate_timestamp ALL
  COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/timestamp.sh" "${TIMESTAMP_FILE}"
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  COMMENT "Updating build timestamp"
)

execute_process (COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/timestamp.sh
                 WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/data/generate_inline_data.list" INLINE_DATA_LIST_LINES)
set(INLINE_DATA_INPUTS
  "${CMAKE_CURRENT_SOURCE_DIR}/data/generate_inline_data.sh"
  "${CMAKE_CURRENT_SOURCE_DIR}/data/generate_inline_data.list"
)
foreach(INLINE_DATA_LINE ${INLINE_DATA_LIST_LINES})
  string(REGEX REPLACE "#.*$" "" INLINE_DATA_LINE "${INLINE_DATA_LINE}")
  string(STRIP "${INLINE_DATA_LINE}" INLINE_DATA_LINE)
  if (INLINE_DATA_LINE)
    string(REGEX REPLACE "[ \t]+" ";" INLINE_DATA_FILES "${INLINE_DATA_LINE}")
    foreach(INLINE_DATA_FILE ${INLINE_DATA_FILES})
      if (IS_ABSOLUTE "${INLINE_DATA_FILE}")
        list(APPEND INLINE_DATA_INPUTS "${INLINE_DATA_FILE}")
      else()
        list(APPEND INLINE_DATA_INPUTS "${CMAKE_CURRENT_SOURCE_DIR}/data/${INLINE_DATA_FILE}")
      endif()
    endforeach()
  endif()
endforeach()

add_custom_command(
  OUTPUT ${INLINE_DATA_CPP} ${INLINE_DATA_H}
  COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/data/generate_inline_data.sh"
  DEPENDS ${INLINE_DATA_INPUTS}
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  COMMENT "Generating inline data"
  VERBATIM
)

add_custom_target(generate_inline_data
  DEPENDS ${INLINE_DATA_CPP} ${INLINE_DATA_H}
)

add_library(neuralblender_cmdline_debug STATIC
  external/cmdline/cmdline_debug.cpp
)

add_library(neuralblender_config STATIC
  src/config.cpp
)

set_target_properties(neuralblender_cmdline_debug PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)

set_target_properties(neuralblender_config PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)

target_include_directories(neuralblender_cmdline_debug PUBLIC
  ${PROJECT_SOURCE_DIR}/external/cmdline
)

target_include_directories(neuralblender_config PUBLIC
  ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(neuralblender_config PUBLIC
  neuralblender_cmdline_debug
)

if (TRACE)
  target_compile_definitions(neuralblender_cmdline_debug PUBLIC CMDLINE_DEBUG_TIMESTAMPS)
endif()

add_library(neuralblender_data OBJECT
  ${INLINE_DATA_CPP}
)

add_dependencies(neuralblender_data generate_inline_data)

set_target_properties(neuralblender_data PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)

target_include_directories(neuralblender_data PUBLIC
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/data
)

add_library(neuralblender_core OBJECT
  src/neuralblender.cpp
  src/gzip.cpp
  
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

add_library(neuralblender_meter_data OBJECT
  src/meter.cpp
)

set_target_properties(neuralblender_core PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)

set_target_properties(neuralblender_meter_data PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)

add_dependencies(neuralblender_core generate_timestamp)
add_dependencies(neuralblender_core generate_inline_data)
add_dependencies(neuralblender_meter_data generate_timestamp)

target_include_directories(neuralblender_core PUBLIC
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/data
  ${PROJECT_SOURCE_DIR}/external
  ${PROJECT_SOURCE_DIR}/external/RTNeural
  ${PROJECT_SOURCE_DIR}/external/NAM
  ${PROJECT_SOURCE_DIR}/external/nlohmann
  ${PROJECT_SOURCE_DIR}
)

target_include_directories(neuralblender_meter_data PUBLIC
  ${CMAKE_CURRENT_BINARY_DIR}
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_SOURCE_DIR}/src
)

target_compile_definitions(neuralblender_core PUBLIC
  NEURALBLENDER_VERSION="${PROJECT_VERSION}"
  NAM_SAMPLE_FLOAT
  DSP_SAMPLE_FLOAT
)

target_compile_definitions(neuralblender_meter_data PRIVATE
  METER_DATA_ONLY
)

target_link_libraries(neuralblender_core PUBLIC Eigen3::Eigen z neuralblender_cmdline_debug neuralblender_config)

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
  pkg_check_modules(JACK IMPORTED_TARGET jack)

  if (JACK_FOUND)
    message(STATUS "JACK: found")

    add_executable(neuralblender
      src/standalone.cpp
      src/timestamp.cpp
      $<TARGET_OBJECTS:neuralblender_data>
      $<TARGET_OBJECTS:neuralblender_meter_data>
    )
    
    target_link_libraries(neuralblender PRIVATE
      neuralblender_core
      neuralblender_cmdline_debug
      Threads::Threads
      PkgConfig::JACK
    )

    target_compile_definitions(neuralblender PRIVATE STANDALONE)

    install(TARGETS neuralblender RUNTIME DESTINATION bin)
  else()
    message(WARNING "JACK development files not found, standalone target disabled")
  endif()
endif()

if (GUI)
  pkg_check_modules(X11 REQUIRED IMPORTED_TARGET x11)
  pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET cairo)
  message(STATUS "X11: found")
  message(STATUS "Cairo: found")

  add_neuralblender_xputty()
  add_neuralblender_gui_library(neuralblender_gui)
endif()

if (TARGET neuralblender AND GUI)
  target_link_libraries(neuralblender PRIVATE neuralblender_gui)
  target_compile_definitions(neuralblender PRIVATE GUI HAVE_GUI=1)
endif()
  
if (LV2)
  pkg_check_modules(LV2_DEPS REQUIRED IMPORTED_TARGET lv2)
  message(STATUS "LV2: found")

  add_custom_target(generate_ttl ALL
    COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/lv2/generate_ttl.sh" "$<BOOL:${GUI}>"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "Generating TTL files"
  )

  add_library(neuralblender_lv2 MODULE
    src/lv2.cpp
    #src/config.cpp
    src/timestamp.cpp
    $<TARGET_OBJECTS:neuralblender_data>
    $<TARGET_OBJECTS:neuralblender_meter_data>
  )

  target_compile_options(neuralblender_lv2 PRIVATE
    -fpermissive
  )
  
  target_link_libraries(neuralblender_lv2 PRIVATE
    neuralblender_core
    neuralblender_cmdline_debug
    Threads::Threads
    PkgConfig::LV2_DEPS
  )
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

  if(GUI)    
    add_neuralblender_gui_library(neuralblender_gui_lv2 LV2)

    add_library(neuralblender_ui MODULE
      src/lv2_ui.cpp
      $<TARGET_OBJECTS:neuralblender_data>
      $<TARGET_OBJECTS:neuralblender_meter_data>
    )

    set_target_properties(neuralblender_ui PROPERTIES
      PREFIX ""
      OUTPUT_NAME "neuralblender_ui"
    )
    
    target_link_libraries(neuralblender_ui PRIVATE
      neuralblender_gui_lv2
      neuralblender_cmdline_debug
      neuralblender_config
      PkgConfig::LV2_DEPS
    )
    target_compile_definitions(neuralblender_ui PRIVATE LV2 LV2_UI)

    install(TARGETS neuralblender_ui
      LIBRARY DESTINATION lib/lv2/neuralblender.lv2
    )
  endif()
endif()
