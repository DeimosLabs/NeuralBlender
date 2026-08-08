
/* NeuralBlender - RTNeural / NAM based amp modeler
 *
 * Config file reading/writing
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

#include "constants.h"

#define CONFIG_FILE_NAME             ".config/neuralblender.conf"
#define CONFIG_KEY_NAME_MODEL_CWD    "model_path"
#define CONFIG_KEY_NAME_IR_CWD       "ir_path"
#define CONFIG_KEY_NAME_ADV          "show_advanced"
#define CONFIG_KEY_NAME_EXCL         "excl_default"
#define CONFIG_KEY_NAME_CALIB        "calib_default"
#define CONFIG_KEY_NAME_CALIB_TARGET "calib_target_db"
#define CONFIG_KEY_NAME_VU_SCALE     "vu_scale_db"
#define CONFIG_KEY_NAME_VU_HEADROOM  "vu_headroom_db"
#define CONFIG_KEY_NAME_BYP_DCLICK   "bypass_doubleclick"
#define CONFIG_KEY_NAME_BYP_RCLICK   "bypass_rightclick"
#define CONFIG_KEY_NAME_TOOLTIPS     "show_tooltips"
#define CONFIG_DEFAULT_DIR           "/"

class c_configfile {
public:
  c_configfile ();
  bool read_file (std::string path);
  bool read_file ();
  bool write_file (std::string path);
  bool write_file ();
  std::string get_path ();
  bool set_item (size_t n, std::string value);
  std::string get_item (size_t n);
  bool set_item (std::string name, std::string value);
  std::string get_item (std::string name);
  int find_item (std::string name);
  void dump (); // for debugging

  static bool istrue (std::string name);
  
private:
  void process_in (int which, std::string value);
  void process_out (int which, std::string value);
};

enum _eq_band_mode {
  EQ_OFF,
  EQ_HIPASS,
  EQ_LOWSHELF,
  EQ_BELL,
  EQ_HISHELF,
  EQ_LOWPASS,
  EQ_KEEP // means don't change current setting
};

extern float g_defaultfreqs [];
extern _eq_band_mode g_defaultmodes [];

enum _lane_bank {
  BANK_PEDAL = 0,
  BANK_EQPRE,
  BANK_AMP,
  BANK_EQPOST,
  BANK_CAB,
  BANK_COUNT
};

enum _engine_mode {
  ENGINE_NONE,
  ENGINE_NAM_A1,
  ENGINE_NAM_A2,
  ENGINE_JSON,
  ENGINE_IR,        // TODO
  ENGINE_UNKNOWN
};

enum _ramp_state {
  RAMP_PLAYING,  // normal processing
  RAMP_START,    // one block fade out, using current model/audio
  RAMP_LOADING,  // silence while loader may own mutex
  RAMP_WARMUP,   // model loaded, process and discard "warmup" blocks
  RAMP_END       // one block fade in
};

enum _mix_mode {
  MIX_LANES,
  MIX_PASSTHROUGH,
  MIX_SILENCE
};

struct c_mix_state {
  _mix_mode mode;
  uint32_t lane_mask;
};

struct c_neuralblender_lane_state {
  std::string filename;
  float gain_in = 1.0f;
  float ir_pitch_semitones = 0.0f;
  float gain_out = 1.0f;
  float dry_out = 0.0f;
  float delay_ms = 0.0f;
  bool lane_mute = false;
  bool loaded = false;
  bool dcflip = false;
  bool do_calib = false;
};

struct c_neuralblender_bank_state {
  c_neuralblender_lane_state lanes [NB_NUM_MODELS];
  int  exclusive_lane = 0;
  bool linked_calib = false;
};

struct c_eq_state {
  c_eq_state () {
    for (int i = 0; i < EQ_NUM_BANDS; ++i) {
      enabled [i] = false;
      mode [i] = g_defaultmodes [i];
      slope [i] = 1;
      freq [i] = g_defaultfreqs [i];
      gain_db [i] = 0.0f;
      q [i] = 1.0f;
    }
  }

  bool on = false;
  float master_gain_db = 0.0f;
  bool enabled [EQ_NUM_BANDS] = {};
  _eq_band_mode mode [EQ_NUM_BANDS] = {};
  int slope [EQ_NUM_BANDS] = {};
  float freq [EQ_NUM_BANDS] = {};
  float gain_db [EQ_NUM_BANDS] = {};
  float q [EQ_NUM_BANDS] = {};
};

struct c_neuralblender_state {
  c_neuralblender_state () : lanes (banks [BANK_AMP].lanes) { }
  c_neuralblender_state (const c_neuralblender_state &other)
      : lanes (banks [BANK_AMP].lanes) {
    *this = other;
  }
  c_neuralblender_state &operator= (const c_neuralblender_state &other) {
    if (this == &other)
      return *this;

    current_dir = other.current_dir;
    bypass = other.bypass;
    do_excl = other.do_excl;
    do_vu = other.do_vu;
    showadvanced = other.showadvanced;
    mute_all = other.mute_all;
    master_gain = other.master_gain;
    presence = other.presence;
    tuner_on = other.tuner_on;
    tuner_base_freq = other.tuner_base_freq;
    noisegate_on = other.noisegate_on;
    noisethresh = other.noisethresh;
    noiseattack = other.noiseattack;
    noisehold = other.noisehold;
    noiserelease = other.noiserelease;
    calib_target_db = other.calib_target_db;
    calib_source = other.calib_source;

    pedal_bypass = other.pedal_bypass;
    eqpre_bypass = other.eqpre_bypass;
    amp_bypass   = other.amp_bypass;
    eqpost_bypass = other.eqpost_bypass;
    cab_bypass   = other.cab_bypass;
    eqpre = other.eqpre;
    eqpost = other.eqpost;

    for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank)
      banks [bank] = other.banks [bank];

    return *this;
  }

  std::string current_dir;
  bool bypass             = false;
  bool pedal_bypass       = false;
  bool eqpre_bypass       = true;
  bool amp_bypass         = false;
  bool eqpost_bypass      = true;
  bool cab_bypass         = false;
  bool mute_all           = false;
  bool do_excl            = false;
  bool do_vu              = true;
  bool showadvanced       = false;
  float master_gain       = 1.0f;
  float presence          = 0.0f;
  bool tuner_on           = false;
  float tuner_base_freq   = 440.0f;
  bool noisegate_on       = false;
  float noisethresh       = -60.0f;
  float noiseattack       = 2.0f;
  float noisehold         = 10.0f;
  float noiserelease      = 20.0f;
  float calib_target_db   = DB_CALIB_TARGET_DEFAULT;
  int calib_source        = 0; // 0=guitar, 1=bass

  c_neuralblender_bank_state banks [BANK_COUNT];
  c_eq_state eqpre;
  c_eq_state eqpost;
  c_neuralblender_lane_state (&lanes) [NB_NUM_MODELS];
};
