
#pragma once

/*

workaround for resource system in xputty
included from xputty/header/xresources.h when BUILD_NEURALBLENDER is defined

*/

#include "data.h"

#ifdef LDVAR
#undef LDVAR
#endif
#define LDVAR(x) x

#define approved_png             data_xputty_approved_png
#define arrow_png                data_xputty_arrow_png
#define cancel_png               data_xputty_cancel_png
#define choice_png               data_xputty_choice_png
#define colors_png               data_xputty_colors_png
#define directory_png            data_xputty_directory_png
#define directory_open_png       data_xputty_directory_open_png
#define directory_select_png     data_xputty_directory_select_png
#define eject_png                data_xputty_eject_png
#define error_png                data_xputty_error_png
#define exit_png                 data_xputty_exit_png
#define exit__png                data_xputty_exit__png
#define file_png                 data_xputty_file_png
#define gear_png                 data_xputty_gear_png
#define grid_png                 data_xputty_grid_png
#define image_directory_png      data_xputty_image_directory_png
#define info_png                 data_xputty_info_png
#define menu_png                 data_xputty_menu_png
#define message_png              data_xputty_message_png
#define midikeyboard_png         data_xputty_midikeyboard_png
#define norm_png                 data_xputty_norm_png
#define question_png             data_xputty_question_png
#define save_png                 data_xputty_save_png
#define screenshot_ardour_png    data_xputty_screenshot_ardour_png
#define settings_png             data_xputty_settings_png
#define warning_png              data_xputty_warning_png
#define xputty_logo_png          data_xputty_xputty_logo_png                   
