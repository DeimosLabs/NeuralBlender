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

execute_process (COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/timestamp.sh
                 WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

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
add_dependencies(neuralblender_meter_data generate_timestamp)

target_include_directories(neuralblender_core PUBLIC
  ${CMAKE_CURRENT_BINARY_DIR}
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

target_link_libraries(neuralblender_core PUBLIC Eigen3::Eigen z)

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
      src/config.cpp
      src/timestamp.cpp
      $<TARGET_OBJECTS:neuralblender_meter_data>
    )
    
    target_include_directories(neuralblender PRIVATE ${JACK_INCLUDE_DIRS})
    target_link_libraries(neuralblender PRIVATE neuralblender_core ${JACK_LIBRARIES})

    target_compile_definitions(neuralblender PRIVATE STANDALONE)

    install(TARGETS neuralblender RUNTIME DESTINATION bin)
  else()
    message(WARNING "JACK development files not found, standalone target disabled")
  endif()
endif()

if (GUI)
  pkg_check_modules(X11 REQUIRED x11)
  pkg_check_modules(CAIRO REQUIRED cairo)
  message(STATUS "X11: found")
  message(STATUS "Cairo: found")

  add_custom_target(generate_inline_data ALL
    COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/data/generate_inline_data.sh"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "Generating inline data"
  )

  execute_process (COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/data/generate_inline_data.sh
                   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  add_library(neuralblender_data OBJECT
    ${CMAKE_CURRENT_BINARY_DIR}/data/data.cpp
  )

  add_dependencies(neuralblender_data generate_inline_data)

  set_target_properties(neuralblender_data PROPERTIES
    POSITION_INDEPENDENT_CODE ON
  )

  target_include_directories(neuralblender_data PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
  )

  add_library(xputty STATIC
    external/libxputty/xputty/xputty.c
    external/libxputty/xputty/xwidget.c
    external/libxputty/xputty/xwidget_private.c
    external/libxputty/xputty/xwidget-linux.c
    external/libxputty/xputty/xadjustment.c
    external/libxputty/xputty/xadjustment_private.c
    external/libxputty/xputty/xchildlist.c
    external/libxputty/xputty/xchildlist_private.c
    external/libxputty/xputty/xcolor.c
    external/libxputty/xputty/xpngloader.c
    external/libxputty/xputty/widgets/xframe.c
    external/libxputty/xputty/widgets/xframe_private.c
    external/libxputty/xputty/widgets/xlabel.c
    external/libxputty/xputty/widgets/xlabel_private.c
    external/libxputty/xputty/widgets/xbutton.c
    external/libxputty/xputty/widgets/xbutton_private.c
    external/libxputty/xputty/widgets/xknob.c
    external/libxputty/xputty/widgets/xknob_private.c
    external/libxputty/xputty/widgets/xcombobox.c
    external/libxputty/xputty/widgets/xcombobox_private.c
    external/libxputty/xputty/widgets/xtooltip.c
    external/libxputty/xputty/widgets/xtooltip_private.c
    external/libxputty/xputty/widgets/xslider.c
    external/libxputty/xputty/widgets/xslider_private.c
    external/libxputty/xputty/widgets/xdrawing_area.c
    external/libxputty/xputty/widgets/xdrawing_area_private.c
    external/libxputty/xputty/dialogs/xfile-dialog.c
    external/libxputty/xputty/xfilepicker.c
    external/libxputty/xputty/xsvgloader.c
    external/libxputty/xputty/widgets/xlistview.c
    external/libxputty/xputty/widgets/xlistview_private.c
    external/libxputty/xputty/widgets/xmultilistview.c
    external/libxputty/xputty/widgets/xmultilistview_private.c
    external/libxputty/xputty/dialogs/xmessage-dialog.c
    external/libxputty/xputty/b64_encode.c
    external/libxputty/xputty/xdgmime/xdgmime.c
    external/libxputty/xputty/xdgmime/xdgmimealias.c
    external/libxputty/xputty/xdgmime/xdgmimecache.c
    external/libxputty/xputty/xdgmime/xdgmimeglob.c
    external/libxputty/xputty/xdgmime/xdgmimeicon.c
    external/libxputty/xputty/xdgmime/xdgmimeint.c
    external/libxputty/xputty/xdgmime/xdgmimemagic.c
    external/libxputty/xputty/xdgmime/xdgmimeparent.c

  )

  set_target_properties(xputty PROPERTIES
    POSITION_INDEPENDENT_CODE ON
  )

  add_dependencies(xputty generate_inline_data)

  target_compile_definitions(xputty PRIVATE
    BUILD_NEURALBLENDER
  )

  target_include_directories(xputty PUBLIC
    ${PROJECT_SOURCE_DIR}/
    ${PROJECT_SOURCE_DIR}/data
    ${PROJECT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/data
    ${CMAKE_CURRENT_BINARY_DIR}/src
    ${PROJECT_SOURCE_DIR}/external/libxputty
    ${PROJECT_SOURCE_DIR}/external/libxputty/xputty/header
    ${PROJECT_SOURCE_DIR}/external/libxputty/xputty/header/widgets
    ${PROJECT_SOURCE_DIR}/external/libxputty/xputty/header/dialogs
    ${PROJECT_SOURCE_DIR}/external/libxputty/xputty/xdgmime
    ${PROJECT_SOURCE_DIR}/external
    ${PROJECT_SOURCE_DIR}/external/RTNeural
    ${PROJECT_SOURCE_DIR}/external/NAM
    ${PROJECT_SOURCE_DIR}/external/nlohmann
    ${X11_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
  )

  target_compile_options(xputty PUBLIC
    ${X11_CFLAGS_OTHER}
    ${CAIRO_CFLAGS_OTHER}
  )

  target_link_libraries(xputty PUBLIC
    ${X11_LIBRARIES}
    ${CAIRO_LIBRARIES}
  )

  add_library(neuralblender_gui STATIC
    src/ui.cpp
    src/timestamp.cpp
    src/meter.cpp
    $<TARGET_OBJECTS:neuralblender_data>
  )

  add_dependencies(neuralblender_gui generate_inline_data)

  set_target_properties(neuralblender_gui PROPERTIES
    POSITION_INDEPENDENT_CODE ON
  )

  target_include_directories(neuralblender_gui PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
  )

  target_link_libraries(neuralblender_gui PUBLIC
    Eigen3::Eigen
    xputty
  )
endif()

if (TARGET neuralblender AND GUI)
  target_link_libraries(neuralblender PRIVATE neuralblender_gui)
  target_compile_definitions(neuralblender PRIVATE GUI HAVE_GUI=1)
endif()
  
if (LV2)
  add_custom_target(generate_ttl ALL
    COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/lv2/generate_ttl.sh" "$<BOOL:${GUI}>"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMMENT "Generating TTL files"
  )

  add_library(neuralblender_lv2 MODULE
    src/lv2.cpp
    src/timestamp.cpp
    $<TARGET_OBJECTS:neuralblender_meter_data>
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

  if(GUI)    
    add_library(neuralblender_ui MODULE
      src/lv2_ui.cpp
      src/config.cpp
      $<TARGET_OBJECTS:neuralblender_meter_data>
    )

    set_target_properties(neuralblender_ui PROPERTIES
      PREFIX ""
      OUTPUT_NAME "neuralblender_ui"
    )
    
    target_link_libraries(neuralblender_ui PRIVATE neuralblender_gui)
    target_compile_definitions(neuralblender_ui PRIVATE LV2_UI)

    install(TARGETS neuralblender_ui
      LIBRARY DESTINATION lib/lv2/neuralblender.lv2
    )
  endif()
endif()
