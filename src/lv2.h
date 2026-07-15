
/* NeuralBlender - shared LV2 port definitions.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include <lv2/atom/atom.h>
#include <lv2/atom/forge.h>
#include <lv2/patch/patch.h>
#include <lv2/urid/urid.h>

#include "neuralblender.h"

#define NB_URI "http://deimos.ca/neuralblender"
#define LV2_METER_FPS 30.0

enum nb_lv2_port {
  PORT_AUDIO_IN = 0,
  PORT_AUDIO_OUT,

  PORT_BYPASS,

  PORT_A_GAIN_IN,
  PORT_A_GAIN_OUT,
  PORT_A_DRY_OUT,
  PORT_A_DELAY,
  PORT_A_MUTE,
  PORT_A_DCFLIP,
  PORT_A_CALIBRATE,

  PORT_B_GAIN_IN,
  PORT_B_GAIN_OUT,
  PORT_B_DRY_OUT,
  PORT_B_DELAY,
  PORT_B_MUTE,
  PORT_B_DCFLIP,
  PORT_B_CALIBRATE,

  PORT_C_GAIN_IN,
  PORT_C_GAIN_OUT,
  PORT_C_DRY_OUT,
  PORT_C_DELAY,
  PORT_C_MUTE,
  PORT_C_DCFLIP,
  PORT_C_CALIBRATE,

  PORT_D_GAIN_IN,
  PORT_D_GAIN_OUT,
  PORT_D_DRY_OUT,
  PORT_D_DELAY,
  PORT_D_MUTE,
  PORT_D_DCFLIP,
  PORT_D_CALIBRATE,

  PORT_CONTROL,
  PORT_NOTIFY,
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
  PORT_NOISEGATE_GAIN,
  PORT_TUNER_ON,
  PORT_TUNER_BASE_FREQ,
  PORT_TUNER_NOTE,
  PORT_TUNER_CENTS_OFF,
  PORT_TUNER_FREQ,

  PORT_COUNT
};

enum nb_lv2_lane_param {
  NB_LV2_LANE_GAIN_IN = 0,
  NB_LV2_LANE_GAIN_OUT,
  NB_LV2_LANE_DRY_OUT,
  NB_LV2_LANE_DELAY,
  NB_LV2_LANE_MUTE,
  NB_LV2_LANE_DCFLIP,
  NB_LV2_LANE_CALIBRATE,

  NB_LV2_LANE_PORT_COUNT
};

static inline uint32_t nb_lv2_lane_port (size_t lane, uint32_t first) {
  return first + (uint32_t) lane * NB_LV2_LANE_PORT_COUNT;
}

static inline bool nb_lv2_decode_lane_port (
    uint32_t port,
    size_t *lane,
    uint32_t *param) {

  if (port < PORT_A_GAIN_IN || port > PORT_D_CALIBRATE)
    return false;

  const uint32_t offset = port - PORT_A_GAIN_IN;
  const uint32_t p = offset % NB_LV2_LANE_PORT_COUNT;
  const size_t l = (size_t) (offset / NB_LV2_LANE_PORT_COUNT);

  if (l >= 4)
    return false;

  if (lane)
    *lane = l;
  if (param)
    *param = p;

  return true;
}

class c_lv2_urids {
public:
  LV2_URID_Map *map = NULL;
  LV2_Atom_Forge forge = {};

  LV2_URID urid_atom_eventTransfer = 0;
  LV2_URID urid_patch_Set = 0;
  LV2_URID urid_patch_Get = 0;
  LV2_URID urid_patch_property = 0;
  LV2_URID urid_patch_value = 0;
  LV2_URID urid_atom_Path = 0;
  LV2_URID urid_atom_String = 0;
  LV2_URID urid_atom_Blank = 0;
  LV2_URID urid_atom_Float = 0;
  LV2_URID urid_atom_Int = 0;
  LV2_URID urid_atom_Vector = 0;
  LV2_URID urid_atom_URID = 0;
  LV2_URID urid_model [NB_NUM_MODELS] = { 0 };
  LV2_URID urid_meters = 0;
  LV2_URID urid_stats = 0;
  LV2_URID urid_calib_target_db = 0;
  LV2_URID urid_calib_bass = 0;
  LV2_URID urid_atom_Sequence = 0;
  
  inline bool init (LV2_URID_Map *m) {
    map = m;
    if (!map || !map->map)
      return false;

    lv2_atom_forge_init (&forge, map);

    urid_atom_eventTransfer =
      map->map (map->handle, LV2_ATOM__eventTransfer);
    urid_patch_Set =
      map->map (map->handle, LV2_PATCH__Set);
    urid_patch_Get =
      map->map (map->handle, LV2_PATCH__Get);
    urid_patch_property =
      map->map (map->handle, LV2_PATCH__property);
    urid_patch_value =
      map->map (map->handle, LV2_PATCH__value);
    urid_atom_Path =
      map->map (map->handle, LV2_ATOM__Path);
    urid_atom_String =
      map->map (map->handle, LV2_ATOM__String);
    urid_atom_Blank =
      map->map (map->handle, LV2_ATOM__Blank);
    urid_atom_Float =
      map->map (map->handle, LV2_ATOM__Float);
    urid_atom_Int =
      map->map (map->handle, LV2_ATOM__Int);
    urid_atom_Vector =
      map->map (map->handle, LV2_ATOM__Vector);
    urid_atom_URID =
      map->map (map->handle, LV2_ATOM__URID);
    urid_atom_Sequence =
      map->map (map->handle, LV2_ATOM__Sequence);

    urid_model [0] =
      map->map (map->handle, NB_URI "#ModelA");
    urid_model [1] =
      map->map (map->handle, NB_URI "#ModelB");
    urid_model [2] =
      map->map (map->handle, NB_URI "#ModelC");
    urid_model [3] =
      map->map (map->handle, NB_URI "#ModelD");
    urid_meters =
      map->map (map->handle, NB_URI "#Meters");
    urid_stats =
      map->map (map->handle, NB_URI "#Stats");
    urid_calib_target_db =
      map->map (map->handle, NB_URI "#CalibTargetDb");
    urid_calib_bass =
      map->map (map->handle, NB_URI "#CalibBass");

    return true;
  }
};

#ifdef LV2_UI

#include <lv2/ui/ui.h>

#include "ui.h"

enum _ui_feedback_type {
  ATOM_METERS,
  ATOM_STATS,
  ATOM_UNKNOWN
};

class c_lv2_ui : public c_neuralblender_ui, public c_lv2_urids {
public:
  LV2UI_Write_Function write = NULL;
  LV2UI_Controller controller = NULL;
  LV2UI_Port_Subscribe *subscribe = NULL;
  LV2UI_Resize *resize = NULL;
  bool updating_from_host = false;
  float tuner_freq_value = 0.0f;
  float tuner_note_value = 0.0f;
  float tuner_cents_value = 0.0f;

  void write_control (uint32_t port, float value);
  uint32_t lane_port (size_t lane, uint32_t first) const;
  bool write_model_path (size_t which, const char *filename);
  bool write_float_property (LV2_URID property, float value);
  bool write_int_property (LV2_URID property, int32_t value);
  void request_current_state ();

  bool load_model (size_t which, const char *filename) override;
  void on_gain_in (c_widget *w, float f) override;
  void on_gain_out (c_widget *w, float f) override;
  void on_dry_out (c_widget *w, float f) override;
  void on_delay (c_widget *w, float f) override;
  void on_filebrowse (c_widget *w) override;
  void on_fileselected (c_widget *w, const char *path) override;
  void on_fileclear (c_widget *w) override;
  void on_mute (c_widget *w, bool b) override;
  void on_dcflip (c_widget *w, bool b) override;
  void on_calibrate (c_widget *w, bool b) override;
  void on_muteall (c_widget *w, bool b) override;
  void on_excl (c_widget *w, int n) override;
  void on_bypass (c_widget *w, bool b) override;
  void on_about (c_widget *w);
  void on_vu (c_widget *w, bool b) override;
  void on_linked_calib (c_widget *w, bool b) override;
  void on_calib_bass (c_widget *w, bool b) override;
  void on_noisegate (c_widget *w, bool b) override;
  void on_noisethresh (c_widget *w, float f) override;
  void on_noiseattack (c_widget *w, float f) override;
  void on_noisehold (c_widget *w, float f) override;
  void on_noiserelease (c_widget *w, float f) override;
  void on_threshgain (c_widget *w, float f) override;
  void on_tuner (c_widget *w, bool b) override;
  void apply_prefs (t_prefs &p) override;
  bool request_window_size (int w, int h) override;

  void set_port_value (uint32_t port, float value);
  void set_model_path (size_t which, const char *path);
  void set_model_property (LV2_URID property, const char *path);
  void redraw_meters_now ();
  void set_ui_values (const LV2_Atom *value, _ui_feedback_type type);
  void handle_atom_event (const LV2_Atom *atom);
  void subscribe_ports ();
};

#endif // LV2_UI
