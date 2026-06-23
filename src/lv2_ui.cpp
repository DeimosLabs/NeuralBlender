/* NeuralBlender - RTNeural / NAM based amp modeler
 *
 * LV2 UI skeleton. Real widgets will be added here once the xputty
 * integration is wired in.
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <lv2/ui/ui.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/forge.h>
#include <lv2/atom/util.h>
#include <lv2/patch/patch.h>
#include <lv2/urid/urid.h>

#include "neuralblender.h"

#include <X11/Xlib.h>

#include "ui.h"

#define NB_UI_URI "http://deimos.ca/neuralblender#ui"

enum {
  PORT_AUDIO_IN = 0,
  PORT_AUDIO_OUT,

  PORT_BYPASS,

  PORT_A_GAIN_IN,
  PORT_A_GAIN_OUT,
  PORT_A_DELAY,
  PORT_A_MUTE,

  PORT_B_GAIN_IN,
  PORT_B_GAIN_OUT,
  PORT_B_DELAY,
  PORT_B_MUTE,

  PORT_C_GAIN_IN,
  PORT_C_GAIN_OUT,
  PORT_C_DELAY,
  PORT_C_MUTE,

  PORT_D_GAIN_IN,
  PORT_D_GAIN_OUT,
  PORT_D_DELAY,
  PORT_D_MUTE,

  PORT_CONTROL,
  PORT_NOTIFY
};

#ifdef CMDLINE_DEBUG
#define CMDLINE_IMPLEMENTATION // separate .so file for UI, so we need this here
#include "cmdline/cmdline.h"
#define debug(...) cmdline_debug(stderr,ANSI_DARK_RED,__FILE__,__LINE__,__FUNC__,__VA_ARGS__)
#else
#define debug(...)
#define CP
#define BP
#endif

class c_lv2_ui : public c_neuralblender_ui {
public:
  LV2UI_Write_Function write;
  LV2UI_Controller controller;
  LV2_URID_Map *map = NULL;
  LV2_Atom_Forge forge;
  LV2_URID urid_atom_eventTransfer = 0;
  LV2_URID urid_patch_Set = 0;
  LV2_URID urid_patch_property = 0;
  LV2_URID urid_patch_value = 0;
  LV2_URID urid_model [NB_UI_MAX_LANES] = { 0 };
  bool updating_from_host = false;
  
  void write_control (uint32_t port, float value) {
    if (updating_from_host || !write)
      return;
    write (controller, port, sizeof (value), 0, &value);
  }

  uint32_t lane_port (size_t lane, uint32_t first) const {
    return first + (uint32_t) lane * 4;
  }

  bool write_model_path (size_t which, const char *filename) {
    if (updating_from_host || !write || !map || which >= NB_UI_MAX_LANES)
      return false;

    const char *path = filename ? filename : "";
    uint8_t buf [4096];
    LV2_Atom_Forge_Frame frame;

    lv2_atom_forge_set_buffer (&forge, buf, sizeof (buf));
    lv2_atom_forge_object (&forge, &frame, 0, urid_patch_Set);
    lv2_atom_forge_key (&forge, urid_patch_property);
    lv2_atom_forge_urid (&forge, urid_model [which]);
    lv2_atom_forge_key (&forge, urid_patch_value);
    if (path [0])
      lv2_atom_forge_path (&forge, path, strlen (path) + 1);
    else
      lv2_atom_forge_string (&forge, "", 1);
    lv2_atom_forge_pop (&forge, &frame);

    const LV2_Atom *atom = (const LV2_Atom *) buf;
    write (controller,
           PORT_CONTROL,
           lv2_atom_total_size (atom),
           urid_atom_eventTransfer,
           atom);
    return path [0] != '\0';
  }

  bool load_model (size_t which, const char *filename) { CP; return write_model_path (which, filename); }
  void on_gain_in (c_widget *w, float f)               { CP; write_control (lane_port (w->lane, PORT_A_GAIN_IN), gain_to_db (f)); }
  void on_gain_out (c_widget *w, float f)              { CP; write_control (lane_port (w->lane, PORT_A_GAIN_OUT), gain_to_db (f)); }
  void on_delay (c_widget *w, float f)                 { CP; write_control (lane_port (w->lane, PORT_A_DELAY), f); }
  void on_filebrowse (c_widget *w)                     { CP }
  void on_fileselected (c_widget *w, const char *path) { CP }
  void on_fileclear (c_widget *w)                      { CP; write_model_path (w->lane, ""); }
  void on_mute (c_widget *w, bool b)                   { CP; write_control (lane_port (w->lane, PORT_A_MUTE), b ? 1.0f : 0.0f); }
  void on_bypass (c_widget *w, bool b)                 { CP; write_control (PORT_BYPASS, b ? 1.0f : 0.0f); }
  void on_about (c_widget *w)                          { CP }

  void set_port_value (uint32_t port, float value) {
    updating_from_host = true;

    if (port == PORT_BYPASS) {
      btn_enable.value = value >= 0.5f;
      btn_enable.set_value (btn_enable.value);
      btn_enable.set_label (btn_enable.value ? "Enabled" : "Bypass");
      updating_from_host = false;
      return;
    }

    for (size_t lane = 0; lane < NB_UI_MAX_LANES; lane++) {
      const uint32_t base = PORT_A_GAIN_IN + (uint32_t) lane * 4;
      if (port == base) {
        lanes [lane].gain_in.set_value (value);
        break;
      } else if (port == base + 1) {
        lanes [lane].gain_out.set_value (value);
        break;
      } else if (port == base + 2) {
        lanes [lane].delay.set_value (value);
        break;
      } else if (port == base + 3) {
        lanes [lane].btn_mute.value = value >= 0.5f;
        lanes [lane].btn_mute.set_value (lanes [lane].btn_mute.value);
        break;
      }
    }

    updating_from_host = false;
  }
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
    } else if (!strcmp (features [i]->URI, LV2_URID__map)) {
      ui->map = (LV2_URID_Map *) features [i]->data;
    }
  }

  if (ui->map) {
    lv2_atom_forge_init (&ui->forge, ui->map);
    ui->urid_atom_eventTransfer = ui->map->map (ui->map->handle, LV2_ATOM__eventTransfer);
    ui->urid_patch_Set = ui->map->map (ui->map->handle, LV2_PATCH__Set);
    ui->urid_patch_property = ui->map->map (ui->map->handle, LV2_PATCH__property);
    ui->urid_patch_value = ui->map->map (ui->map->handle, LV2_PATCH__value);
    ui->urid_model [0] = ui->map->map (ui->map->handle, "http://deimos.ca/neuralblender#ModelA");
    ui->urid_model [1] = ui->map->map (ui->map->handle, "http://deimos.ca/neuralblender#ModelB");
    ui->urid_model [2] = ui->map->map (ui->map->handle, "http://deimos.ca/neuralblender#ModelC");
    ui->urid_model [3] = ui->map->map (ui->map->handle, "http://deimos.ca/neuralblender#ModelD");
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
  
  c_lv2_ui *ui = (c_lv2_ui *) handle;
  if (!ui || !buffer)
    return;

  if (format == 0 && buffer_size == sizeof (float)) {
    const float value = *(const float *) buffer;
    ui->set_port_value (port_index, value);
    return;
  }
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
