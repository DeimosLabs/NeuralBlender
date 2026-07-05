
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
 * Core class implementations
*/

#include <filesystem>
#include <cerrno>
#include <cstdlib>
#include <chrono>
#include <thread>
#include "neuralblender.h"
#include "data.h"
#include "config.h"

//#define DEBUG

#define CMDLINE_DEBUG_COLOR ANSI_BLUE
#include "cmdline_debug.h"

//#define STANDALONE // done for us by build system (currently cmake)

extern c_neuralblender g_blender;

// a few helper functions

static void fade_block_in_out (float *f, size_t nframes) {
  if (!f || nframes < 3)
    return;

  const float mid = (float) (nframes - 1) * 0.5f;

  for (size_t i = 0; i < nframes; ++i) {
    float g = fabsf ((float) i - mid) / mid;
    f [i] *= g;
  }
}

static void fade_block_in (float *f, size_t nframes) {
  if (!f || nframes < 2)
    return;

  const float denom = (float) (nframes - 1);
  for (size_t i = 0; i < nframes; ++i) {
    f [i] *= (float) i / denom;
  }
}

static void fade_block_out (float *f, size_t nframes) {
  if (!f || nframes < 2)
    return;

  const float denom = (float) (nframes - 1);
  for (size_t i = 0; i < nframes; ++i) {
    f [i] *= 1.0f - ((float) i / denom);
  }
}

static bool ends_with (const std::string &a, const std::string &b, bool casesens = false) {
  bool ret = true;
  size_t len_a = a.length ();
  size_t len_b = b.length ();

  if (len_a < len_b)
    return false;
  size_t startpos = len_a - len_b;

  for (size_t i = 0; i < len_b; i++) {
    unsigned char ca = a [startpos + i];
    unsigned char cb = b [i];
    if (!casesens) {
      ca = tolower (ca);
      cb = tolower (cb);
    }
    if (ca != cb) {
      ret = false;
      break;
    }
  }

  return ret;
}

static bool is_supported_model_path (const std::string &path) {
  return ends_with (path, ".nam") ||
         ends_with (path, ".json") ||
         ends_with (path, ".aidax");
}

static bool parse_float (const std::string &s, float &value) {
  if (s.empty ())
    return false;

  char *end = NULL;
  errno = 0;
  const float parsed = std::strtof (s.c_str (), &end);
  if (errno || end == s.c_str () || *end != '\0' || !std::isfinite (parsed))
    return false;

  value = parsed;
  return true;
}

static float clamp_gain_multiplier (float gain) {
  if (!std::isfinite (gain))
    return 1.0f;

  return std::clamp (gain, db_to_gain (GAIN_DB_MIN), db_to_gain (GAIN_DB_MAX));
}

static float clamp_calib_target_db (float db) {
  if (!std::isfinite (db))
    return DB_CALIB_TARGET_DEFAULT;

  return std::clamp (db, CALIB_TARGET_DB_MIN, CALIB_TARGET_DB_MAX);
}

/******************************************************************************
 * c_delayline
 */

c_delayline::c_delayline ()
: m_buffer (MAX_DELAY_FRAMES, 0.0f) { CP
}

c_delayline::~c_delayline () { CP
}

bool c_delayline::set_frames (uint32_t frames) {
  debug ("frames=%d", (int) frames);
  if (frames >= m_buffer.size ())
    return false;

  m_delay_frames = frames;
  return true;
}

uint32_t c_delayline::frames () const {
  return m_delay_frames;
}

void c_delayline::clear () { CP
  std::fill (m_buffer.begin (), m_buffer.end (), 0.0f);
  m_writepos = 0;
}

void c_delayline::process_block (float *in, float *out, uint32_t nframes) {
  if (!nframes) return;

  const uint32_t size = (uint32_t) m_buffer.size ();

  for (uint32_t i = 0; i < nframes; ++i) {
    m_buffer [m_writepos] = in [i];

    uint32_t readpos =
      (m_writepos + size - m_delay_frames) % size;

    out [i] = m_buffer [readpos];

    m_writepos++;
    if (m_writepos >= size)
      m_writepos = 0;
  }
}

/******************************************************************************
 * c_neuralamp
 */

c_neuralamp::c_neuralamp () { CP
}

c_neuralamp::~c_neuralamp () { CP
}

void c_neuralamp::set_samplerate (uint32_t sr) { CP
  if (samplerate == sr)
    return;

  samplerate = sr;
  reset ();
}

bool c_neuralamp::loaded () const {
  return m_loaded.load (std::memory_order_relaxed);
}

std::string c_neuralamp::model_filename () const {
  std::lock_guard<std::mutex> lock (model_mutex);
  return filename;
}

bool c_neuralamp::load_nam (const std::string &fn) { CP
  try
  {
    auto new_model =
      nam::get_dsp (std::filesystem::path (fn));

    if (!new_model)
    {
      fprintf (stderr,
              "NAM loader returned null: %s\n",
              fn.c_str ());
      return false;
    }

    new_model->Reset(
      static_cast<double> (samplerate),
      MAX_BLOCK_SIZE); /* revisit later */

    m_nam_model = std::move (new_model);

    fprintf (stderr,
            "Loaded NAM model: %s\n",
            fn.c_str ());

    m_engine_mode = ENGINE_NAM;
    return true;
  }
  catch (const std::exception &e)
  {
    fprintf (stderr,
            "NAM load failed for %s: %s\n",
            fn.c_str (),
            e.what ());

    return false;
  }
}

bool c_neuralamp::load_json (const std::string &fn) { CP
  try {
    std::ifstream file (fn);
    if (!file.is_open ()) {
      fprintf (stderr, "Could not open model file: %s\n", fn.c_str ());
      return false;
    }

    auto new_model = RTNeural::json_parser::parseJson<float>(file);
    if (!new_model) {
      fprintf (stderr, "RTNeural parser returned null: %s\n", fn.c_str ());
      return false;
    }

    new_model->reset ();
    m_nam_model.reset ();
    m_rtneural_model = std::move (new_model);
    m_engine_mode = ENGINE_JSON;

    fprintf (stderr, "Loaded model: %s\n", fn.c_str ());
    return true;
  }
  catch (const std::exception &e) {
    fprintf (stderr, "RTNeural load failed for %s: %s\n",
            fn.c_str (), e.what ());
    return false;
  }
}

bool c_neuralamp::request_load_model (const std::string &fn) { CP
  const std::string load_fn = fn.empty () ? model_filename () : fn;

  if (load_fn.empty ()) {
    fprintf (stderr, "No model filename specified\n");
    return false;
  }

  if (!is_supported_model_path (load_fn)) {
    fprintf (stderr, "Unsupported model file type: %s\n", load_fn.c_str ());
    unload_model ();
    return false;
  }

  {
    std::lock_guard<std::mutex> lock (pending_mutex);
    pending_filename = load_fn;
  }

  ramp.store (loaded () ? RAMP_START : RAMP_LOADING,
              std::memory_order_release);
  return true;
}

bool c_neuralamp::ready_to_load () {
  return ramp.load (std::memory_order_acquire) == RAMP_LOADING;
}

bool c_neuralamp::load_model () { CP
  std::string load_fn;
  {
    std::lock_guard<std::mutex> lock (pending_mutex);
    load_fn = pending_filename.empty () ? filename : pending_filename;
    pending_filename.clear ();
  }

  return load_model_now (load_fn);
}

bool c_neuralamp::load_model_now (const std::string &load_fn) { CP
  if (load_fn.empty ()) {
    fprintf (stderr, "No model filename specified\n");
    return false;
  }

  if (!is_supported_model_path (load_fn)) {
    fprintf (stderr, "Unsupported model file type: %s\n", load_fn.c_str ());
    unload_model ();
    return false;
  }

  std::lock_guard<std::mutex> lock (model_mutex);
  m_loaded.store (false, std::memory_order_release);

  const bool ret = ends_with (load_fn, ".nam")
    ? load_nam (load_fn)
    : load_json (load_fn);

  if (ret) {
    filename = load_fn;
    m_loaded.store (true, std::memory_order_release);
    reset_unlocked ();
    ramp.store (RAMP_WARMUP, std::memory_order_release);
  } else {
    reset_unlocked ();
    m_rtneural_model.reset ();
    m_nam_model.reset ();
    m_engine_mode = ENGINE_NONE;
    filename = "";
    m_loaded.store (false, std::memory_order_release);
    ramp.store (RAMP_END, std::memory_order_release);
  }

  return ret;
}

float c_neuralamp::get_block_rms (float *data, size_t nframes) {
  if (!data || nframes == 0)
    return 0.0f;

  double sum = 0.0;

  switch (m_engine_mode) {
    case ENGINE_NONE:
      return 0.0f;
    break;

    case ENGINE_JSON:
      if (!m_rtneural_model) {
        return 1.0f;
      }

      for (uint32_t i = 0; i < nframes; ++i) {
        float input [1] = { data [i] };
        float y = m_rtneural_model->forward (input);
        sum += (double) y * (double) y;
      }
      return (float) sqrt (sum / (double) nframes);
    break;

    case ENGINE_NAM:
      if (!m_nam_model) {
        return 0.0f;
      }

      {
        std::vector<float> buf (nframes);
        NAM_SAMPLE *inputs [1]  = { data };
        NAM_SAMPLE *outputs [1] = { buf.data () };

        m_nam_model->process (inputs, outputs, (int) nframes);

        for (uint32_t i = 0; i < nframes; ++i) {
          const float y = buf [i];
          sum += (double) y * (double) y;
        }
      }
      return (float) sqrt (sum / (double) nframes);
    break;
    
    default:
      return 0.0f;
  }
}

float c_neuralamp::calibrate (float *data, size_t size) {
  debug ("data=0x%lx, size=%d", (long int) data, (int) size);
  std::lock_guard<std::mutex> lock (model_mutex);
  float ret = 0.0f;
  
  calib_target_db = clamp_calib_target_db (calib_target_db);

  size_t i;
  size_t blocksize = 128;
  
  reset_unlocked ();
  
  if (data) {
  
    for (i = 0; i + blocksize <= size; i += blocksize) {
      if (i > 3) // carla sends a few bogus blocks on plugin load
        ret = std::max (ret, get_block_rms (&data[i], blocksize));
      //debug ("max %f", ret);
    }

    const size_t left = size - i;
    if (left > 0) {
      ret = std::max (ret, get_block_rms (&data[i], left));
      //debug ("max %f", ret);
    }
  }
  
  const float target = db_to_gain (calib_target_db);
  if (ret > db_to_gain (DB_SILENCE) && std::isfinite (ret))
    trim = target / ret;
  else
    trim = 1.0f;
  
  debug ("rms=%f", ret);
  debug ("trim=%f", (float) trim);
  
  reset_unlocked ();
  return ret;
}

void c_neuralamp::reset_unlocked () { CP
  debug ("RESET, block %ld", (long int) block_counter);
  if (m_rtneural_model) m_rtneural_model->reset ();
  if (m_nam_model) m_nam_model->Reset (samplerate, MAX_BLOCK_SIZE);
  warmup = WARMUP_BLOCKS;
}

void c_neuralamp::reset () {
  std::lock_guard<std::mutex> lock (model_mutex);
  reset_unlocked ();
}

void c_neuralamp::unload_model () {
  std::lock_guard<std::mutex> lock (model_mutex);
  m_loaded.store (false, std::memory_order_release);
  reset_unlocked ();
  m_rtneural_model.reset ();
  m_nam_model.reset ();
  m_engine_mode = ENGINE_NONE;
  filename = "";
  trim = 1.0f;
}

static void copy_with_gain (float *in, float *out, uint32_t nframes, float gain) {
  //if (1||out != in) {
    for (uint32_t i = 0; i < nframes; ++i)
      out [i] = in [i] * gain;
  //}
}

void c_neuralamp::process_block (float *in, float *out, uint32_t nframes) {
  block_counter++;
  if (!out) return;
  
  const float input_gain = clamp_gain_multiplier (gain_in);
  const float output_gain = clamp_gain_multiplier (gain_out);
  
  if (mute.load (std::memory_order_relaxed) || !model_mutex.try_lock ()) {
    memset (out, 0, nframes * sizeof (float));
    return;
  }

  // releases when it goes out of scope
  std::lock_guard<std::mutex> lock (model_mutex, std::adopt_lock);
  
  switch (m_engine_mode) {
    case ENGINE_NONE:
      copy_with_gain (in, out, nframes, input_gain * output_gain);
      break;

    case ENGINE_JSON:
      if (!m_rtneural_model) {
        copy_with_gain (in, out, nframes, input_gain * output_gain);
        break;
      }

      {
        const float out_gain = output_gain * trim;
        for (uint32_t i = 0; i < nframes; ++i) {
          float input [1] = { in [i] * input_gain };
          float y = m_rtneural_model->forward (input);
          if (dcflip)
            out[i] = -1.0f * std::clamp (y * out_gain, -1.0f, 1.0f);
          else
            out[i] = std::clamp (y * out_gain, -1.0f, 1.0f);
        }
      }
      //return;
    break;

    case ENGINE_NAM:
      if (!m_nam_model) {
        copy_with_gain (in, out, nframes, input_gain * output_gain);
        break;
      }

      {
        NAM_SAMPLE *inputs [1]  = { in };
        NAM_SAMPLE *outputs [1] = { out };

        if (input_gain != 1.0f) {
          for (uint32_t i = 0; i < nframes; ++i)
            in[i] *= input_gain;
        }

        m_nam_model->process (inputs, outputs, (int) nframes);

        const float out_gain = output_gain * trim;
        for (uint32_t i = 0; i < nframes; ++i)
          if (dcflip)
            out [i] = -1.0f * std::clamp (out [i] * out_gain, -1.0f, 1.0f);
          else
            out [i] = std::clamp (out [i] * out_gain, -1.0f, 1.0f);
      }
      //return;
    break;
  }

  switch (ramp.load (std::memory_order_acquire)) {
    case RAMP_START:
      debug ("%d (%ld) RAMP_START block %ld", (int) which, (long int) this, (long int) block_counter);
      fade_block_out(out, nframes);
      ramp.store (RAMP_LOADING, std::memory_order_release);
      break;

    case RAMP_LOADING:
      debug ("%d (%ld) RAMP_LOADING block %ld", (int) which, (long int) this, (long int) block_counter);
      memset(out, 0, nframes * sizeof(float));
      break;

    case RAMP_WARMUP:
      debug ("%d (%ld) RAMP_WARMUP block %ld", (int) which, (long int) this, (long int) block_counter);
      memset(out, 0, nframes * sizeof(float));
      if (warmup > 0)
        warmup--;
      if (warmup == 0)
        ramp.store (RAMP_END, std::memory_order_release);
      break;

    case RAMP_END:
      debug ("%d (%ld) RAMP_END block %ld", (int) which, (long int) this, (long int) block_counter);
      fade_block_in(out, nframes);
      ramp.store (RAMP_PLAYING, std::memory_order_release);
      warmup = -1;
      break;

    case RAMP_PLAYING:
    default:
      //debug ("%d (%ld) RAMP_PLAYING block %ld", (int) which, (long int) this, (long int) block_counter);
      break;
  }
}

/******************************************************************************
 * c_neuralblender
 */

c_neuralblender::c_neuralblender () { CP
  meter_in = nullptr;

  for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
    amps [i].which = i;
    m_lane_mute [i].store (false, std::memory_order_relaxed);
    meters_out [i] = nullptr;
    amps [i].dcflip = false;
  }

  c_configfile configfile;
  if (configfile.read_file ()) {
    float calib_target_db = DB_CALIB_TARGET_DEFAULT;
    if (parse_float (
        configfile.get_item (CONFIG_KEY_NAME_CALIB_TARGET),
        calib_target_db))
      set_calib_target_db (calib_target_db);
  }

  m_bypass.store (false, std::memory_order_relaxed);
  update_mutes ();

}

c_neuralblender::~c_neuralblender () { CP
}

/*static*/ void c_neuralblender::get_calib_data (std::vector<float> &v, bool bass) {
  debug ("bass=%d", (int) bass);
  unsigned char *scan = data_calib_f32;
  const int sf = sizeof (float);
  size_t len = data_calib_f32_len;
  
  if (bass) {
    scan = data_calib_bass_f32;
    len = data_calib_bass_f32_len;
  }
  
  for (size_t i = 0; i < len / sf; i++) {
    float *f = (float *) scan;
    scan += sizeof (float);
    v.push_back (*f);
  }
}

bool c_neuralblender::calibrate (size_t which, bool bass) {
  debug ("bass=%d", (int) bass);
  if (which >= NB_NUM_MODELS)
    return false;
    
  float *data = (float *) data_calib_f32;
  size_t samples = data_calib_f32_len / sizeof (float);
  
  if (bass) {
    data = (float *) data_calib_bass_f32;
    samples = data_calib_bass_f32_len / sizeof (float);
  }
  
  if (amps [which].do_calib && amps [which].loaded ()) {
    amps [which].calibrate (data, samples);
  } else {
    amps [which].calibrate (NULL, 0);
  }

  return true;
}

bool c_neuralblender::calibrate_linked (bool bass) {
  debug ("bass=%d", (int) bass);
  float linked_trim = INFINITY;
  bool any_linked = false;
  float *data = (float *) data_calib_f32;
  size_t samples = data_calib_f32_len / sizeof (float);

  if (bass) {
    data = (float *) data_calib_bass_f32;
    samples = data_calib_bass_f32_len / sizeof (float);
  }

  for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
    if (amps [i].do_calib && amps [i].loaded ()) {
      amps [i].calibrate (data, samples);
      const float trim = amps [i].trim.load (std::memory_order_acquire);
      if (std::isfinite (trim) && trim > 0.0f) {
        linked_trim = std::min (linked_trim, trim);
        any_linked = true;
      }
    } else {
      amps [i].calibrate (NULL, 0);
    }
  }

  if (!any_linked)
    return true;

  for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
    if (amps [i].do_calib && amps [i].loaded ())
      amps [i].trim.store (linked_trim, std::memory_order_release);
  }

  return true;
}

void c_neuralblender::set_samplerate (uint32_t sr) { CP
  debug ("start");
  m_samplerate = sr;
  if (meter_in)
    meter_in->samplerate = (int) sr;
  for (size_t i = 0; i < NB_NUM_MODELS; ++i)
    amps [i].set_samplerate (sr);
  for (size_t i = 0; i < NB_NUM_MODELS; ++i)
    if (meters_out [i])
      meters_out [i]->samplerate = (int) sr;
  debug ("end");
}

void c_neuralblender::set_blocksize (uint32_t bs) { CP
  //m_blocksize = bs;
  if (meter_in)
    meter_in->bufsize = (int) bs;
  for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
    amps [i].blocksize = bs;
    if (meters_out [i])
      meters_out [i]->bufsize = (int) bs;
    m_delay_bufs [i].resize (MAX_BLOCK_SIZE);
    m_model_bufs [i].resize (MAX_BLOCK_SIZE);
  }
}

bool c_neuralblender::dcflip (size_t which, bool b) {
  if (which >= NB_NUM_MODELS)
    return false;
  amps [which].dcflip = b;
  return true;
}

bool c_neuralblender::is_dcflipped (size_t which) {
  if (which >= NB_NUM_MODELS) return false;
  return amps [which].dcflip;
}

bool c_neuralblender::calib_on (size_t which, bool b) {
  if (which >= NB_NUM_MODELS)
    return false;
  amps [which].do_calib = b;
  return true;
}

bool c_neuralblender::is_calib_on (size_t which) {
  if (which >= NB_NUM_MODELS) return false;
  return amps [which].do_calib;
}

bool c_neuralblender::set_delay_frames (size_t which, uint32_t frames) { CP
  if (which >= NB_NUM_MODELS)
    return false;
  
  return delays [which].set_frames (frames);
}

bool c_neuralblender::set_gain_in (size_t which, float g) {
  if (which >= NB_NUM_MODELS)
    return false;

  amps [which].gain_in = clamp_gain_multiplier (g);
  return true;
}

bool c_neuralblender::set_gain_out (size_t which, float g) {
  if (which >= NB_NUM_MODELS)
    return false;

  amps [which].gain_out = clamp_gain_multiplier (g);
  return true;
}

bool c_neuralblender::set_lane_mute (size_t which, bool muted) {
  if (which >= NB_NUM_MODELS)
    return false;

  m_lane_mute [which].store (muted, std::memory_order_relaxed);
  request_mix_update ();
  return true;
}

void c_neuralblender::set_bypass (bool bypass) {
  m_bypass.store (bypass, std::memory_order_relaxed);
  request_mix_update ();
}

bool c_neuralblender::lane_mute (size_t which) const {
  if (which >= NB_NUM_MODELS)
    return false;

  return m_lane_mute [which].load (std::memory_order_relaxed);
}

bool c_neuralblender::bypass () const {
  return m_bypass.load (std::memory_order_relaxed);
}

float c_neuralblender::delay_ms (size_t which) const {
  if (which >= NB_NUM_MODELS || !m_samplerate)
    return 0.0f;

  return (float) delays [which].frames () * 1000.0f / (float) m_samplerate;
}

// returns whether all lanes are same state, if so sets
// enabled to that state
bool c_neuralblender::consistent_calib_state (bool &enabled,
    c_neuralblender_state &state) const {

  get_state (state);

  bool all_on = true;
  bool all_off = true;

  for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
    all_on  &= state.lanes[i].do_calib;
    all_off &= !state.lanes[i].do_calib;
  }

  if (all_on) {
    enabled = true;
    return true;
  }

  if (all_off) {
    enabled = false;
    return true;
  }

  return false;
}

void c_neuralblender::get_state (c_neuralblender_state &state) const {
  state.bypass = bypass ();
  state.do_vu = do_vu;
  state.mute_all = mute_all;
  for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
    state.lanes [i].filename = amps [i].model_filename ();
    state.lanes [i].gain_in = amps [i].gain_in;
    state.lanes [i].gain_out = amps [i].gain_out;
    state.lanes [i].delay_ms = delay_ms (i);
    state.lanes [i].lane_mute = lane_mute (i);
    state.lanes [i].loaded = amps [i].loaded ();
    state.lanes [i].dcflip = amps [i].dcflip;
    state.lanes [i].do_calib = amps [i].do_calib;
  }
  
  /*bool calib_enabled = false;
  if (consistent_calib_state (calib_enabled, state)) {
    configfile.set_item (CONFIG_KEY_NAME_CALIB, calib_enabled ? "1" : "0");
    configfile.write_file ();
  }*/
}

void c_neuralblender::update_mutes () {
  bool any_loaded = false;

  for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
    const bool loaded = amps [i].loaded ();
    amps [i].mute = !loaded;
    any_loaded |= loaded;
  }

  if (!any_loaded)
    amps [0].mute = false;

  request_mix_update ();
}

bool c_neuralblender::set_calib_target_db (float f) { CP
  bool ret = true;
  
  if (!std::isfinite (f))
    return false;
    
  f = clamp_calib_target_db (f);
  
  for (int i = 0; i < NB_NUM_MODELS; i++) {
    amps [i].calib_target_db = f;
    // TODO: should we recalibrate models that are loaded AND have calib enabled?
  }
  
  return ret;
}

bool c_neuralblender::load_model (size_t which, const char *fn) { CP
  if (which >= NB_NUM_MODELS) {
    debug ("which >= %d", NB_NUM_MODELS);
    return false;
  }

  debug ("LOAD MODEL, block %ld", (long int) amps [which].block_counter);

  const bool requested = amps [which].request_load_model (fn);
  if (!requested) {
    update_mutes ();
    return false;
  }

  int wait_ms = 0;
  while (!amps [which].ready_to_load () && wait_ms < 250) {
    std::this_thread::sleep_for (std::chrono::milliseconds (1));
    wait_ms++;
  }

  if (!amps [which].ready_to_load ())
    amps [which].ramp.store (RAMP_LOADING, std::memory_order_release);

  const bool ret = amps [which].load_model ();

  int bf = 0;
  for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
    if (amps [i].loaded ())
      bf |= (1 << i);
  }

  debug ("mute bitfield 0x%02x", bf);
  update_mutes ();
  return ret;
}

bool c_neuralblender::unload_model (size_t which) { CP
  if (which >= NB_NUM_MODELS)
    return false;

  amps [which].unload_model ();
  if (meters_out [which])
    meters_out [which]->update ();
  update_mutes ();
  return true;
}

bool c_neuralblender::set_delay_ms (size_t which, float ms) {
  const uint32_t frames =
    (uint32_t) ((ms * m_samplerate) / 1000.0f + 0.5f); // round to nearest int

  return set_delay_frames (which, frames);
}

static void update_loaded_output_meters (c_neuralblender *blender) {
  if (!blender)
    return;

  if (!blender->do_vu)
    return;

  for (size_t lane = 0; lane < NB_NUM_MODELS; ++lane) {
    if (blender->amps [lane].loaded () && blender->meters_out [lane])
      blender->meters_out [lane]->update ();
  }
}

/*
void c_neuralblender::process_block_main (float *in, float *out, uint32_t nframes) {
  if (!in || !out || !nframes)
    return;

  float *process_in = in;
  if (in == out) {
    if (m_input_buf.size () < nframes)
      m_input_buf.resize (nframes);
    memcpy (m_input_buf.data (), in, nframes * sizeof (float));
    process_in = m_input_buf.data ();
  }

  if (do_vu && meter_in) {
    for (uint32_t i = 0; i < nframes; i++)
      meter_in->sample (process_in [i], 0.0f);
    meter_in->update ();
  }

  if (m_bypass.load (std::memory_order_relaxed)) {
    if (process_in != out)
      memcpy (out, process_in, nframes * sizeof (float));
    update_loaded_output_meters (this);
    return;
  }

  if (mute_all) {
    std::fill(out, out + nframes, 0.0f);
    if (do_vu)
      update_loaded_output_meters(this);
    return;
  }

  for (size_t lane = 0; lane < NB_NUM_MODELS; ++lane) {
    if (m_delay_bufs [lane].size () < nframes)
      m_delay_bufs [lane].resize (nframes);
    if (m_model_bufs [lane].size () < nframes)
      m_model_bufs [lane].resize (nframes);
  }

  bool any_loaded = false;
  bool any_active = false;
  for (size_t lane = 0; lane < NB_NUM_MODELS; ++lane) {
    if (!amps [lane].loaded ())
      continue;

    any_loaded = true;

    if (!amps [lane].mute.load (std::memory_order_relaxed) &&
        !m_lane_mute [lane].load (std::memory_order_relaxed))
      any_active = true;
  }

  if (!any_loaded) {
    delays [0].process_block (process_in, out, nframes);
    for (size_t lane = 0; lane < NB_NUM_MODELS; ++lane) {
      if (do_vu && meters_out [lane])
        meters_out [lane]->update ();
    }
    return;
  }

  if (!any_active) {
    std::fill (out, out + nframes, 0.0f);
    if (do_vu)
      update_loaded_output_meters (this);
    return;
  }

  std::fill (out, out + nframes, 0.0f);

  for (size_t lane = 0; lane < NB_NUM_MODELS; ++lane) {
    if (!amps [lane].loaded ()) {
      if (meters_out [lane] && do_vu)
        meters_out [lane]->update ();
      continue;
    }

    if (amps [lane].mute.load (std::memory_order_relaxed) ||
        m_lane_mute [lane].load (std::memory_order_relaxed)) {
      if (meters_out [lane] && do_vu)
        meters_out [lane]->update ();
      continue;
    }

    delays [lane].process_block (process_in, m_delay_bufs [lane].data (), nframes);
    amps [lane].process_block (m_delay_bufs [lane].data (), m_model_bufs [lane].data (), nframes);

    for (uint32_t i = 0; i < nframes; ++i) {
      if (meters_out [lane] && do_vu)
        meters_out [lane]->sample (m_model_bufs [lane] [i], 0.0);
      out [i] += m_model_bufs [lane][i];
    }

    if (meters_out [lane] && do_vu)
      meters_out [lane]->update ();
  }

  for (uint32_t i = 0; i < nframes; ++i)
    out [i] = std::clamp (out [i], -1.0f, 1.0f);
}

void c_neuralblender::process_block (float *in, float *out, uint32_t nframes) {
  process_block_main (in, out, nframes);
}
*/

static inline float ramp_in_gain (uint32_t i, uint32_t n) {
  return n > 1 ? (float)i / (float) (n - 1) : 1.0f;
}

static inline float ramp_out_gain (uint32_t i, uint32_t n) {
  return n > 1 ? 1.0f - ((float)i / (float) (n - 1)) : 0.0f;
}

static void add_lane (float *dst, const float *src, uint32_t n) {
  for (uint32_t i = 0; i < n; ++i)
    dst[i] += src[i];
}

static void add_lane_fade_in (float *dst, const float *src, uint32_t n) {
  const float denom = n > 1 ? (float) (n - 1) : 1.0f;
  for (uint32_t i = 0; i < n; ++i)
    dst [i] += src [i] * ((float) i / denom);
}

static void add_lane_fade_out (float *dst, const float *src, uint32_t n) {
  const float denom = n > 1 ? (float) (n - 1) : 1.0f;
  for (uint32_t i = 0; i < n; ++i)
    dst [i] += src [i] * (1.0f - ((float) i / denom));
}

static void final_clamp (float *out, uint32_t n) {
  if (!out)
    return;

  for (uint32_t i = 0; i < n; ++i)
    out [i] = std::clamp (out [i], -1.0f, 1.0f);
}

static bool any_lane_loaded (const c_neuralblender *blender) {
  if (!blender)
    return false;

  for (size_t lane = 0; lane < NB_NUM_MODELS; ++lane)
    if (blender->amps [lane].loaded ())
      return true;

  return false;
}

uint32_t c_neuralblender::make_active_lane_mask() const {
  if (m_bypass.load (std::memory_order_relaxed))
    return 0;

  if (mute_all)
    return 0;

  uint32_t mask = 0;

  for (size_t lane = 0; lane < NB_NUM_MODELS; ++lane) {
    if (!amps [lane].loaded ())
      continue;

    if (amps [lane].mute.load () || m_lane_mute[lane].load ())
      continue;

    mask |= 1u << lane;
  }

  return mask;
}

void c_neuralblender::request_mix_update () {
  pending_lane_mask.store (make_active_lane_mask (),
                           std::memory_order_release);
  transition_pending.store (true, std::memory_order_release);
}

float *c_neuralblender::prepare_input_buffer (
    float *in, float *out, uint32_t nframes) {

  if (in != out)
    return in;

  if (m_input_buf.size () < nframes)
    m_input_buf.resize (nframes);

  memcpy (m_input_buf.data (), in, nframes * sizeof (float));
  return m_input_buf.data ();
}

void c_neuralblender::update_input_meter (float *in, uint32_t nframes) {
  if (!do_vu || !meter_in || !in)
    return;

  for (uint32_t i = 0; i < nframes; i++)
    meter_in->sample (in [i], 0.0f);
  meter_in->update ();
}

void c_neuralblender::render_lane (
    size_t lane, float *in, uint32_t nframes) {

  if (lane >= NB_NUM_MODELS)
    return;

  if (m_delay_bufs [lane].size () < nframes)
    m_delay_bufs [lane].resize (nframes);

  if (m_model_bufs [lane].size () < nframes)
    m_model_bufs [lane].resize (nframes);

  delays [lane].process_block (in, m_delay_bufs [lane].data (), nframes);
  amps [lane].process_block (m_delay_bufs [lane].data (),
                             m_model_bufs [lane].data (),
                             nframes);

  if (meters_out [lane] && do_vu) {
    for (uint32_t i = 0; i < nframes; ++i)
      meters_out [lane]->sample (m_model_bufs [lane] [i], 0.0f);
    meters_out [lane]->update ();
  }
}

void c_neuralblender::render_mix (float *in,
                                  float *out,
                                  uint32_t nframes,
                                  uint32_t old_mask,
                                  uint32_t new_mask) {
  const uint32_t relevant = old_mask | new_mask;

  for (size_t lane = 0; lane < NB_NUM_MODELS; ++lane) {
    const uint32_t bit = 1u << lane;

    if (!(relevant & bit)) {
      if (meters_out [lane] && do_vu)
        meters_out [lane]->update ();

      continue;
    }

    render_lane (lane, in, nframes);

    const bool was = old_mask & bit;
    const bool now = new_mask & bit;

    if (was && now)
      add_lane (out, m_model_bufs [lane].data (), nframes);
    else if (was && !now)
      add_lane_fade_out (out, m_model_bufs[lane].data (), nframes);
    else if (!was && now)
      add_lane_fade_in (out, m_model_bufs[lane].data (), nframes);
  }
}

void c_neuralblender::process_block (float *in, float *out, uint32_t nframes) {
  if (!in || !out || !nframes)
    return;

  float *process_in = prepare_input_buffer (in, out, nframes);

  update_input_meter (process_in, nframes);

  const uint32_t old_mask =
    active_lane_mask.load (std::memory_order_acquire);

  uint32_t new_mask =
    pending_lane_mask.load (std::memory_order_acquire);

  bool do_transition =
    transition_pending.exchange (false, std::memory_order_acq_rel);

  if (!do_transition) {
    new_mask = make_active_lane_mask ();
    do_transition = new_mask != old_mask;
  }

  std::fill (out, out + nframes, 0.0f);

  if (m_bypass.load (std::memory_order_relaxed)) {
    if (process_in != out)
      memcpy (out, process_in, nframes * sizeof (float));
    update_loaded_output_meters (this);
    return;
  }

  if (!any_lane_loaded (this)) {
    delays [0].process_block (process_in, out, nframes);
    for (size_t lane = 0; lane < NB_NUM_MODELS; ++lane) {
      if (do_vu && meters_out [lane])
        meters_out [lane]->update ();
    }
    return;
  }

  if (!do_transition) {
    render_mix (process_in, out, nframes, old_mask, old_mask);
  } else {
    render_mix (process_in, out, nframes, old_mask, new_mask);
    active_lane_mask.store (new_mask, std::memory_order_release);
  }

  final_clamp (out, nframes);
}
