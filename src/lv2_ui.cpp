
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

static uint32_t widget_lane_port (const c_widget *w, uint32_t param) {
  const _lane_bank bank =
    w && w->bank < BANK_COUNT ? (_lane_bank) w->bank : BANK_AMP;
  const size_t lane =
    w && w->lane < NB_NUM_MODELS ? w->lane : 0;
  return nb_lv2_bank_lane_port (bank, lane, param);
}

bool c_lv2_ui::write_model_path (
    _lane_bank bank, size_t which, const char *filename) {
  if (updating_from_host || !write || !map ||
      bank < BANK_PEDAL || bank >= BANK_COUNT ||
      which >= NB_NUM_MODELS)
    return false;

  const char *path = filename ? filename : "";
  const LV2_URID property = urid_bank_model [bank] [which];
  if (!property)
    return false;

  uint8_t buf [4096];
  LV2_Atom_Forge_Frame frame;

  lv2_atom_forge_set_buffer (&forge, buf, sizeof (buf));
  lv2_atom_forge_object (&forge, &frame, 0, urid_patch_Set);
  lv2_atom_forge_key (&forge, urid_patch_property);
  lv2_atom_forge_urid (&forge, property);
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

bool c_lv2_ui::load_model (
    _lane_bank bank, size_t which, const char *filename) {
  CP
  return write_model_path (bank, which, filename);
}

void c_lv2_ui::on_gain_in (c_widget *w, float f) {
  CP
  write_control (widget_lane_port (w, NB_LV2_LANE_GAIN_IN), gain_to_db (f));
}

void c_lv2_ui::on_ir_pitch (c_widget *w, float f) {
  CP
  write_control (widget_lane_port (w, NB_LV2_LANE_IR_PITCH), f);
}

void c_lv2_ui::on_gain_out (c_widget *w, float f) {
  CP
  write_control (widget_lane_port (w, NB_LV2_LANE_GAIN_OUT), gain_to_db (f));
}

void c_lv2_ui::on_dry_out (c_widget *w, float f) {
  CP
  write_control (widget_lane_port (w, NB_LV2_LANE_DRY_OUT), gain_to_db (f));
}

void c_lv2_ui::on_delay (c_widget *w, float f) {
  CP
  write_control (widget_lane_port (w, NB_LV2_LANE_DELAY), f);
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
  const _lane_bank bank =
    w->bank < BANK_COUNT ? (_lane_bank) w->bank : BANK_AMP;
  clear_lane_model_ui (bank, w->lane);
  write_model_path (bank, w->lane, "");
}

void c_lv2_ui::on_mute (c_widget *w, bool b) {
  CP
  write_control (widget_lane_port (w, NB_LV2_LANE_MUTE), b ? 1.0f : 0.0f);
}

void c_lv2_ui::on_dcflip (c_widget *w, bool b) {
  CP
  write_control (widget_lane_port (w, NB_LV2_LANE_DCFLIP), b ? 1.0f : 0.0f);
}

void c_lv2_ui::on_calibrate (c_widget *w, bool b) {
  CP
  write_control (
    widget_lane_port (w, NB_LV2_LANE_CALIBRATE), b ? 1.0f : 0.0f);
}

void c_lv2_ui::on_muteall (c_widget *w, bool b) {
  (void) w;
  CP
  write_control (PORT_MUTE_ALL, b ? 1.0f : 0.0f);
}

void c_lv2_ui::on_excl (c_widget *w, int n) {
  (void) w;
  CP
  set_exclusive_lane_for_bank (visible_bank, n);
  if (n > 0 && n <= (int) NB_NUM_MODELS)
    last_exclusive_lane [visible_bank] = (size_t) n;
  switch (visible_bank) {
    case BANK_PEDAL:
      write_control (PORT_EXCLUSIVE_LANE_PEDAL, (float) n);
    break;

    case BANK_CAB:
      write_control (PORT_EXCLUSIVE_LANE_CAB, (float) n);
    break;

    case BANK_AMP:
    default:
      write_control (PORT_EXCLUSIVE_LANE_AMP, (float) n);
    break;
  }
  sync_widgets_from_state (state);
}

void c_lv2_ui::on_bypass (c_widget *w, bool b) {
  (void) w;
  CP
  write_control (PORT_BYPASS, b ? 1.0f : 0.0f);
}

void c_lv2_ui::on_bank_bypass (c_widget *w, _lane_bank bank, bool b) {
  (void) w;
  CP
  switch (bank) {
    case BANK_PEDAL:
      write_control (PORT_PEDAL_BYPASS, b ? 1.0f : 0.0f);
    break;

    case BANK_CAB:
      write_control (PORT_CAB_BYPASS, b ? 1.0f : 0.0f);
    break;

    case BANK_AMP:
    default:
      write_control (PORT_AMP_BYPASS, b ? 1.0f : 0.0f);
    break;
  }
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
  set_linked_calib_for_bank (visible_bank, b);
  switch (visible_bank) {
    case BANK_PEDAL:
      write_control (PORT_LINKED_CALIB_PEDAL, b ? 1.0f : 0.0f);
    break;

    case BANK_CAB:
      write_control (PORT_LINKED_CALIB_CAB, b ? 1.0f : 0.0f);
    break;

    case BANK_AMP:
    default:
      prefs.linked_calib = b;
      write_control (PORT_LINKED_CALIB_AMP, b ? 1.0f : 0.0f);
    break;
  }
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
  write_control (PORT_NOISEGATE_THRESHOLD, f);
}

void c_lv2_ui::on_noiseattack (c_widget *w, float f) {
  (void) w;
  CP
  state.noiseattack = f;
  prefs.noiseattack = f;
  write_control (PORT_NOISEGATE_ATTACK, f);
}

void c_lv2_ui::on_noisehold (c_widget *w, float f) {
  (void) w;
  CP
  state.noisehold = f;
  prefs.noisehold = f;
  write_control (PORT_NOISEGATE_HOLD, f);
}

void c_lv2_ui::on_noiserelease (c_widget *w, float f) {
  (void) w;
  CP
  state.noiserelease = f;
  prefs.noiserelease = f;
  write_control (PORT_NOISEGATE_RELEASE, f);
}

void c_lv2_ui::on_threshgain (c_widget *w, float f) {
  (void) w;
  set_threshgain (f);
}

void c_lv2_ui::on_tuner (c_widget *w, bool b) {
  (void) w;
  CP
  state.tuner_on = b;
  write_control (PORT_TUNER_ON, b ? 1.0f : 0.0f);
}

void c_lv2_ui::on_tuner_base_freq (c_widget *w, float f) {
  (void) w;
  state.tuner_base_freq = f;
  prefs.tuner_base_freq = f;
  write_control (PORT_TUNER_BASE_FREQ, f);
}

void c_lv2_ui::on_calib_target_db (c_widget *w, float f) {
  (void) w;
  prefs.calib_target_db = f;
  write_control (PORT_CALIB_TARGET_DB, f);
}

void c_lv2_ui::on_master_gain (c_widget *w, float f) {
  (void) w;
  state.master_gain = db_to_gain (f);
  write_control (PORT_MASTER_GAIN, f);
}

void c_lv2_ui::on_presence (c_widget *w, float f) {
  (void) w;
  state.presence = f;
  write_control (PORT_PRESENCE, f);
}

void c_lv2_ui::on_bank_switch (c_widget *w, int n) {
  c_neuralblender_ui::on_bank_switch (w, n);
  write_control (PORT_ACTIVE_PAGE, (float) visible_page);
}

void c_lv2_ui::apply_prefs (t_prefs &p) {
  c_neuralblender_ui::apply_prefs (p);
  write_control (PORT_LINKED_CALIB_PEDAL, p.linked_calib ? 1.0f : 0.0f);
  write_control (PORT_LINKED_CALIB_AMP, p.linked_calib ? 1.0f : 0.0f);
  write_control (PORT_LINKED_CALIB_CAB, p.linked_calib ? 1.0f : 0.0f);
  write_control (PORT_CALIB_SOURCE, (float) p.calib_source);
  write_control (PORT_CALIB_TARGET_DB, p.calib_target_db);
  write_control (PORT_NOISEGATE_ATTACK, p.noiseattack);
  write_control (PORT_NOISEGATE_HOLD, p.noisehold);
  write_control (PORT_NOISEGATE_RELEASE, p.noiserelease);
  write_control (PORT_TUNER_BASE_FREQ, p.tuner_base_freq);
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
    btn_other_vu.set_value (state.do_vu);
    if (state.do_vu)
      vu_on ();
    else
      vu_off ();
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

  if (port == PORT_EXCLUSIVE_LANE_PEDAL ||
      port == PORT_EXCLUSIVE_LANE_AMP ||
      port == PORT_EXCLUSIVE_LANE_CAB) {
    int n = (int) lrintf (value);
    if (n < 0)
      n = 0;
    if (n > (int) NB_NUM_MODELS)
      n = (int) NB_NUM_MODELS;
    const _lane_bank bank =
      port == PORT_EXCLUSIVE_LANE_PEDAL ? BANK_PEDAL :
      port == PORT_EXCLUSIVE_LANE_CAB   ? BANK_CAB   : BANK_AMP;
    set_exclusive_lane_for_bank (bank, n);
    updating_from_state = old_updating_from_state;
    sync_widgets_from_state (state);
    updating_from_host = false;
    return;
  }

  if (port == PORT_LINKED_CALIB_PEDAL ||
      port == PORT_LINKED_CALIB_AMP ||
      port == PORT_LINKED_CALIB_CAB) {
    const bool linked = value >= 0.5f;
    const _lane_bank bank =
      port == PORT_LINKED_CALIB_PEDAL ? BANK_PEDAL :
      port == PORT_LINKED_CALIB_CAB   ? BANK_CAB   : BANK_AMP;
    if (bank == BANK_AMP)
      prefs.linked_calib = linked;
    set_linked_calib_for_bank (bank, linked);
    btn_other_link_pedal.set_value (linked_calib_for_bank (BANK_PEDAL));
    btn_other_link_amp.set_value (linked_calib_for_bank (BANK_AMP));
    btn_other_link_cab.set_value (linked_calib_for_bank (BANK_CAB));
    updating_from_state = old_updating_from_state;
    updating_from_host = false;
    return;
  }

  if (port == PORT_CALIB_SOURCE) {
    int source = (int) lrintf (value);
    if (source < 0)
      source = 0;
    prefs.calib_source = source;
    btn_other_bass.set_value (prefs.calib_source == 1);
    updating_from_state = old_updating_from_state;
    updating_from_host = false;
    return;
  }

  if (port == PORT_CALIB_TARGET_DB) {
    prefs.calib_target_db = value;
    char buf [64];
    snprintf (buf, sizeof (buf), "%.1f", prefs.calib_target_db);
    text_other_calib.set_text (buf);
    if (prefswindow.widget) {
      snprintf (buf, sizeof (buf), "%.1f", prefs.calib_target_db);
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

  if (port == PORT_NOISEGATE_ATTACK) {
    state.noiseattack = value;
    prefs.noiseattack = value;
    knob_noiseattack.set_value (value);
    updating_from_state = old_updating_from_state;
    updating_from_host = false;
    return;
  }

  if (port == PORT_NOISEGATE_HOLD) {
    state.noisehold = value;
    prefs.noisehold = value;
    knob_noisehold.set_value (value);
    updating_from_state = old_updating_from_state;
    updating_from_host = false;
    return;
  }

  if (port == PORT_NOISEGATE_RELEASE) {
    state.noiserelease = value;
    prefs.noiserelease = value;
    knob_noiserelease.set_value (value);
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
    sync_tuner_visibility ();
    updating_from_state = old_updating_from_state;
    updating_from_host = false;
    return;
  }

  if (port == PORT_TUNER_BASE_FREQ) {
    state.tuner_base_freq = value;
    prefs.tuner_base_freq = value;
    char buf [64];
    snprintf (buf, sizeof (buf), "%.3f", prefs.tuner_base_freq);
    text_other_tuner.set_text (buf);
    updating_from_state = old_updating_from_state;
    updating_from_host = false;
    return;
  }

  if (port == PORT_MASTER_GAIN) {
    state.master_gain = db_to_gain (value);
    knob_mastervolume.set_value (value);
    updating_from_state = old_updating_from_state;
    updating_from_host = false;
    return;
  }

  if (port == PORT_PRESENCE) {
    state.presence = value;
    knob_presence.set_value (value);
    updating_from_state = old_updating_from_state;
    updating_from_host = false;
    return;
  }

  if (port == PORT_ACTIVE_PAGE) {
    int page = (int) lrintf (value);
    if (page < PAGE_PEDAL)
      page = PAGE_PEDAL;
    if (page >= PAGE_COUNT)
      page = PAGE_COUNT - 1;
    c_neuralblender_ui::on_bank_switch (nullptr, page);
    updating_from_state = old_updating_from_state;
    updating_from_host = false;
    return;
  }

  if (port == PORT_PEDAL_BYPASS ||
      port == PORT_AMP_BYPASS ||
      port == PORT_CAB_BYPASS) {
    const bool bypassed = value >= 0.5f;
    if (port == PORT_PEDAL_BYPASS)
      state.pedal_bypass = bypassed;
    else if (port == PORT_CAB_BYPASS)
      state.cab_bypass = bypassed;
    else
      state.amp_bypass = bypassed;
    sync_widgets_from_state (state);
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

  _lane_bank bank = BANK_AMP;
  size_t lane = 0;
  uint32_t param = 0;
  if (nb_lv2_decode_bank_lane_port (port, &bank, &lane, &param)) {
    c_lane_widgets *bank_lanes = lanes_for_bank (bank);
    c_neuralblender_lane_state &lane_state = state.banks [bank].lanes [lane];

    switch (param) {
      case NB_LV2_LANE_GAIN_IN:
        lane_state.gain_in = db_to_gain (value);
        bank_lanes [lane].knob_gain_in.set_value (value);
      break;

      case NB_LV2_LANE_IR_PITCH:
        lane_state.ir_pitch_semitones = value;
        bank_lanes [lane].knob_ir_pitch.set_value (value);
      break;

      case NB_LV2_LANE_GAIN_OUT:
        lane_state.gain_out = db_to_gain (value);
        bank_lanes [lane].knob_gain_out.set_value (value);
      break;

      case NB_LV2_LANE_DRY_OUT:
        lane_state.dry_out =
          value <= DB_SILENCE ? 0.0f : db_to_gain (value);
        bank_lanes [lane].knob_dry_out.set_value (value);
      break;

      case NB_LV2_LANE_DELAY:
        lane_state.delay_ms = value;
        bank_lanes [lane].knob_delay.set_value (value);
      break;

      case NB_LV2_LANE_MUTE:
        lane_state.lane_mute = value >= 0.5f;
        bank_lanes [lane].btn_mute.set_value (lane_state.lane_mute);
      break;

      case NB_LV2_LANE_DCFLIP:
        lane_state.dcflip = value >= 0.5f;
        bank_lanes [lane].btn_flip.set_value (lane_state.dcflip);
      break;

      case NB_LV2_LANE_CALIBRATE:
        lane_state.do_calib = value >= 0.5f;
        bank_lanes [lane].btn_calib.set_value (lane_state.do_calib);
      break;
    }

    updating_from_state = old_updating_from_state;
    updating_from_host = false;
    return;
  }

  updating_from_state = old_updating_from_state;
  updating_from_host = false;
}

void c_lv2_ui::set_model_path (
    _lane_bank bank, size_t which, const char *path) {
  if (bank < BANK_PEDAL || bank >= BANK_COUNT || which >= NB_NUM_MODELS)
    return;

  const char *p = path ? path : "";
  state.banks [bank].lanes [which].filename = p;
  state.banks [bank].lanes [which].loaded = p [0] != '\0';
  if (bank == BANK_AMP) {
    state.lanes [which].filename = p;
    state.lanes [which].loaded = p [0] != '\0';
  }

  if (exclusive_lane_for_bank (bank) > 0) {
    sync_widgets_from_state (state, true);
    return;
  }

  sync_widgets_from_state (state, true);
}

void c_lv2_ui::set_model_property (LV2_URID property, const char *path) {
  for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
    for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
      if (property == urid_bank_model [bank] [i]) {
        set_model_path ((_lane_bank) bank, i, path);
        return;
      }
    }
  }
}

void c_lv2_ui::redraw_meters_now () {
  if (meter_in [visible_bank].needs_redraw () && meter_in [visible_bank].widget)
    transparent_draw (meter_in [visible_bank].widget, NULL);

  c_lane_widgets *bank_lanes = lanes_for_bank (visible_bank);
  for (size_t lane = 0; lane < NB_NUM_MODELS; lane++) {
    if (bank_lanes [lane].meter_out.needs_redraw () &&
        bank_lanes [lane].meter_out.widget)
      transparent_draw (bank_lanes [lane].meter_out.widget, NULL);
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
      const uint32_t old_need = (1 + NB_NUM_MODELS) * 2;
      const uint32_t banked_need = BANK_COUNT * old_need;
      if (count < old_need)
        return;

      if (count >= banked_need) {
        size_t n = 0;
        for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
          c_lane_widgets *bank_lanes =
            lanes_for_bank ((_lane_bank) bank);

          vudata_in [bank].set_l_smooth (values [n], values [n + 1]);
          n += 2;

          for (size_t lane = 0; lane < NB_NUM_MODELS; lane++) {
            bank_lanes [lane].vudata_out.set_l_smooth (
              values [n], values [n + 1]);
            n += 2;
          }
        }
      } else {
        size_t n = 0;
        vudata_in [BANK_AMP].set_l_smooth (values [n], values [n + 1]);
        n += 2;

        for (size_t lane = 0; lane < NB_NUM_MODELS; lane++) {
          lanes_models [lane].vudata_out.set_l_smooth (
            values [n], values [n + 1]);
          n += 2;
        }
      }

      redraw_meters_now ();
      break;
    }

    case ATOM_STATS: { CP
      const uint32_t old_need = NB_NUM_MODELS * UI_STATS_PER_LANE;
      const uint32_t banked_need = BANK_COUNT * old_need;
      if (count < old_need)
        return;

      if (count >= banked_need) {
        size_t src = 0;
        for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
          for (size_t lane = 0; lane < NB_NUM_MODELS; lane++) {
            const size_t dst = lane * UI_STATS_PER_LANE;
            stats [bank] [dst] = values [src++];
            stats [bank] [dst + 1] = values [src++];
            stats [bank] [dst + 2] = values [src++];
          }
        }
      } else {
        for (size_t lane = 0; lane < NB_NUM_MODELS; lane++) {
          const size_t n = lane * UI_STATS_PER_LANE;
          stats [BANK_AMP] [n] = values [n];
          stats [BANK_AMP] [n + 1] = values [n + 1];
          stats [BANK_AMP] [n + 2] = values [n + 2];
        }
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
    PORT_EXCLUSIVE_LANE_PEDAL,
    PORT_EXCLUSIVE_LANE_AMP,
    PORT_EXCLUSIVE_LANE_CAB,
    PORT_LINKED_CALIB_PEDAL,
    PORT_LINKED_CALIB_AMP,
    PORT_LINKED_CALIB_CAB,
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
    PORT_MASTER_GAIN,
    PORT_PRESENCE,
    PORT_ACTIVE_PAGE,
    PORT_PEDAL_BYPASS,
    PORT_AMP_BYPASS,
    PORT_CAB_BYPASS,
  };

  for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
    for (size_t lane = 0; lane < NB_NUM_MODELS; lane++) {
      for (uint32_t param = 0; param < NB_LV2_LANE_PORT_COUNT; param++) {
        subscribe->subscribe (
          subscribe->handle,
          nb_lv2_bank_lane_port ((_lane_bank) bank, lane, param),
          0,
          NULL);
      }
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

static void save_lv2_ui_config (c_lv2_ui *ui) {
  if (!ui || !ui->ui_ready)
    return;

  ui->write_prefs_to (ui->prefs);
  ui->write_calib_state_if_consistent ();
  write_prefs_to_config (ui->configfile, ui->prefs);
}

static void cleanup (LV2UI_Handle handle) { CP
  c_lv2_ui *ui = (c_lv2_ui *) handle;
  if (!ui)
    return;
  
  CP
  save_lv2_ui_config (ui);
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
