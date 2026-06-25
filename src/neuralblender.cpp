
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
#include "neuralblender.h"

//#define DEBUG

#ifdef CMDLINE_DEBUG
#define CMDLINE_IMPLEMENTATION // This should only be in ONE implementation file!!
#include "cmdline/cmdline.h"
#define debug(...) cmdline_debug(stderr,ANSI_BLUE,__FILE__,__LINE__,__FUNC__,__VA_ARGS__)
#else
#define debug(...)
#define CP
#define BP
#endif

//#define STANDALONE // done for us by build system (currently cmake)

extern c_neuralblender g_blender;

// a few helper functions

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

/******************************************************************************
 * c_delayline
 */

c_delayline::c_delayline () 
: m_buffer (MAX_DELAY_FRAMES, 0.0f) { CP
}

c_delayline::~c_delayline () { CP
}

bool c_delayline::set_frames (uint32_t frames) { CP
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
    
    /*if (meter_in)
      meter_in->sample (m_buffer [readpos], 0.0);*/
    out [i] = m_buffer [readpos];

    m_writepos++;
    if (m_writepos >= size)
      m_writepos = 0;
  }
  
  /*if (meter_in)
    meter_in->update ();*/
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

bool c_neuralamp::load_model (const std::string &fn) { CP
  const std::string load_fn = fn.empty() ? model_filename () : fn;

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
  reset_unlocked ();
  
  const bool ret = ends_with (load_fn, ".nam")
    ? load_nam (load_fn)
    : load_json (load_fn);

  if (ret) {
    filename = load_fn;
    m_loaded.store (true, std::memory_order_release);
  } else {
    reset_unlocked ();
    m_rtneural_model.reset ();
    m_nam_model.reset ();
    m_engine_mode = ENGINE_NONE;
    filename = "";
    m_loaded.store (false, std::memory_order_release);
  }
  
  return ret;
}

void c_neuralamp::reset_unlocked () {
  if (m_rtneural_model) m_rtneural_model->reset ();
  if (m_nam_model) m_nam_model->Reset (samplerate, MAX_BLOCK_SIZE);
  warmup = 5; CP
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
}

static void copy_with_gain (float *in, float *out, uint32_t nframes, float gain) {
  //if (1||out != in) {
    for (uint32_t i = 0; i < nframes; ++i)
      out [i] = in [i] * gain;
  //}
}

void c_neuralamp::process_block (float *in, float *out, uint32_t nframes)
{
  if (!out) return;
  
  if (mute.load (std::memory_order_relaxed) || !model_mutex.try_lock ()) {
    memset (out, 0, nframes * sizeof (float));
    return;
  }
  
  // releases when it goes out of scope
  std::lock_guard<std::mutex> lock (model_mutex, std::adopt_lock);
  
  // output gain at minimum (-40dB) -> silence
  /*if (gain_out <= 0.01) {
    memset (out, 0, nframes * sizeof (float));
    return;
  }*/
  
  if (warmup > 0) {
    debug ("skipping block, warmup=%d", warmup);
    warmup--;
    if (in != out)
      memcpy (out, in, nframes * sizeof (float));
    return;
  }

  switch (m_engine_mode) {
    case ENGINE_NONE:
      copy_with_gain (in, out, nframes, gain_in * gain_out);
      return;
    break;

    case ENGINE_JSON:
      if (!m_rtneural_model) {
        copy_with_gain (in, out, nframes, gain_in * gain_out);
        return;
      }

      for (uint32_t i = 0; i < nframes; ++i) {
        float input[1] = { in[i] * gain_in };
        float y = m_rtneural_model->forward(input);
        out[i] = std::clamp(y * gain_out, -1.0f, 1.0f);
      }
      return;
    break;
    
    case ENGINE_NAM:
      if (!m_nam_model) {
        copy_with_gain (in, out, nframes, gain_in * gain_out);
        return;
      }

      {
        NAM_SAMPLE *inputs [1]  = { in };
        NAM_SAMPLE *outputs [1] = { out };

        if (gain_in != 1.0f) {
          for (uint32_t i = 0; i < nframes; ++i)
            in[i] *= gain_in;
        }

        m_nam_model->process (inputs, outputs, (int) nframes);

        for (uint32_t i = 0; i < nframes; ++i)
          out[i] = std::clamp (out [i] * gain_out, -1.0f, 1.0f);
      }
      return;
    break;
  }
}

/******************************************************************************
 * c_neuralblender
 */

c_neuralblender::c_neuralblender () { CP
  meter_in = nullptr;

  for (size_t i = 0; i < NB_MAX_MODELS; ++i) {
    m_lane_mute [i].store (false, std::memory_order_relaxed);
    meters_out [i] = nullptr;
  }

  m_bypass.store (false, std::memory_order_relaxed);
  update_mutes ();
}

c_neuralblender::~c_neuralblender () { CP
}

void c_neuralblender::set_samplerate (uint32_t sr) { CP
  debug ("start");
  m_samplerate = sr;
  for (size_t i = 0; i < NB_MAX_MODELS; ++i)
    amps [i].set_samplerate (sr);
  debug ("end");
}

void c_neuralblender::set_blocksize (uint32_t bs) { CP
  //m_blocksize = bs;
  for (size_t i = 0; i < NB_MAX_MODELS; ++i) {
    amps [i].blocksize = bs;
    m_delay_bufs [i].resize (MAX_BLOCK_SIZE);
    m_model_bufs [i].resize (MAX_BLOCK_SIZE);
  }
}

bool c_neuralblender::set_delay_frames (size_t which, uint32_t frames) { CP
  if (which >= NB_MAX_MODELS)
    return false;

  return delays [which].set_frames (frames);
}

bool c_neuralblender::set_gain_in (size_t which, float g) {
  if (which >= NB_MAX_MODELS)
    return false;

  amps [which].gain_in = g;
  return true;
}

bool c_neuralblender::set_gain_out (size_t which, float g) {
  if (which >= NB_MAX_MODELS)
    return false;

  amps [which].gain_out = g;
  return true;
}

bool c_neuralblender::set_lane_mute (size_t which, bool muted) {
  if (which >= NB_MAX_MODELS)
    return false;

  m_lane_mute [which].store (muted, std::memory_order_relaxed);
  return true;
}

void c_neuralblender::set_bypass (bool bypass) {
  m_bypass.store (bypass, std::memory_order_relaxed);
}

bool c_neuralblender::lane_mute (size_t which) const {
  if (which >= NB_MAX_MODELS)
    return false;

  return m_lane_mute [which].load (std::memory_order_relaxed);
}

bool c_neuralblender::bypass () const {
  return m_bypass.load (std::memory_order_relaxed);
}

float c_neuralblender::delay_ms (size_t which) const {
  if (which >= NB_MAX_MODELS || !m_samplerate)
    return 0.0f;

  return (float) delays [which].frames () * 1000.0f / (float) m_samplerate;
}

void c_neuralblender::get_state (c_neuralblender_state &state) const {
  state.bypass = bypass ();

  for (size_t i = 0; i < NB_MAX_MODELS; ++i) {
    state.lanes [i].filename = amps [i].model_filename ();
    state.lanes [i].gain_in = amps [i].gain_in;
    state.lanes [i].gain_out = amps [i].gain_out;
    state.lanes [i].delay_ms = delay_ms (i);
    state.lanes [i].lane_mute = lane_mute (i);
    state.lanes [i].loaded = amps [i].loaded ();
  }
}

void c_neuralblender::update_mutes () {
  bool any_loaded = false;

  for (size_t i = 0; i < NB_MAX_MODELS; ++i) {
    const bool loaded = amps [i].loaded ();
    amps [i].mute = !loaded;
    any_loaded |= loaded;
  }

  if (!any_loaded)
    amps [0].mute = false;
}

bool c_neuralblender::load_model (size_t which, const char *fn) { CP
  if (which >= NB_MAX_MODELS) {
    debug ("which >= %d", NB_MAX_MODELS);
    return false;
  }

  for (size_t i = 0; i < NB_MAX_MODELS; ++i)
    amps [i].mute = true;

  const bool ret = amps [which].load_model (fn);
  amps [which].warmup = 5;

  int bf = 0;
  for (size_t i = 0; i < NB_MAX_MODELS; ++i) {
    if (amps [i].loaded ())
      bf |= (1 << i);
  }
  
  debug ("mute bitfield 0x%02x", bf);
  update_mutes ();
  return ret;
}

bool c_neuralblender::unload_model (size_t which) { CP
  if (which >= NB_MAX_MODELS)
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

  for (size_t lane = 0; lane < NB_MAX_MODELS; ++lane) {
    if (blender->amps [lane].loaded () && blender->meters_out [lane])
      blender->meters_out [lane]->update ();
  }
}

void c_neuralblender::process_block (float *in, float *out, uint32_t nframes) {
  if (!in || !out || !nframes)
    return;

  float *process_in = in;
  if (in == out) {
    if (m_input_buf.size () < nframes)
      m_input_buf.resize (nframes);
    memcpy (m_input_buf.data (), in, nframes * sizeof (float));
    process_in = m_input_buf.data ();
  }

  if (meter_in) {
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

  for (size_t lane = 0; lane < NB_MAX_MODELS; ++lane) {
    if (m_delay_bufs [lane].size () < nframes)
      m_delay_bufs [lane].resize (nframes);
    if (m_model_bufs [lane].size () < nframes)
      m_model_bufs [lane].resize (nframes);
  }

  bool any_loaded = false;
  bool any_active = false;
  for (size_t lane = 0; lane < NB_MAX_MODELS; ++lane) {
    if (!amps [lane].loaded ())
      continue;

    any_loaded = true;

    if (!amps [lane].mute.load (std::memory_order_relaxed) &&
        !m_lane_mute [lane].load (std::memory_order_relaxed))
      any_active = true;
  }

  if (!any_loaded) {
    delays [0].process_block (process_in, out, nframes);
    for (size_t lane = 0; lane < NB_MAX_MODELS; ++lane) {
      if (meters_out [lane])
        meters_out [lane]->update ();
    }
    return;
  }

  if (!any_active) {
    std::fill (out, out + nframes, 0.0f);
    update_loaded_output_meters (this);
    return;
  }

  std::fill (out, out + nframes, 0.0f);

  for (size_t lane = 0; lane < NB_MAX_MODELS; ++lane) {
    if (!amps [lane].loaded ())
      continue;

    if (amps [lane].mute.load (std::memory_order_relaxed) ||
        m_lane_mute [lane].load (std::memory_order_relaxed)) {
      if (meters_out [lane])
        meters_out [lane]->update ();
      continue;
    }

    delays [lane].process_block (process_in, m_delay_bufs [lane].data (), nframes);
    amps [lane].process_block (m_delay_bufs [lane].data (), m_model_bufs [lane].data (), nframes);

    for (uint32_t i = 0; i < nframes; ++i) {
      if (meters_out [lane])
        meters_out [lane]->sample (m_model_bufs [lane] [i], 0.0);
      out [i] += m_model_bufs [lane][i];
    }

    if (meters_out [lane])
      meters_out [lane]->update ();
  }

  for (uint32_t i = 0; i < nframes; ++i)
    out [i] = std::clamp (out [i], -1.0f, 1.0f);
}
