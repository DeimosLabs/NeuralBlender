
/* NeuralBlender - RTNeural / NAM based amp modeler
 *
 * -----------------------------------------------------------------------------
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 * ----------------------------------------------------------------------------
 *
 * Core header file
*/

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <algorithm>
#include <mutex>
#include <cmath>
#include <atomic>

#define RTNEURAL_DEFAULT_ALIGNMENT 16
#include "RTNeural/RTNeural.h"
#include "NAM/dsp.h"
#include "NAM/get_dsp.h"

#ifdef HAVE_FFTW
#include "fftw3.h"
#endif

#include "meter.h"
#include "tuner.h"

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#define SEMITONE_MULTIPLIER      1.0594630943592953
#define NB_NUM_PEDALS            4
#define NB_NUM_MODELS            4
#define NB_NUM_CABS              4
#define NB_MAX_LANES             4
#define NB_STATS_PER_LANE        3 // dsp->ui: delay frames, model type, trim

#define MAX_DELAY_MS             30
#define MAX_DELAY_FRAMES         (MAX_DELAY_MS * 192)
#define MAX_BLOCK_SIZE           8192
#define DB_SILENCE               -120.0f
#define DB_CALIB_TARGET_DEFAULT  -18.0f
#define GAIN_DB_MIN              -40.0f
#define GAIN_DB_MAX              40.0f
#define CALIB_TARGET_DB_MIN      -40.0f
#define CALIB_TARGET_DB_MAX      0.0f
#define NOISEGATE_THRESH_MIN     DB_SILENCE
#define NOISEGATE_THRESH_MAX     -6.0f
#define WARMUP_BLOCKS            5
#define NB_XFADE_MS              10.0f
#define NB_LANE_XFADE_MS         NB_XFADE_MS
#define TUNER_THRESH_DB          -40.0f

enum _lane_bank {
  BANK_PEDAL = 0,
  BANK_AMP,
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

extern const char *g_build_timestamp;

static inline float db_to_gain (float db) {
  return powf (10.0f, db / 20.0f);
}

static inline float gain_to_db (float gain) {
  if (gain <= 0.0f)
    return DB_SILENCE;

  return 20.0f * log10f(gain);
}

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
    amp_bypass   = other.amp_bypass;
    cab_bypass   = other.cab_bypass;

    for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank)
      banks [bank] = other.banks [bank];

    return *this;
  }

  std::string current_dir;
  bool bypass             = false;
  bool pedal_bypass       = false;
  bool amp_bypass         = false;
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
  c_neuralblender_lane_state (&lanes) [NB_NUM_MODELS];
};

// a simple but effective noise gate
class c_noisegate {
public:
  void process_block (float *in, float *out, uint32_t nframes);
  void set_threshold (float thresh_db);
  void set_attack (float attack_ms);
  void set_hold (float hold_ms);
  void set_release (float release_ms);
  float get_current_gain ();
  float get_current_db ();
  
  void set_samplerate (int sr);
  float threshold_db = -60.0f;
  float attack_ms = 2.0f;
  float hold_ms = 10.0f;
  float release_ms = 20.0f;

private:
  void update_coeffs ();

  float samplerate = 48000.0f;
  float env = 0.0f;
  float gain = 1.0f;
  int hold_samples = 0;
  
  float threshold_gain = 0.001f;
  float attack_coeff = 0.0f;
  int hold_coeff = 0.0f;
  float release_coeff = 0.0f;

  bool coeffs_dirty = true;

  std::atomic<float> display_gain = 0.0f;
};

class c_delayline {
public:
  c_delayline ();
  ~c_delayline ();
  //float process_sample (float x);
  void process_block (float *in, float *out, uint32_t nframes);
  bool set_frames (uint32_t f);
  uint32_t frames () const;
  void clear ();

  uint32_t m_delay_frames = 0;
private:
  std::vector<float> m_buffer;
  uint32_t m_writepos = 0;
};

#ifdef HAVE_FFTW

typedef struct {
  float r = 0.0f;
  float i = 0.0f;
} cpx;

class c_convolver {
public:
  c_convolver ();
  ~c_convolver ();
  c_convolver (const c_convolver &) = delete;
  c_convolver &operator= (const c_convolver &) = delete;
  
  bool load_ir (const float *ir, uint32_t nframes, uint32_t samplerate = 0);
  bool load_ir_from_file (const char *filename, int channel = 0);
  void clear ();
  void reset ();
  void clear_fft_state ();
  bool loaded () const;
  bool ready () const;
  void process_block (const float *in, float *out, uint32_t nframes);
  void set_samplerate (uint32_t samplerate);
  void set_blocksize (uint32_t nframes);
  bool set_pitch_semitones (float semitones);
  
private:
  bool rebuild_for_blocksize (uint32_t nframes);
  bool rebuild_resampled_ir ();
  
  std::vector<float> m_ir_source;       // cleaned source IR
  std::vector<float> m_ir;              // current pitch-resampled IR
  std::vector<float> m_overlap;

  std::vector<cpx> m_fft_out;
  std::vector<std::vector<cpx>> m_ir_fft;
  std::vector<std::vector<cpx>> m_accum_fft;

  bool               m_loaded           = false;
  bool               m_ready            = false;
  float              m_pitch_semitones  = 0.0f;
  uint32_t           m_ir_samplerate    = 0;
  uint32_t           m_samplerate       = 48000;
  uint32_t           m_blocksize        = 0;
  uint32_t           m_partition_size   = 0;
  uint32_t           m_num_partitions   = 0;
  uint32_t           m_fft_size         = 0;
  uint32_t           m_freq_bins        = 0;
  uint32_t           m_accum_pos        = 0;
  
  fftwf_plan m_forward_plan = NULL;
  fftwf_plan m_inverse_plan = NULL;
  
  float *m_fftw_time_in = NULL;
  float *m_fftw_time_out = NULL;
  
  fftwf_complex *m_fftw_freq_in = NULL;
  fftwf_complex *m_fftw_freq_out = NULL;
};

#else

// just a stub class that will pass signal through
class c_convolver {
public:
  c_convolver () { }
  ~c_convolver () { }

  bool load_ir (const float *, uint32_t, uint32_t = 0) { return false; }
  bool load_ir_from_file (const char *, int = 0) { return false; }
  void clear () { }
  void reset () { }
  void clear_fft_state () { }
  bool loaded () const { return false; }
  bool ready () const { return false; }
  void process_block (const float *in, float *out, uint32_t nframes) {
    if (!in || !out)
      return;
    for (uint32_t i = 0; i < nframes; ++i)
      out [i] = in [i];
  }
  void set_samplerate (uint32_t) { }
  void set_blocksize (uint32_t) { }
  bool set_pitch_semitones (float) { return false; }
};

#endif

class c_neuralamp {
public:
  c_neuralamp ();
  ~c_neuralamp ();
  void set_samplerate (uint32_t sr);
  void set_blocksize (uint32_t bs);
  bool set_ir_pitch (float semitones);

  bool request_load_model (const std::string &filename = "");
  bool load_model ();
  void unload_model ();
  void reset ();
  float calibrate (float *data, size_t sz);
  _engine_mode engine () const { return m_engine_mode; }

  //float process_sample (float x);
  void process_block (float *in, float *out, uint32_t nframes);

  bool ready_to_load ();
  bool loaded () const;
  std::string model_filename () const;
  std::atomic<float> trim { 1.0f };
  std::atomic<float> effective_trim { 1.0f };

  size_t      bank            = -1;
  size_t      lane            = -1;
  std::string filename        = "";
  float       gain_in         = 1.0f;
  float       ir_pitch_semitones = 0.0f;
  float       gain_out        = 1.0f;
  float       dry_out         = 0.0f;
  c_delayline delay;
  float       calib_target_db = DB_CALIB_TARGET_DEFAULT;
  uint32_t    samplerate      = 48000;
  uint32_t    blocksize       = 0;
  std::atomic<bool> mute      { false };
  std::atomic<_ramp_state> ramp = RAMP_PLAYING;
  uint32_t    ramp_pos       = 0;
  uint32_t    ramp_len       = 0;
  int         warmup          = 5;
  bool        dcflip          = false;
  bool        do_calib        = false;
  
  // for debugging
  size_t      block_counter   = 0;

private:
  void reset_unlocked ();
  bool load_model_now (const std::string &filename);
  bool load_json ( const std::string &filename);
  bool load_nam ( const std::string &filename);
  float get_block_rms (float *data, size_t sz);
  
  // model impl.
  std::unique_ptr<nam::DSP> m_nam_model;
  std::unique_ptr<RTNeural::Model<float>> m_rtneural_model;
  c_convolver m_convolver;
  
  mutable std::mutex model_mutex;
  mutable std::mutex pending_mutex;
  std::string pending_filename;
  std::atomic<bool> m_loaded { false };

  _engine_mode m_engine_mode = ENGINE_NONE;
};

struct c_model_bank {
  size_t num_lanes = NB_NUM_MODELS;
  c_neuralamp lanes [NB_NUM_MODELS];

  c_vudata *meter_in = nullptr;
  c_vudata *meters_out [NB_NUM_MODELS] = {};

  std::atomic<bool> lane_mute [NB_NUM_MODELS] = {};
  int exclusive_lane = 0;       // 0 off, 1..N selected
  bool linked_calib = false;

  uint32_t active_mask = 0;
};

// creates NB_NUM_MODELS instances of c_delayline and c_neuralamp
class c_neuralblender {
public:
  c_neuralblender ();
  ~c_neuralblender ();
  void set_samplerate (uint32_t sr);
  void set_blocksize (uint32_t bs);
  //void process_block_main (float *in, float *out, uint32_t nframes);
  uint32_t make_active_lane_mask (_lane_bank bank) const;
  float *prepare_input_buffer (float *in, float *out, uint32_t nframes);
  void render_lane (_lane_bank bank, size_t lane, float *in, uint32_t nframes);
  void render_mix (float *in, float *out, uint32_t nframes, _lane_bank bank,
                   uint32_t old_mask, uint32_t new_mask,
                   uint32_t xfade_pos, uint32_t xfade_len);
  void process_block (float *in, float *out, uint32_t nframes);
  bool load_model (_lane_bank bank, size_t which, const char *filename);
  bool unload_model (_lane_bank bank, size_t which);
  bool set_delay_frames (_lane_bank bank, size_t which, uint32_t frames);
  bool set_delay_ms (_lane_bank bank, size_t which, float ms);
  bool set_gain_in (_lane_bank bank, size_t which, float g);
  bool set_ir_pitch (_lane_bank bank, size_t which, float semitones);
  bool set_gain_out (_lane_bank bank, size_t which, float g);
  bool set_dry_out (_lane_bank bank, size_t which, float g);
  bool set_lane_mute (_lane_bank bank, size_t which, bool muted);
  bool set_exclusive_lane (_lane_bank bank, int lane);
  void set_bypass (bool bypass);
  void set_pedal_bypass (bool bypass);
  void set_amp_bypass (bool bypass);
  void set_cab_bypass (bool bypass);
  bool lane_mute (_lane_bank bank, size_t which) const;
  bool bypass () const;
  bool pedal_bypass () const;
  bool amp_bypass () const;
  bool cab_bypass () const;
  float delay_ms (_lane_bank bank, size_t which) const;
  float delay_ms (size_t which) const;
  void get_state (c_neuralblender_state &state) const;
  bool dcflip (_lane_bank bank, size_t which, bool b);
  bool calib_on (_lane_bank bank, size_t which, bool b);
  bool is_dcflipped (_lane_bank bank, size_t which);
  bool is_calib_on (_lane_bank bank, size_t which);
  bool set_calib_target_db (float f);
  bool calibrate (_lane_bank bank, size_t which, bool bass);
  bool calibrate_linked (_lane_bank bank, bool bass);
  void update_input_meter (_lane_bank bank, float *in, uint32_t nframes);
  void update_loaded_output_meters (_lane_bank bank);
  void render_bank (_lane_bank bank,
                    float *in,
                    float *out,
                    uint32_t nframes,
                    uint32_t old_mask,
                    uint32_t new_mask,
                    uint32_t xfade_pos,
                    uint32_t xfade_len);
  int tuner_freq ();
  bool set_master_gain (float db);
  bool set_presence (float pres);
  void update_effective_trim ();
  
  //static void get_calib_data (std::vector<float> &v, bool bass);
  
  c_noisegate noisegate;
  c_model_bank banks [BANK_COUNT];
  c_pitchtracker pitchtracker;
  
  float master_gain = 1.0f;
  float presence = 0.0f;
  bool do_vu = true;
  bool noisegate_on = false;
  bool tuner_on = false;
  float tuner_base_freq = 440.0f;
  float tuner_note = 0.0f;
  float tuner_cents_off = 0.0f;
  bool mute_all = false;
  bool linked_calib = false;
  int calib_source = 0; // 0=guitar, 1=bass
  std::atomic<_ramp_state> ramp = RAMP_PLAYING;
  c_vudata *meter_masterin  = NULL;
  c_vudata *meter_masterout = NULL;

private:
  c_model_bank &which_bank (_lane_bank bank);
  const c_model_bank &which_bank (_lane_bank bank) const;
  c_neuralamp &which_amp (_lane_bank bank, size_t lane);
  const c_neuralamp &which_amp (_lane_bank bank, size_t lane) const;
  void update_mutes ();
  void request_mix_update ();
  bool consistent_calib_state (bool &enabled,
      c_neuralblender_state &state) const;
  
  c_convolver           m_conv_presence;
  std::vector<float>    m_delay_bufs [NB_NUM_MODELS];
  std::vector<float>    m_model_bufs [NB_NUM_MODELS];
  std::vector<float>    m_input_buf;
  std::vector<float>    m_stage_buf_a;
  std::vector<float>    m_stage_buf_b;
  std::vector<float>    m_presence_buf;
  std::atomic<bool>     m_bypass          { false };
  std::atomic<bool>     m_pedal_bypass    { false };
  std::atomic<bool>     m_amp_bypass      { false };
  std::atomic<bool>     m_cab_bypass      { false };
  std::atomic<bool>     xfade_pending     { false };
  std::atomic<uint32_t> active_lane_mask  { 0 };
  std::atomic<uint32_t> pending_lane_mask { 0 };
  std::atomic<uint32_t> loaded_lane_mask  { 0 };
  bool xfade_active = false;
  uint32_t xfade_old_mask = 0;
  uint32_t xfade_new_mask = 0;
  uint32_t xfade_pos = 0;
  uint32_t xfade_len = 0;

  bool       m_ready = false;
  uint32_t   m_samplerate = 48000;
  uint32_t   m_blocksize = 256;
};
