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

#include "ui.h"

#define NB_UI_URI "http://deimos.ca/neuralblender#ui"

#ifdef CMDLINE_DEBUG
#define CMDLINE_IMPLEMENTATION // separate .so file for UI, so we need this here
#include "cmdline/cmdline.h"
#define debug(...) cmdline_debug(stderr,ANSI_DARK_MAGENTA,__FILE__,__LINE__,__FUNC__,__VA_ARGS__)
#else
#define debug(...)
#define CP
#define BP
#endif

class c_lv2_ui : public c_neuralblender_ui {
public:
  LV2UI_Write_Function write;
  LV2UI_Controller controller;
  
  bool load_model (size_t which, const char *filename) { CP; return false; }
  void on_gain_in (c_widget *w, float f)               { CP }
  void on_gain_out (c_widget *w, float f)              { CP }
  void on_delay (c_widget *w, float f)                 { CP }
  void on_filebrowse (c_widget *w)                     { CP }
  void on_fileselected (c_widget *w, const char *path) { CP }
  void on_fileclear (c_widget *w)                      { CP }
  void on_mute (c_widget *w, bool b)                   { CP }
  void on_bypass (c_widget *w, bool b)                 { CP }
  void on_about (c_widget *w)                          { CP }
};

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
  c_lv2_ui *ui = new c_lv2_ui;
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

  if (!ui->create (parent)) {
    delete ui;
    return NULL;
  }

  if (widget)
    *widget = (LV2UI_Widget) (uintptr_t) ui->window;
  
  return (LV2UI_Handle) ui;
}

static void cleanup (LV2UI_Handle handle) { CP
  c_lv2_ui *ui = (c_lv2_ui *) handle;
  if (!ui)
    return;
  
  CP
  delete ui;
  CP
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

static int idle (LV2UI_Handle handle) {
  c_lv2_ui *ui = (c_lv2_ui *) handle;
  if (!ui)
    return 0;
  
  return ui->idle ();
}

static const LV2UI_Idle_Interface idle_iface = { idle };

static const void *extension_data (const char *uri) {
  if (!strcmp (uri, LV2_UI__idleInterface))
    return &idle_iface;
  return NULL;
}

static const LV2UI_Descriptor descriptor = {
  NB_UI_URI,
  instantiate,
  cleanup,
  port_event,
  extension_data
};

extern "C" const LV2UI_Descriptor *lv2ui_descriptor (uint32_t index) {
  return index == 0 ? &descriptor : NULL;
}
