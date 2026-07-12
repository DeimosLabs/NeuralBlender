
/* NeuralBlender - tuner widget.
 * Code for c_customwidget used for c_meter and here, originally written
 * for wxWidgets, translated to cairo by codex.
 */

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <X11/Xlib.h>

#include "xputty_compat.h"
#include "tuner.h"

#define CMDLINE_DEBUG_COLOR ANSI_DARK_GREEN
#include "cmdline_debug.h"

c_tunerwidget::c_tunerwidget () { CP }
c_tunerwidget::~c_tunerwidget () { CP }

void c_tunerwidget::create (Widget_t *parent, const char *label,
                            int x, int y, int w, int h) {
  CP                            
}
