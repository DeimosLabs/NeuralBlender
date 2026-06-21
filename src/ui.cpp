
/* NeuralBlender - RTNeural / NAM based amp modeler
 *
 * Shared UI code
 */
 
#include "ui.h"

#ifdef CMDLINE_DEBUG
#include "cmdline/cmdline.h"
#define debug(...) cmdline_debug(stderr,ANSI_MAGENTA,__FILE__,__LINE__,__FUNC__,__VA_ARGS__)
#else
#define debug(...)
#define CP
#define BP
#endif

c_neuralblender_ui::c_neuralblender_ui () {
  display = NULL;
  window = 0;
}

c_neuralblender_ui::~c_neuralblender_ui () {
  destroy ();
}

bool c_neuralblender_ui::create (Window parent) { CP
  destroy ();

  display = XOpenDisplay (NULL);
  if (!display)
    return false;

  if (!parent)
    parent = DefaultRootWindow (display);
  
  const int screen = DefaultScreen (display);
  window = XCreateSimpleWindow (
    display,
    parent,
    0, 0,
    640, 480,
    0,
    BlackPixel (display, screen),
    0x202020);

  XStoreName (display, window, "NeuralBlender");
  XMapWindow (display, window);
  XFlush (display);
  
  return true;
}

void c_neuralblender_ui::destroy () { CP
  if (display) {
    if (window)
      XDestroyWindow (display, window);
    XCloseDisplay (display);
  }

  display = NULL;
  window = 0;
}
