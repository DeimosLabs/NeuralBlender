/* NeuralBlender - RTNeural / NAM based amp modeler
 *
 * LV2 UI skeleton. Real widgets will be added here once the xputty
 * integration is wired in.
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <lv2/ui/ui.h>
#include <X11/Xlib.h>

#define NB_UI_URI "http://deimos.ca/neuralblender#ui"

#ifdef CMDLINE_DEBUG
#define CMDLINE_IMPLEMENTATION
#include "cmdline/cmdline.h"
#define debug(...) cmdline_debug(stderr,ANSI_MAGENTA,__FILE__,__LINE__,__FUNC__,__VA_ARGS__)
#else
#define debug(...)
#define CP
#define BP
#endif

typedef struct {
  LV2UI_Write_Function write;
  LV2UI_Controller controller;
  Display *display;
  Window window;
} NeuralBlenderUI;

static LV2UI_Handle instantiate (
  const LV2UI_Descriptor *descriptor,
  const char *plugin_uri,
  const char *bundle_path,
  LV2UI_Write_Function write_function,
  LV2UI_Controller controller,
  LV2UI_Widget *widget,
  const LV2_Feature *const *features) { CP
    
  (void) descriptor;
  (void) plugin_uri;
  (void) bundle_path;
  NeuralBlenderUI *ui = (NeuralBlenderUI *) calloc (1, sizeof (NeuralBlenderUI));
  if (!ui)
    return NULL;

  ui->write = write_function;
  ui->controller = controller;

  Window parent = 0;
  for (int i = 0; features && features [i]; ++i) {
    if (!strcmp (features [i]->URI, LV2_UI__parent)) {
      parent = (Window) (uintptr_t) features [i]->data;
      break;
    }
  }

  ui->display = XOpenDisplay (NULL);
  if (!ui->display) {
    free (ui);
    return NULL;
  }

  if (!parent)
    parent = DefaultRootWindow (ui->display);

  const int screen = DefaultScreen (ui->display);
  ui->window = XCreateSimpleWindow (
    ui->display,
    parent,
    0, 0,
    640, 480,
    0,
    BlackPixel (ui->display, screen),
    0x202020);

  XStoreName (ui->display, ui->window, "NeuralBlender");
  XMapWindow (ui->display, ui->window);
  XFlush (ui->display);

  if (widget)
    *widget = (LV2UI_Widget) (uintptr_t) ui->window;

  return (LV2UI_Handle) ui;
}

static void cleanup (LV2UI_Handle handle) { CP
  NeuralBlenderUI *ui = (NeuralBlenderUI *) handle;
  if (!ui)
    return;

  if (ui->display) {
    if (ui->window)
      XDestroyWindow (ui->display, ui->window);
    XCloseDisplay (ui->display);
  }

  free (ui);
}

static void port_event (
  LV2UI_Handle handle,
  uint32_t port_index,
  uint32_t buffer_size,
  uint32_t format,
  const void *buffer) { CP
  
  (void) handle;
  (void) port_index;
  (void) buffer_size;
  (void) format;
  (void) buffer;
}

static const LV2UI_Descriptor descriptor = {
  NB_UI_URI,
  instantiate,
  cleanup,
  port_event,
  NULL
};

extern "C" const LV2UI_Descriptor *lv2ui_descriptor (uint32_t index) {
  return index == 0 ? &descriptor : NULL;
}
