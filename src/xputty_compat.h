/* Keep xputty's min/max macros out of C++ code. */

#pragma once

#include "xputty.h"
#include "xwidgets.h"
#include "dialogs/xfile-dialog.h"

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif
