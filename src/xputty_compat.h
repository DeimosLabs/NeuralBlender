/* Keep xputty's min/max macros out of C++ code. */

#pragma once

#include "xputty.h"
#include "xwidgets.h"
#include "dialogs/xfile-dialog.h"

typedef Widget_t *nbtk_native_handle_t;
typedef Window nbtk_native_window_t;

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif
