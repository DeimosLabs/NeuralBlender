function(add_neuralblender_xputty)
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
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
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
	  )
	
	  target_link_libraries(xputty PUBLIC
	    PkgConfig::X11
	    PkgConfig::CAIRO
	  )
	endfunction()

function(add_neuralblender_gui_library target_name)
  add_library(${target_name} STATIC
    src/ui.cpp
    src/widgets.cpp
    src/timestamp.cpp
    src/meter.cpp
    src/tuner.cpp
  )

  add_dependencies(${target_name} generate_inline_data)

  set_target_properties(${target_name} PROPERTIES
    POSITION_INDEPENDENT_CODE ON
  )

  target_include_directories(${target_name} PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
  )

  target_link_libraries(${target_name} PUBLIC
    neuralblender_core
    Eigen3::Eigen
    neuralblender_cmdline_debug
    neuralblender_config
    xputty
  )
  
  if (ARGN)
    target_compile_definitions(${target_name} PRIVATE ${ARGN})
  endif()
endfunction()
