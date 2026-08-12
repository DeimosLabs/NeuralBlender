
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
#include <chrono>
#include <iostream>
#include <array>
#include <functional>

#include "state.h" // includes constants.h

#ifdef HAVE_FFTW
#include "fftw3.h"
#endif

#define RTNEURAL_DEFAULT_ALIGNMENT 16
#include "RTNeural/RTNeural.h"
#include "NAM/dsp.h"
#include "NAM/get_dsp.h"

#include "data.h"

#include "meter.h"
#include "tuner.h"
#include "spectrum.h"

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

typedef struct {
  float r = 0.0f;
  float i = 0.0f;
} cpx;

#ifndef NB_DEBUG_RATE_HELPERS
#define NB_DEBUG_RATE_HELPERS

inline uint64_t now_ms () {
  using clock = std::chrono::steady_clock;
  return std::chrono::duration_cast<std::chrono::milliseconds> (
    clock::now ().time_since_epoch ()
  ).count ();
}

struct c_printfps {
  std::string m_str;
  uint64_t count = 0;
  uint64_t last = now_ms ();
  
  c_printfps (std::string str) : m_str (std::move (str)) { }
  
  inline void tick () {
    count++;
    
    uint64_t now = now_ms ();
    if (now - last >= 1000) {
      std::cout << m_str << count << "\n";
      count = 0;
      last = now;
    }
  }
};
#endif

// TODO: fix this shit
#ifndef DEBUG_SHOW_RATE
//#ifdef DEBUG
#define DEBUG_SHOW_RATE(x) {static c_printfps fps(x);fps.tick();}
//#else
//#define DEBUG_SHOW_RATE(x)
//#endif
#endif

bool read_file_to_mem (const char *fn, std::vector<unsigned char> &out);

extern const char *g_build_timestamp;

/*static inline float db_to_gain (float db) {
  return powf (10.0f, db / 20.0f);
}

static inline float gain_to_db (float gain) {
  if (gain <= 0.0f)
    return DB_SILENCE;
  
  return 20.0f * log10f(gain);
}*/

struct t_neuralblender_error {
  size_t code = NB_ERROR_NONE;
  size_t bank = BANK_NONE;
  size_t lane = 0;
  std::string filename;
};

#ifdef HAVE_FFTW

class c_spectrum_analyzer {
public:
  c_spectrum_analyzer ();
  ~c_spectrum_analyzer ();
  c_spectrum_analyzer &operator= (const c_spectrum_analyzer &) = delete;
  c_spectrum_analyzer (const c_spectrum_analyzer &) = delete;
  
  void set_samplerate (int samplerate);
  void process_block (const float *in, size_t count);
  
  // Called outside the audio thread.
  bool analyze ();
  bool copy_bins (float *out, size_t count) const;
  
  // On EQ graph show/hide.
  void start ();
  void stop ();

private:
  void publish_snapshot ();
  bool copy_latest_snapshot (float *out, uint64_t &sequence) const;
  void publish_magnitudes (const float *values);
  
  int samplerate = 0;
  size_t write_pos = 0;
  uint64_t total_samples = 0;
  uint64_t next_snapshot_at = SPECTRUM_FFT_SIZE;
  int write_snapshot = 0;
  std::atomic<int> published_snapshot { -1 };
  mutable std::atomic<int> reading_snapshot { -1 };
  std::atomic<uint64_t> published_sequence { 0 };
  uint64_t snapshot_sequences [3] = { 0 };
  uint64_t analyzed_sequence = 0;
  
  std::array<float, SPECTRUM_FFT_SIZE> ring {};
  std::array<float, SPECTRUM_FFT_SIZE> snapshots [3] {};
  std::array<float, SPECTRUM_FFT_SIZE> fft_input {};
  std::array<fftwf_complex, SPECTRUM_FFT_SIZE / 2 + 1> fft_output {};
  
  std::array<float, SPECTRUM_BINS> frequencies {};
  std::array<float, SPECTRUM_BINS> magnitude_work {};
  std::array<float, SPECTRUM_BINS> magnitude_buffers [3] {};
  int write_magnitudes = 0;
  std::atomic<int> published_magnitudes { -1 };
  mutable std::atomic<int> reading_magnitudes { -1 };
  
  std::array<float, SPECTRUM_FFT_SIZE> window {};
  float window_sum = 0.0f;
  
  fftwf_plan fft_plan = nullptr;
  std::atomic<bool> enabled { false };
  bool capture_active = false;

};

#else

class c_spectrum_analyzer {
public:

  void set_samplerate (int) {}
  void process_block (const float *, size_t) {}
  bool analyze () { return false; }
  bool copy_bins (float *, size_t) const { return false; }
  void start () {}
  void stop () {}
};

#endif

class c_biquad {
public:
  void set_peak (float sr, float freq, float gain_db, float q,
                 _eq_band_mode = EQ_KEEP);
  void disable ();
  
  inline float process (float x) {
    const int n =
      (mode == EQ_HIPASS || mode == EQ_LOWPASS)
        ? std::clamp (slope, 1, 4)
        : 1;
    
    for (int i = 0; i < n; i++) {
      const float y = b0 * x + z1 [i];
      z1 [i] = b1 * x - a1 * y + z2 [i];
      z2 [i] = b2 * x - a2 * y;
      x = y;
    }
    
    return x;
  }
  
  /* old version w/o slope
  inline float process (float x) {
    const float y = b0 * x + z1;
    z1 = b1 * x - a1 * y + z2;
    z2 = b2 * x - a2 * y;
    return y;
  }*/
  
  inline void reset () {
    for (int i = 0; i < EQ_SLOPE_MAX; i++) {
      z1 [i] = 0.0f;
      z2 [i] = 0.0f;
    }
  }
  float response_db (float freq, float samplerate) const;
  
  _eq_band_mode mode = EQ_OFF;
  
  int slope = 1;
  float b0 = 1.0f;
  float b1 = 0.0f;
  float b2 = 0.0f;
  float a1 = 0.0f;
  float a2 = 0.0f;
  
  float z1 [EQ_SLOPE_MAX] = { 0.0f };
  float z2 [EQ_SLOPE_MAX] = { 0.0f };
};

class c_eq {
public:
  c_eq ();
  void set_samplerate (int sr);
  void reset ();
  void process_block (float *buf, uint32_t nframes);
  bool set_enabled (int band, bool enabled);
  void set_band (int band, float freq, float gain_db, float q, 
                 _eq_band_mode mode = EQ_KEEP, int slope = 1);
  void set_master_gain_db (float db);
  
  bool on                            = false;
  int samplerate                     = 0;
  float               master_gain_db = 0.0f;
  bool enabled        [EQ_NUM_BANDS] = { false };
  _eq_band_mode mode  [EQ_NUM_BANDS] = { EQ_OFF };
  int slope           [EQ_NUM_BANDS] = {};
  float freq          [EQ_NUM_BANDS] = { 0 };
  float gain_db       [EQ_NUM_BANDS] = { 0 };
  float q             [EQ_NUM_BANDS] = { 0 };
  c_biquad bands      [EQ_NUM_BANDS];
  c_biquad old_bands  [EQ_NUM_BANDS];
  uint32_t coeff_xfade_pos [EQ_NUM_BANDS] = { 0 };
  uint32_t coeff_xfade_len [EQ_NUM_BANDS] = { 0 };
  
  inline bool analyze_spectra () {
    const bool input_changed  = m_spectrum_input.analyze ();
    const bool output_changed = m_spectrum_output.analyze ();
    return input_changed || output_changed;
  }
  
  inline void start_spectra () {
    m_spectrum_input.start ();
    m_spectrum_output.start ();
  }
  inline void stop_spectra () {
    m_spectrum_input.stop ();
    m_spectrum_output.stop ();
  }
  inline bool copy_spectrum_input_bins (float *buf, size_t count) const {
    return m_spectrum_input.copy_bins (buf, count);
  }
  inline bool copy_spectrum_output_bins (float *buf, size_t count) const {
    return m_spectrum_output.copy_bins (buf, count);
  }

private:
  c_spectrum_analyzer m_spectrum_input;
  c_spectrum_analyzer m_spectrum_output;
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

class c_convolver {
public:
  c_convolver ();
  ~c_convolver ();
  c_convolver (const c_convolver &) = delete;
  c_convolver &operator= (const c_convolver &) = delete;
  
  size_t load_ir (const float *ir, uint32_t nframes, uint32_t samplerate = 0);
  size_t load_ir_from_file (const char *filename, int channel = 0);
  void clear ();
  void reset ();
  void clear_fft_state ();
  bool loaded () const;
  bool ready () const;
  void process_block (const float *in, float *out, uint32_t nframes);
  void set_samplerate (uint32_t samplerate);
  void set_blocksize (uint32_t nframes);
  size_t set_pitch_semitones (float semitones);

private:
  bool rebuild_for_blocksize (uint32_t nframes);
  bool rebuild_resampled_ir ();
  void process_fft_block (const float *in, float *out);
  void process_direct_block (const float *in, float *out, uint32_t nframes);
  void update_direct_history (const float *in, uint32_t nframes);
  static bool resample_audio (const std::vector<float> in,  int in_rate,
                                    std::vector<float> out, int out_rate);
  
  std::vector<float> m_ir_source;       // cleaned source IR
  std::vector<float> m_ir;              // current pitch-resampled IR
  std::vector<float> m_overlap;
  std::vector<float> m_direct_history;
  std::vector<float> m_variable_input;
  std::vector<float> m_fft_sync_out;
  
  std::vector<cpx> m_fft_out;
  std::vector<std::vector<cpx>> m_ir_fft;
  std::vector<std::vector<cpx>> m_accum_fft;
  
  bool               m_loaded           = false;
  bool               m_ready            = false;
  float              m_pitch_semitones  = 0.0f;
  uint32_t           m_ir_samplerate    = 0;
  uint32_t           m_samplerate       = 0;
  uint32_t           m_blocksize        = 0; // DUUUH
  uint32_t           m_partition_size   = 0;
  uint32_t           m_num_partitions   = 0;
  uint32_t           m_fft_size         = 0;
  uint32_t           m_freq_bins        = 0;
  uint32_t           m_accum_pos        = 0;
  uint32_t           m_direct_pos       = 0;
  
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
  
  size_t load_ir (const float *, uint32_t, uint32_t = 0) { return NB_ERROR_INTERNAL; }
  size_t load_ir_from_file (const char *, int = 0) { return NB_ERROR_INTERNAL; }
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
  size_t set_pitch_semitones (float) { return NB_ERROR_INTERNAL; }
};

#endif

class c_neuralamp {
public:
  c_neuralamp ();
  ~c_neuralamp ();
  void set_samplerate (uint32_t sr);
  void set_blocksize (uint32_t bs);
  bool set_ir_pitch (float semitones);
  
  size_t request_load_model (const std::string &filename = "");
  size_t load_model ();
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
  uint32_t    samplerate      = 0;
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
  size_t load_model_now (const std::string &filename);
  size_t load_json ( const std::string &filename);
  size_t load_nam ( const std::string &filename);
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
  using t_error_handler =
    std::function<void (const t_neuralblender_error &)>;
  
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
  size_t load_model (_lane_bank bank, size_t which, const char *filename);
  bool unload_model (_lane_bank bank, size_t which);
  void set_error_handler (t_error_handler handler);
  void throw_error (
    size_t code, size_t bank, size_t lane, const std::string &filename = "");
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
  bool set_eq_bypass (_lane_bank bank, bool bypass);
  bool set_eq_master_gain_db (_lane_bank bank, float db);
  bool set_eq_band (
      _lane_bank bank,
      size_t band,
      bool enabled,
      _eq_band_mode mode,
      int slope,
      float freq,
      float gain_db,
      float q);
  bool lane_mute (_lane_bank bank, size_t which) const;
  bool bypass () const;
  bool pedal_bypass () const;
  bool amp_bypass () const;
  bool cab_bypass () const;
  float delay_ms (_lane_bank bank, size_t which) const;
  float delay_ms (size_t which) const;
  void get_state (c_neuralblender_state &state) const;
  // Restores models and controls; call only from a non-RT thread.
  bool set_state (const c_neuralblender_state &state);
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
  c_eq eq_pre;
  c_eq eq_post;
  
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
  int calib_source = 0; // for now 0=guitar, 1=bass
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
  t_error_handler m_error_handler;
  
  bool xfade_active = false;
  uint32_t xfade_old_mask = 0;
  uint32_t xfade_new_mask = 0;
  uint32_t xfade_pos = 0;
  uint32_t xfade_len = 0;
  
  bool       m_ready = false;
  uint32_t   m_samplerate = 0;
  uint32_t   m_blocksize = 0;
};
