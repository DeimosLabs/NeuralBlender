
/* NeuralBlender - RTNeural / NAM based amp modeler
 *
 * lv2 user interface. Mostly written by codex because i don't
 * like the lv2 api.
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

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

#define CMDLINE_IMPLEMENTATION
#define CMDLINE_DEBUG_COLOR ANSI_DARK_MAGENTA
#include "cmdline_debug.h"

enum _ui_feedback_type {
  ATOM_METERS,
  ATOM_STATS,
  ATOM_UNKNOWN
};

std::string path_dirname (const std::string &path);

enum {
  PORT_AUDIO_IN = 0,
  PORT_AUDIO_OUT,

  PORT_BYPASS,

  PORT_A_GAIN_IN,
  PORT_A_GAIN_OUT,
  PORT_A_DELAY,
  PORT_A_MUTE,
  PORT_A_DCFLIP,
  PORT_A_CALIBRATE,

  PORT_B_GAIN_IN,
  PORT_B_GAIN_OUT,
  PORT_B_DELAY,
  PORT_B_MUTE,
  PORT_B_DCFLIP,
  PORT_B_CALIBRATE,

  PORT_C_GAIN_IN,
  PORT_C_GAIN_OUT,
  PORT_C_DELAY,
  PORT_C_MUTE,
  PORT_C_DCFLIP,
  PORT_C_CALIBRATE,

  PORT_D_GAIN_IN,
  PORT_D_GAIN_OUT,
  PORT_D_DELAY,
  PORT_D_MUTE,
  PORT_D_DCFLIP,
  PORT_D_CALIBRATE,

  PORT_CONTROL,
  PORT_NOTIFY,
  PORT_VU_ENABLE,
  PORT_MUTE_ALL,
  PORT_EXCLUSIVE_LANE
};

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
  LV2_URID urid_stats = 0;
  LV2UI_Port_Subscribe *subscribe = NULL;
  LV2UI_Resize *resize = NULL;
  bool updating_from_host = false;
  
  void write_control (uint32_t port, float value) {
    if (updating_from_host || !write)
      return;
    write (controller, port, sizeof (value), 0, &value);
  }

  uint32_t lane_port (size_t lane, uint32_t first) const {
    return first + (uint32_t) lane * 6;
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
  void on_dcflip (c_widget *w, bool b)                 { CP; write_control (lane_port (w->lane, PORT_A_DCFLIP), b ? 1.0f : 0.0f); }
  void on_calibrate (c_widget *w, bool b)              { CP; write_control (lane_port (w->lane, PORT_A_CALIBRATE), b ? 1.0f : 0.0f); }
  void on_muteall (c_widget *w, bool b)                { CP; write_control (PORT_MUTE_ALL, b ? 1.0f : 0.0f); }
  void on_excl (c_widget *w, int n) {
    CP
    state.exclusive_lane = n;
    if (n > 0 && n <= (int) NB_UI_MAX_LANES)
      last_exclusive_lane = (size_t) n;
    write_control (PORT_EXCLUSIVE_LANE, (float) n);
    sync_widgets_from_state (state);
  }
  void on_bypass (c_widget *w, bool b)                 { CP; write_control (PORT_BYPASS, b ? 1.0f : 0.0f); }
  void on_about (c_widget *w)                          { CP }
  void on_vu (c_widget *w, bool b)                     { CP; write_control (PORT_VU_ENABLE, b ? 1.0f : 0.0f); }

  bool request_window_size (int w, int h) {
    if (resize && resize->ui_resize) {
      const bool ok = resize->ui_resize (resize->handle, w, h) == 0;
      if (ok && main_widget && display)
        os_resize_window (display, main_widget, w, h);
      return ok;
    }

    return c_neuralblender_ui::request_window_size (w, h);
  }

  void set_port_value (uint32_t port, float value) {
    updating_from_host = true;
    const bool old_updating_from_state = updating_from_state;
    updating_from_state = true;

    if (port == PORT_BYPASS) {
      state.bypass = value < 0.5f;
      const bool enabled = !state.bypass;
      btn_enable.set_value (enabled);
      btn_enable.set_label (enabled ? "Enabled" : "Bypass");
      updating_from_state = old_updating_from_state;
      updating_from_host = false;
      return;
    }

    if (port == PORT_VU_ENABLE) {
      state.do_vu = value >= 0.5f;
      btn_vu.set_value (state.do_vu);
      if (state.do_vu) {
        meter_in.show ();
        for (size_t i = 0; i < NB_UI_MAX_LANES; ++i)
          lanes [i].meter_out.show ();
      } else {
        meter_in.hide ();
        for (size_t i = 0; i < NB_UI_MAX_LANES; ++i)
          lanes [i].meter_out.hide ();
      }
      updating_from_state = old_updating_from_state;
      updating_from_host = false;
      return;
    }

    if (port == PORT_MUTE_ALL) {
      state.mute_all = value >= 0.5f;
      btn_muteall.set_value (state.mute_all);
      updating_from_state = old_updating_from_state;
      updating_from_host = false;
      return;
    }

    if (port == PORT_EXCLUSIVE_LANE) {
      int n = (int) lrintf (value);
      if (n < 0)
        n = 0;
      if (n > (int) NB_UI_MAX_LANES)
        n = (int) NB_UI_MAX_LANES;
      state.exclusive_lane = n;
      updating_from_state = old_updating_from_state;
      sync_widgets_from_state (state);
      updating_from_host = false;
      return;
    }

    for (size_t lane = 0; lane < NB_UI_MAX_LANES; lane++) {
      const uint32_t base = PORT_A_GAIN_IN + (uint32_t) lane * 6;
      if (port == base) {
        state.lanes [lane].gain_in = db_to_gain (value);
        lanes [lane].gain_in.set_value (value);
        updating_from_state = old_updating_from_state;
        updating_from_host = false;
        return;
      } else if (port == base + 1) {
        state.lanes [lane].gain_out = db_to_gain (value);
        lanes [lane].gain_out.set_value (value);
        updating_from_state = old_updating_from_state;
        updating_from_host = false;
        return;
      } else if (port == base + 2) {
        state.lanes [lane].delay_ms = value;
        lanes [lane].delay.set_value (value);
        updating_from_state = old_updating_from_state;
        updating_from_host = false;
        return;
      } else if (port == base + 3) {
        state.lanes [lane].lane_mute = value >= 0.5f;
        lanes [lane].btn_mute.set_value (state.lanes [lane].lane_mute);
        updating_from_state = old_updating_from_state;
        updating_from_host = false;
        return;
      } else if (port == base + 4) {
        state.lanes [lane].dcflip = value >= 0.5f;
        lanes [lane].btn_flip.set_value (state.lanes [lane].dcflip);
        updating_from_state = old_updating_from_state;
        updating_from_host = false;
        return;
      } else if (port == base + 5) {
        state.lanes [lane].do_calib = value >= 0.5f;
        lanes [lane].btn_calib.set_value (state.lanes [lane].do_calib);
        updating_from_state = old_updating_from_state;
        updating_from_host = false;
        return;
      }
    }

    updating_from_state = old_updating_from_state;
    updating_from_host = false;
  }

  void set_model_path (size_t which, const char *path) {
    if (which >= NB_UI_MAX_LANES)
      return;

    const char *p = path ? path : "";
    state.lanes [which].filename = p;
    state.lanes [which].loaded = p [0] != '\0';

    if (state.exclusive_lane > 0) {
      sync_widgets_from_state (state);
      return;
    }

    const bool old_updating_from_state = updating_from_state;
    updating_from_state = true;
    if (!p [0]) {
      lanes [which].menu_list.clear ();
    } else {
      filepickers [which].current_dir = path_dirname (state.lanes [which].filename);
      filepickers [which].scan_current_dir ();
      filepickers [which].add_files_from_dir (&lanes [which].menu_list);
    }
    updating_from_state = old_updating_from_state;
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

  void set_ui_values (const LV2_Atom *value, _ui_feedback_type type) {
    if (!value || value->type != urid_atom_Vector)
      return;

    const LV2_Atom_Vector *vec = (const LV2_Atom_Vector *) value;
    if (vec->body.child_type != urid_atom_Float ||
        vec->body.child_size != sizeof (float) ||
        value->size < sizeof (LV2_Atom_Vector_Body))
      return;

    const uint32_t count =
      (value->size - sizeof (LV2_Atom_Vector_Body)) / sizeof (float);

    const float *values =
      (const float *) ((const uint8_t *) LV2_ATOM_BODY_CONST (value) +
                       sizeof (LV2_Atom_Vector_Body));

    switch (type) {
      case ATOM_METERS: {
        const uint32_t need = (1 + NB_UI_MAX_LANES) * 2;
        if (count < need)
          return;

        size_t n = 0;
        vudata_in.set_l (values [n], values [n + 1]);
        n += 2;
        
        for (size_t lane = 0; lane < NB_UI_MAX_LANES; lane++) {
          lanes [lane].vudata_out.set_l (values [n], values [n + 1]);
          n += 2;
        }

        redraw_meters_now ();
        break;
      }

      case ATOM_STATS: { CP
        const uint32_t need = NB_UI_MAX_LANES * 2;
        if (count < need)
          return;

        for (size_t lane = 0; lane < NB_UI_MAX_LANES; lane++) {
          const size_t n = lane * 2;
          stats [n] = values [n];
          stats [n + 1] = values [n + 1];
        }
        
        update_stats ();
        break;
      }

      default: CP
        break;
    }
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
      set_ui_values (value, ATOM_METERS);
      return;
    }
    if (prop == urid_stats) {
      set_ui_values (value, ATOM_STATS);
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

    for (uint32_t port = PORT_BYPASS; port <= PORT_D_CALIBRATE; ++port)
      subscribe->subscribe (subscribe->handle, port, 0, NULL);
    subscribe->subscribe (subscribe->handle, PORT_VU_ENABLE, 0, NULL);
    subscribe->subscribe (subscribe->handle, PORT_MUTE_ALL, 0, NULL);
    subscribe->subscribe (subscribe->handle, PORT_EXCLUSIVE_LANE, 0, NULL);
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
    } else if (!strcmp (features [i]->URI, LV2_UI__resize)) {
      ui->resize = (LV2UI_Resize *) features [i]->data;
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
    ui->urid_stats = ui->map->map (ui->map->handle, "http://deimos.ca/neuralblender#Stats");
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
