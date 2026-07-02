
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

#include "meter.h"

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#define NB_NUM_MODELS            4

#define MAX_DELAY_MS             30
#define MAX_DELAY_FRAMES         (MAX_DELAY_MS * 192)
#define MAX_BLOCK_SIZE           8192
#define DB_SILENCE               -120.0f
#define DB_CALIB_TARGET_DEFAULT  -12.0f
#define GAIN_DB_MIN              -40.0f
#define GAIN_DB_MAX              40.0f
#define CALIB_TARGET_DB_MIN      -40.0f
#define CALIB_TARGET_DB_MAX      0.0f

enum _engine_mode {
  ENGINE_NONE,
  ENGINE_NAM,
  ENGINE_JSON
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
  float gain_out = 1.0f;
  float delay_ms = 0.0f;
  bool lane_mute = false;
  bool loaded = false;
  bool dcflip = false;
  bool do_calib = false;
};

struct c_neuralblender_state {
  std::string current_dir;
  bool bypass = false;
  bool do_excl = false;
  bool do_vu = true;
  bool showadvanced = false;
  bool mute_all = false;
  int  exclusive_lane = 0;
  c_neuralblender_lane_state lanes [NB_NUM_MODELS];
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

class c_neuralamp {
public:
  c_neuralamp ();
  ~c_neuralamp ();
  void set_samplerate (uint32_t sr);
  void set_blocksize (uint32_t bs);

  bool load_model (const std::string &filename = "");
  void unload_model ();
  void reset ();
  float calibrate (float *data, size_t sz);

  //float process_sample (float x);
  void process_block (float *in, float *out, uint32_t nframes);

  bool loaded () const;
  std::string model_filename () const;
  std::atomic<float> trim { 1.0f };

  std::string filename        = "";
  float       gain_in         = 1.0f;
  float       gain_out        = 1.0f;
  float       calib_target_db = DB_CALIB_TARGET_DEFAULT;
  uint32_t    samplerate      = 48000;
  uint32_t    blocksize       = -1;
  std::atomic<bool> mute      { false };
  int         warmup          = 5;
  bool        dcflip          = false;
  bool        do_calib        = false;

private:
  void reset_unlocked ();
  bool load_json ( const std::string &filename);
  bool load_nam ( const std::string &filename);
  float get_block_rms (float *data, size_t sz);
  // model impl.
  std::unique_ptr<nam::DSP> m_nam_model;
  std::unique_ptr<RTNeural::Model<float>> m_rtneural_model;
  mutable std::mutex model_mutex;
  std::atomic<bool> m_loaded { false };

  _engine_mode m_engine_mode = ENGINE_NONE;
};

class c_neuralblender {
public:
  c_neuralblender ();
  ~c_neuralblender ();
  void set_samplerate (uint32_t sr);
  void set_blocksize (uint32_t bs);
  void process_block (float *in, float *out, uint32_t count);
  bool load_model (size_t which, const char *filename);
  bool unload_model (size_t which);
  bool set_delay_frames (size_t which, uint32_t frames);
  bool set_delay_ms (size_t which, float ms);
  bool set_gain_in (size_t which, float g);
  bool set_gain_out (size_t which, float g);
  bool set_lane_mute (size_t which, bool muted);
  void set_bypass (bool bypass);
  bool lane_mute (size_t which) const;
  bool bypass () const;
  float delay_ms (size_t which) const;
  void get_state (c_neuralblender_state &state) const;
  bool dcflip (size_t which, bool b);
  bool calib_on (size_t which, bool b);
  bool is_dcflipped (size_t which);
  bool is_calib_on (size_t which);
  bool set_calib_target_db (float f);
  bool calibrate (size_t which, bool bass);
  bool calibrate_linked (bool bass);
  
  static void get_calib_data (std::vector<float> &v, bool bass);

  c_delayline delays [NB_NUM_MODELS];
  c_neuralamp amps [NB_NUM_MODELS];
  c_vudata *meter_in;
  c_vudata *meters_out [NB_NUM_MODELS];
  bool do_vu = true;
  bool mute_all = false;
  bool linked_calib = false;
  bool calib_bass = false;

private:
  void update_mutes ();
  bool consistent_calib_state (bool &enabled,
      c_neuralblender_state &state) const;
  std::vector <float> m_delay_bufs [NB_NUM_MODELS];
  std::vector <float> m_model_bufs [NB_NUM_MODELS];
  std::vector <float> m_input_buf;
  std::atomic<bool> m_lane_mute [NB_NUM_MODELS];
  std::atomic<bool> m_bypass { false };

  bool       m_ready = false;
  uint32_t   m_samplerate = 48000;
  //uint32_t   m_blocksize = 256;
};
