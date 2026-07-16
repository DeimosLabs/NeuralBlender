
/* NeuralBlender - RTNeural / NAM based amp modeler
 *
 * lv2 user interface. Mostly written by codex because i don't
 * like the lv2 api.
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
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
#include "lv2.h"

#define NB_UI_URI "http://deimos.ca/neuralblender#ui"

#define CMDLINE_IMPLEMENTATION
#define CMDLINE_DEBUG_COLOR ANSI_DARK_MAGENTA
#include "cmdline_debug.h"

std::string path_dirname (const std::string &path);


void c_lv2_ui::write_control (uint32_t port, float value) {
  if (updating_from_host || !write)
    return;
  write (controller, port, sizeof (value), 0, &value);
}

uint32_t c_lv2_ui::lane_port (size_t lane, uint32_t first) const {
  return nb_lv2_lane_port (lane, first);
}

bool c_lv2_ui::write_model_path (size_t which, const char *filename) {
  if (updating_from_host || !write || !map || which >= NB_NUM_MODELS)
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

bool c_lv2_ui::write_float_property (LV2_URID property, float value) {
  if (updating_from_host || !write || !map || !property)
    return false;

  uint8_t buf [256];
  LV2_Atom_Forge_Frame frame;

  lv2_atom_forge_set_buffer (&forge, buf, sizeof (buf));
  lv2_atom_forge_object (&forge, &frame, 0, urid_patch_Set);
  lv2_atom_forge_key (&forge, urid_patch_property);
  lv2_atom_forge_urid (&forge, property);
  lv2_atom_forge_key (&forge, urid_patch_value);
  lv2_atom_forge_float (&forge, value);
  lv2_atom_forge_pop (&forge, &frame);

  const LV2_Atom *atom = (const LV2_Atom *) buf;
  write (controller,
         PORT_CONTROL,
         lv2_atom_total_size (atom),
         urid_atom_eventTransfer,
         atom);
  return true;
}

bool c_lv2_ui::write_int_property (LV2_URID property, int32_t value) {
  if (updating_from_host || !write || !map || !property)
    return false;

  uint8_t buf [256];
  LV2_Atom_Forge_Frame frame;

  lv2_atom_forge_set_buffer (&forge, buf, sizeof (buf));
  lv2_atom_forge_object (&forge, &frame, 0, urid_patch_Set);
  lv2_atom_forge_key (&forge, urid_patch_property);
  lv2_atom_forge_urid (&forge, property);
  lv2_atom_forge_key (&forge, urid_patch_value);
  lv2_atom_forge_int (&forge, value);
  lv2_atom_forge_pop (&forge, &frame);

  const LV2_Atom *atom = (const LV2_Atom *) buf;
  write (controller,
         PORT_CONTROL,
         lv2_atom_total_size (atom),
         urid_atom_eventTransfer,
         atom);
  return true;
}

void c_lv2_ui::request_current_state () {
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

bool c_lv2_ui::load_model (size_t which, const char *filename) {
  CP
  return write_model_path (which, filename);
}

void c_lv2_ui::on_gain_in (c_widget *w, float f) {
  CP
  write_control (lane_port (w->lane, PORT_A_GAIN_IN), gain_to_db (f));
}

void c_lv2_ui::on_gain_out (c_widget *w, float f) {
  CP
  write_control (lane_port (w->lane, PORT_A_GAIN_OUT), gain_to_db (f));
}

void c_lv2_ui::on_dry_out (c_widget *w, float f) {
  CP
  write_control (lane_port (w->lane, PORT_A_DRY_OUT), gain_to_db (f));
}

void c_lv2_ui::on_delay (c_widget *w, float f) {
  CP
  write_control (lane_port (w->lane, PORT_A_DELAY), f);
}

void c_lv2_ui::on_filebrowse (c_widget *w) {
  (void) w;
  CP
}

void c_lv2_ui::on_fileselected (c_widget *w, const char *path) {
  (void) w;
  (void) path;
  CP
}

void c_lv2_ui::on_fileclear (c_widget *w) {
  CP
  clear_lane_model_ui (w->lane);
  write_model_path (w->lane, "");
}

void c_lv2_ui::on_mute (c_widget *w, bool b) {
  CP
  write_control (lane_port (w->lane, PORT_A_MUTE), b ? 1.0f : 0.0f);
}

void c_lv2_ui::on_dcflip (c_widget *w, bool b) {
  CP
  write_control (lane_port (w->lane, PORT_A_DCFLIP), b ? 1.0f : 0.0f);
}

void c_lv2_ui::on_calibrate (c_widget *w, bool b) {
  CP
  write_control (lane_port (w->lane, PORT_A_CALIBRATE), b ? 1.0f : 0.0f);
}

void c_lv2_ui::on_muteall (c_widget *w, bool b) {
  (void) w;
  CP
  write_control (PORT_MUTE_ALL, b ? 1.0f : 0.0f);
}

void c_lv2_ui::on_excl (c_widget *w, int n) {
  (void) w;
  CP
  state.exclusive_lane = n;
  if (n > 0 && n <= (int) NB_NUM_MODELS)
    last_exclusive_lane = (size_t) n;
  write_control (PORT_EXCLUSIVE_LANE, (float) n);
  sync_widgets_from_state (state);
}

void c_lv2_ui::on_bypass (c_widget *w, bool b) {
  (void) w;
  CP
  write_control (PORT_BYPASS, b ? 1.0f : 0.0f);
}

void c_lv2_ui::on_about (c_widget *w) {
  (void) w;
  CP
}

void c_lv2_ui::on_vu (c_widget *w, bool b) {
  (void) w;
  CP
  write_control (PORT_VU_ENABLE, b ? 1.0f : 0.0f);
}

void c_lv2_ui::on_linked_calib (c_widget *w, bool b) {
  (void) w;
  CP
  write_control (PORT_LINKED_CALIB, b ? 1.0f : 0.0f);
}

void c_lv2_ui::on_calib_bass (c_widget *w, bool b) {
  (void) w;
  CP
  write_control (PORT_CALIB_SOURCE, b ? 1.0f : 0.0f);
}

void c_lv2_ui::on_noisegate (c_widget *w, bool b) {
  (void) w;
  CP
  state.noisegate_on = b;
  prefs.noisegate_on = b;
  write_control (PORT_NOISEGATE_ENABLED, b ? 1.0f : 0.0f);
}

void c_lv2_ui::on_noisethresh (c_widget *w, float f) {
  (void) w;
  CP
  state.noisethresh = f;
  prefs.noisethresh = f;
  write_prefs_to_config (configfile, prefs);
  write_control (PORT_NOISEGATE_THRESHOLD, f);
}

void c_lv2_ui::on_noiseattack (c_widget *w, float f) {
  (void) w;
  CP
  write_control (PORT_NOISEGATE_ATTACK, f);
}

void c_lv2_ui::on_noisehold (c_widget *w, float f) {
  (void) w;
  CP
  write_control (PORT_NOISEGATE_HOLD, f);
}

void c_lv2_ui::on_noiserelease (c_widget *w, float f) {
  (void) w;
  CP
  write_control (PORT_NOISEGATE_RELEASE, f);
}

void c_lv2_ui::on_threshgain (c_widget *w, float f) {
  (void) w;
  meter_in.set_compression_gain (f);
}

void c_lv2_ui::on_tuner (c_widget *w, bool b) {
  (void) w;
  CP
  state.tuner_on = b;
  write_control (PORT_TUNER_ON, b ? 1.0f : 0.0f);
}

void c_lv2_ui::apply_prefs (t_prefs &p) {
  c_neuralblender_ui::apply_prefs (p);
  write_control (PORT_LINKED_CALIB, p.linked_calib ? 1.0f : 0.0f);
  write_control (PORT_CALIB_SOURCE, (float) p.calib_source);
  write_control (PORT_CALIB_TARGET_DB, p.calib_target_db);
}

bool c_lv2_ui::request_window_size (int w, int h) {
  if (resize && resize->ui_resize) {
    const bool ok = resize->ui_resize (resize->handle, w, h) == 0;
    if (ok && mainwindow.widget && display)
      os_resize_window (display, mainwindow.widget, w, h);
    return ok;
  }

  return c_neuralblender_ui::request_window_size (w, h);
}

void c_lv2_ui::set_port_value (uint32_t port, float value) {
  updating_from_host = true;
  const bool old_updating_from_state = updating_from_state;
  updating_from_state = true;

  if (port == PORT_BYPASS) {
    state.bypass = value < 0.5f;
    updating_from_state = old_updating_from_state;
    sync_widgets_from_state (state);
    updating_from_host = false;
    return;
  }

  if (port == PORT_VU_ENABLE) {
    state.do_vu = value >= 0.5f;
    prefs.vu_on = state.do_vu;
    if (prefswindow.widget)
      prefswindow.btn_vu.set_value (state.do_vu);
    if (state.do_vu) {
      meter_in.show ();
      for (size_t i = 0; i < NB_NUM_MODELS; ++i)
        lanes [i].meter_out.show ();
    } else {
      meter_in.hide ();
      for (size_t i = 0; i < NB_NUM_MODELS; ++i)
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
    if (n > (int) NB_NUM_MODELS)
      n = (int) NB_NUM_MODELS;
    state.exclusive_lane = n;
    updating_from_state = old_updating_from_state;
    sync_widgets_from_state (state);
    updating_from_host = false;
    return;
  }

  if (port == PORT_LINKED_CALIB) {
    prefs.linked_calib = value >= 0.5f;
    btn_linkcalib.set_value (prefs.linked_calib);
    if (prefswindow.widget)
      prefswindow.btn_linkcalib.set_value (prefs.linked_calib);
    updating_from_state = old_updating_from_state;
    updating_from_host = false;
    return;
  }

  if (port == PORT_CALIB_SOURCE) {
    int source = (int) lrintf (value);
    if (source < 0)
      source = 0;
    prefs.calib_source = source;
    btn_bass.set_value (prefs.calib_source == 1);
    if (prefswindow.widget)
      prefswindow.btn_bass.set_value (prefs.calib_source == 1);
    updating_from_state = old_updating_from_state;
    updating_from_host = false;
    return;
  }

  if (port == PORT_CALIB_TARGET_DB) {
    prefs.calib_target_db = value;
    if (prefswindow.widget) {
      char buf [64];
      snprintf (buf, sizeof (buf), "%.6g", prefs.calib_target_db);
      prefswindow.text_calibdb.set_text (buf);
    }
    updating_from_state = old_updating_from_state;
    updating_from_host = false;
    return;
  }

  if (port == PORT_NOISEGATE_ENABLED) {
    state.noisegate_on = value >= 0.5f;
    prefs.noisegate_on = state.noisegate_on;
    btn_noisegate.set_value (state.noisegate_on);
    if (state.noisegate_on)
      knob_noisethresh.show ();
    else
      knob_noisethresh.hide ();
    updating_from_state = old_updating_from_state;
    updating_from_host = false;
    return;
  }

  if (port == PORT_NOISEGATE_THRESHOLD) {
    state.noisethresh = value;
    prefs.noisethresh = value;
    knob_noisethresh.set_value (value);
    updating_from_state = old_updating_from_state;
    updating_from_host = false;
    return;
  }

  if (port == PORT_NOISEGATE_GAIN) {
    on_threshgain (nullptr, value);
    updating_from_state = old_updating_from_state;
    updating_from_host = false;
    return;
  }

  if (port == PORT_TUNER_ON) {
    state.tuner_on = value >= 0.5f;
    btn_tuner.set_value (state.tuner_on);
    if (state.tuner_on) {
      tuner.show ();
      img_logo.hide ();
    } else {
      tuner.hide ();
      img_logo.show ();
    }
    updating_from_state = old_updating_from_state;
    updating_from_host = false;
    return;
  }

  if (port == PORT_TUNER_NOTE) {
    tuner_note_value = value;
    tuner.set_pitch (tuner_freq_value, tuner_note_value, tuner_cents_value);
    updating_from_state = old_updating_from_state;
    updating_from_host = false;
    return;
  }

  if (port == PORT_TUNER_CENTS_OFF) {
    tuner_cents_value = value;
    tuner.set_pitch (tuner_freq_value, tuner_note_value, tuner_cents_value);
    updating_from_state = old_updating_from_state;
    updating_from_host = false;
    return;
  }

  if (port == PORT_TUNER_FREQ) {
    tuner_freq_value = value;
    tuner.set_pitch (tuner_freq_value, tuner_note_value, tuner_cents_value);
    updating_from_state = old_updating_from_state;
    updating_from_host = false;
    return;
  }

  size_t lane = 0;
  uint32_t param = 0;
  if (nb_lv2_decode_lane_port (port, &lane, &param)) {
    switch (param) {
      case NB_LV2_LANE_GAIN_IN:
        state.lanes [lane].gain_in = db_to_gain (value);
        lanes [lane].knob_gain_in.set_value (value);
      break;

      case NB_LV2_LANE_GAIN_OUT:
        state.lanes [lane].gain_out = db_to_gain (value);
        lanes [lane].knob_gain_out.set_value (value);
      break;

      case NB_LV2_LANE_DRY_OUT:
        state.lanes [lane].dry_out =
          value <= DB_SILENCE ? 0.0f : db_to_gain (value);
        lanes [lane].knob_dry_out.set_value (value);
      break;

      case NB_LV2_LANE_DELAY:
        state.lanes [lane].delay_ms = value;
        lanes [lane].knob_delay.set_value (value);
      break;

      case NB_LV2_LANE_MUTE:
        state.lanes [lane].lane_mute = value >= 0.5f;
        lanes [lane].btn_mute.set_value (state.lanes [lane].lane_mute);
      break;

      case NB_LV2_LANE_DCFLIP:
        state.lanes [lane].dcflip = value >= 0.5f;
        lanes [lane].btn_flip.set_value (state.lanes [lane].dcflip);
      break;

      case NB_LV2_LANE_CALIBRATE:
        state.lanes [lane].do_calib = value >= 0.5f;
        lanes [lane].btn_calib.set_value (state.lanes [lane].do_calib);
      break;
    }

    updating_from_state = old_updating_from_state;
    updating_from_host = false;
    return;
  }

  updating_from_state = old_updating_from_state;
  updating_from_host = false;
}

void c_lv2_ui::set_model_path (size_t which, const char *path) {
  if (which >= NB_NUM_MODELS)
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

void c_lv2_ui::set_model_property (LV2_URID property, const char *path) {
  for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
    if (property == urid_model [i]) {
      set_model_path (i, path);
      return;
    }
  }
}

void c_lv2_ui::redraw_meters_now () {
  if (meter_in.needs_redraw () && meter_in.widget)
    transparent_draw (meter_in.widget, NULL);

  for (size_t lane = 0; lane < NB_NUM_MODELS; lane++) {
    if (lanes [lane].meter_out.needs_redraw () &&
        lanes [lane].meter_out.widget)
      transparent_draw (lanes [lane].meter_out.widget, NULL);
  }
}

void c_lv2_ui::set_ui_values (const LV2_Atom *value, _ui_feedback_type type) {
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
      const uint32_t need = (1 + NB_NUM_MODELS) * 2;
      if (count < need)
        return;

      size_t n = 0;
      vudata_in.set_l_smooth (values [n], values [n + 1]);
      n += 2;

      for (size_t lane = 0; lane < NB_NUM_MODELS; lane++) {
        lanes [lane].vudata_out.set_l_smooth (values [n], values [n + 1]);
        n += 2;
      }

      redraw_meters_now ();
      break;
    }

    case ATOM_STATS: { CP
      const uint32_t need = NB_NUM_MODELS * 2;
      if (count < need)
        return;

      for (size_t lane = 0; lane < NB_NUM_MODELS; lane++) {
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

void c_lv2_ui::handle_atom_event (const LV2_Atom *atom) {
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

void c_lv2_ui::subscribe_ports () {
  if (!subscribe)
    return;

  static const uint32_t scalar_ports [] = {
    PORT_BYPASS,
    PORT_VU_ENABLE,
    PORT_MUTE_ALL,
    PORT_EXCLUSIVE_LANE,
    PORT_LINKED_CALIB,
    PORT_CALIB_SOURCE,
    PORT_CALIB_TARGET_DB,
    PORT_NOISEGATE_ENABLED,
    PORT_NOISEGATE_THRESHOLD,
    PORT_NOISEGATE_ATTACK,
    PORT_NOISEGATE_HOLD,
    PORT_NOISEGATE_RELEASE,
    PORT_TUNER_ON,
    PORT_TUNER_BASE_FREQ,
    PORT_NOISEGATE_GAIN,
    PORT_TUNER_NOTE,
    PORT_TUNER_CENTS_OFF,
    PORT_TUNER_FREQ,
  };

  for (size_t lane = 0; lane < NB_NUM_MODELS; lane++) {
    for (uint32_t param = 0; param < NB_LV2_LANE_PORT_COUNT; param++) {
      subscribe->subscribe (
        subscribe->handle,
        nb_lv2_lane_port (lane, PORT_A_GAIN_IN + param),
        0,
        NULL);
    }
  }

  for (uint32_t port : scalar_ports)
    subscribe->subscribe (subscribe->handle, port, 0, NULL);
}

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

  if (ui->map && !ui->init (ui->map)) {
    delete ui;
    return NULL;
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
