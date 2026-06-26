
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
  PORT_NOTIFY,
  PORT_VU_ENABLE,
  PORT_MUTE_ALL
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
  LV2_URID urid_atom_Path = 0;
  LV2_URID urid_atom_String = 0;
  LV2_URID urid_atom_URID = 0;
  LV2_URID urid_atom_Float = 0;
  LV2_URID urid_atom_Vector = 0;
  LV2_URID urid_patch_Set = 0;
  LV2_URID urid_patch_Get = 0;
  LV2_URID urid_patch_property = 0;
  LV2_URID urid_patch_value = 0;
  LV2_URID urid_model [NB_UI_MAX_LANES] = { 0 };
  LV2_URID urid_meters = 0;
  LV2UI_Port_Subscribe *subscribe = NULL;
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

  void request_current_state () {
    if (!write || !map)
      return;

    uint8_t buf [256];
    LV2_Atom_Forge_Frame frame;

    lv2_atom_forge_set_buffer (&forge, buf, sizeof (buf));
    lv2_atom_forge_object (&forge, &frame, 0, urid_patch_Get);
    lv2_atom_forge_pop (&forge, &frame);

    const LV2_Atom *atom = (const LV2_Atom *) buf;
    write (controller,
           PORT_CONTROL,
           lv2_atom_total_size (atom),
           urid_atom_eventTransfer,
           atom);
  }

  bool load_model (size_t which, const char *filename) { CP; return write_model_path (which, filename); }
  void on_gain_in (c_widget *w, float f)               { CP; write_control (lane_port (w->lane, PORT_A_GAIN_IN), gain_to_db (f)); }
  void on_gain_out (c_widget *w, float f)              { CP; write_control (lane_port (w->lane, PORT_A_GAIN_OUT), gain_to_db (f)); }
  void on_delay (c_widget *w, float f)                 { CP; write_control (lane_port (w->lane, PORT_A_DELAY), f); }
  void on_filebrowse (c_widget *w)                     { CP }
  void on_fileselected (c_widget *w, const char *path) { CP }
  void on_fileclear (c_widget *w)                      { CP; clear_lane_model_ui (w->lane); write_model_path (w->lane, ""); }
  void on_mute (c_widget *w, bool b)                   { CP; write_control (lane_port (w->lane, PORT_A_MUTE), b ? 1.0f : 0.0f); }
  void on_muteall (c_widget *w, bool b)                { CP; write_control (PORT_MUTE_ALL, b ? 1.0f : 0.0f); }
  void on_excl (c_widget *w, int n)                    { CP }
  void on_bypass (c_widget *w, bool b)                 { CP; write_control (PORT_BYPASS, b ? 1.0f : 0.0f); }
  void on_about (c_widget *w)                          { CP }
  void on_vu (c_widget *w, bool b)                     { CP; write_control (PORT_VU_ENABLE, b ? 1.0f : 0.0f); }

  void set_port_value (uint32_t port, float value) {
    updating_from_host = true;
    updating_from_state = true;

    if (port == PORT_BYPASS) {
      const bool enabled = value >= 0.5f;
      btn_enable.set_value (enabled);
      btn_enable.set_label (enabled ? "Enabled" : "Bypass");
      updating_from_state = false;
      updating_from_host = false;
      return;
    }

    if (port == PORT_VU_ENABLE) {
      btn_vu.set_value (value >= 0.5f);
      vu_on (value >= 0.5f);
      updating_from_state = false;
      updating_from_host = false;
      return;
    }

    if (port == PORT_MUTE_ALL) {
      btn_muteall.set_value (value >= 0.5f);
      updating_from_state = false;
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
        lanes [lane].btn_mute.set_value (value >= 0.5f);
        break;
      }
    }

    updating_from_state = false;
    updating_from_host = false;
  }

  void set_model_path (size_t which, const char *path) {
    if (which >= NB_UI_MAX_LANES)
      return;

    const char *p = path ? path : "";
    c_neuralblender_state state;
    state.bypass = !btn_enable.value;
    for (size_t i = 0; i < NB_UI_MAX_LANES; ++i) {
      state.lanes [i].filename = filepickers [i].selected_file;
      state.lanes [i].gain_in = db_to_gain (lanes [i].gain_in.value);
      state.lanes [i].gain_out = db_to_gain (lanes [i].gain_out.value);
      state.lanes [i].delay_ms = lanes [i].delay.value;
      state.lanes [i].lane_mute = lanes [i].btn_mute.value;
      state.lanes [i].loaded = !state.lanes [i].filename.empty ();
    }
    state.lanes [which].filename = p;
    state.lanes [which].loaded = p [0] != '\0';

    apply_state (state);
  }

  void set_model_property (LV2_URID property, const char *path) {
    for (size_t i = 0; i < NB_UI_MAX_LANES; ++i) {
      if (property == urid_model [i]) {
        set_model_path (i, path);
        return;
      }
    }
  }

  void redraw_meters_now () {
    if (meter_in.needs_redraw () && meter_in.widget)
      transparent_draw (meter_in.widget, NULL);

    for (size_t lane = 0; lane < NB_UI_MAX_LANES; lane++) {
      if (lanes [lane].meter_out.needs_redraw () &&
          lanes [lane].meter_out.widget)
        transparent_draw (lanes [lane].meter_out.widget, NULL);
    }
  }

  void set_meter_values (const LV2_Atom *value) {
    if (!value || value->type != urid_atom_Vector)
      return;

    const LV2_Atom_Vector *vec = (const LV2_Atom_Vector *) value;
    if (vec->body.child_type != urid_atom_Float ||
        vec->body.child_size != sizeof (float) ||
        value->size < sizeof (LV2_Atom_Vector_Body))
      return;

    const uint32_t count =
      (value->size - sizeof (LV2_Atom_Vector_Body)) / sizeof (float);
    const uint32_t need = (1 + NB_UI_MAX_LANES) * 2;
    if (count < need)
      return;

    const float *values =
      (const float *) ((const uint8_t *) LV2_ATOM_BODY_CONST (value) +
                       sizeof (LV2_Atom_Vector_Body));

    size_t n = 0;
    vudata_in.set_l (values [n], values [n + 1]);
    n += 2;

    for (size_t lane = 0; lane < NB_UI_MAX_LANES; lane++) {
      lanes [lane].vudata_out.set_l (values [n], values [n + 1]);
      n += 2;
    }

    redraw_meters_now ();
  }

  void handle_atom_event (const LV2_Atom *atom) {
    if (!atom)
      return;

    const LV2_Atom_Object *obj = (const LV2_Atom_Object *) atom;
    if (obj->body.otype != urid_patch_Set)
      return;

    const LV2_Atom *property = NULL;
    const LV2_Atom *value = NULL;
    lv2_atom_object_get (
      obj,
      urid_patch_property, &property,
      urid_patch_value, &value,
      0);

    if (!property || !value || property->type != urid_atom_URID)
      return;

    const LV2_URID prop = ((const LV2_Atom_URID *) property)->body;
    if (prop == urid_meters) {
      set_meter_values (value);
      return;
    }

    if (value->type != urid_atom_Path &&
        value->type != urid_atom_String)
      return;

    const char *path = (const char *) LV2_ATOM_BODY_CONST (value);
    set_model_property (prop, path);
  }

  void subscribe_ports () {
    if (!subscribe)
      return;

    for (uint32_t port = PORT_BYPASS; port <= PORT_D_MUTE; ++port)
      subscribe->subscribe (subscribe->handle, port, 0, NULL);
    subscribe->subscribe (subscribe->handle, PORT_VU_ENABLE, 0, NULL);
    subscribe->subscribe (subscribe->handle, PORT_MUTE_ALL, 0, NULL);
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
    } else if (!strcmp (features [i]->URI, LV2_UI__portSubscribe)) {
      ui->subscribe = (LV2UI_Port_Subscribe *) features [i]->data;
    }
  }

  if (ui->map) {
    lv2_atom_forge_init (&ui->forge, ui->map);
    ui->urid_atom_eventTransfer = ui->map->map (ui->map->handle, LV2_ATOM__eventTransfer);
    ui->urid_atom_Path = ui->map->map (ui->map->handle, LV2_ATOM__Path);
    ui->urid_atom_String = ui->map->map (ui->map->handle, LV2_ATOM__String);
    ui->urid_atom_URID = ui->map->map (ui->map->handle, LV2_ATOM__URID);
    ui->urid_atom_Float = ui->map->map (ui->map->handle, LV2_ATOM__Float);
    ui->urid_atom_Vector = ui->map->map (ui->map->handle, LV2_ATOM__Vector);
    ui->urid_patch_Set = ui->map->map (ui->map->handle, LV2_PATCH__Set);
    ui->urid_patch_Get = ui->map->map (ui->map->handle, LV2_PATCH__Get);
    ui->urid_patch_property = ui->map->map (ui->map->handle, LV2_PATCH__property);
    ui->urid_patch_value = ui->map->map (ui->map->handle, LV2_PATCH__value);
    ui->urid_model [0] = ui->map->map (ui->map->handle, "http://deimos.ca/neuralblender#ModelA");
    ui->urid_model [1] = ui->map->map (ui->map->handle, "http://deimos.ca/neuralblender#ModelB");
    ui->urid_model [2] = ui->map->map (ui->map->handle, "http://deimos.ca/neuralblender#ModelC");
    ui->urid_model [3] = ui->map->map (ui->map->handle, "http://deimos.ca/neuralblender#ModelD");
    ui->urid_meters = ui->map->map (ui->map->handle, "http://deimos.ca/neuralblender#Meters");
  }

  if (!ui->create (parent)) {
    delete ui;
    return NULL;
  }

  ui->subscribe_ports ();
  ui->request_current_state ();

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
  const void *buffer) {
  
  c_lv2_ui *ui = (c_lv2_ui *) handle;
  if (!ui || !buffer)
    return;

  if (format == 0 && buffer_size == sizeof (float)) {
    const float value = *(const float *) buffer;
    ui->set_port_value (port_index, value);
    return;
  }

  if (format == ui->urid_atom_eventTransfer && buffer_size >= sizeof (LV2_Atom_Object)) {
    ui->handle_atom_event ((const LV2_Atom *) buffer);
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
