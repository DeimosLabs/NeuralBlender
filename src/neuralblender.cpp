
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
#include <iostream>
#include "neuralblender.h"
#include "data.h"
#include "configfile.h"

#ifdef HAVE_SNDFILE
#include <sndfile.h>
#endif

#ifdef HAVE_SAMPLERATE
#include <samplerate.h>
#endif

//#define DEBUG


#define CMDLINE_DEBUG_COLOR ANSI_BLUE
#include "cmdline_debug.h"

//#define STANDALONE // done for us by build system (currently cmake)

// a few helper functions

// not using this one, let's keep it for now
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

static uint32_t xfade_samples_for_rate (uint32_t samplerate) {
  uint32_t ret = (uint32_t)
    (((float) samplerate * NB_XFADE_MS) / 1000.0f + 0.5f);
  if (ret < 1)
    ret = 1;
  return ret;
}

static inline float xfade_in_gain (uint32_t pos, uint32_t len) {
  if (len < 1)
    return 1.0f;
  if (pos >= len)
    return 1.0f;
  return (float) pos / (float) len;
}

static inline float xfade_out_gain (uint32_t pos, uint32_t len) {
  return 1.0f - xfade_in_gain (pos, len);
}

static void fade_block_in (float *f, uint32_t nframes,
                           uint32_t pos, uint32_t len) {
  if (!f)
    return;

  for (uint32_t i = 0; i < nframes; ++i)
    f [i] *= xfade_in_gain (pos + i, len);
}

static void fade_block_out (float *f, uint32_t nframes,
                            uint32_t pos, uint32_t len) {
  if (!f)
    return;

  for (uint32_t i = 0; i < nframes; ++i)
    f [i] *= xfade_out_gain (pos + i, len);
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
         ends_with (path, ".aidax") ||
         ends_with (path, ".wav");
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

static float clamp_dry_multiplier (float gain) {
  if (!std::isfinite (gain))
    return 0.0f;

  return std::clamp (gain, 0.0f, db_to_gain (12.0f));
}

static float clamp_calib_target_db (float db) {
  if (!std::isfinite (db))
    return DB_CALIB_TARGET_DEFAULT;

  return std::clamp (db, CALIB_TARGET_DB_MIN, CALIB_TARGET_DB_MAX);
}

////////////////////////////////////////////////////////////////////////////////
// c_noisegate
// TODO: add UI settings for attack, hold, delay

// should be called on startup, and when user changes a setting
void c_noisegate::update_coeffs () {
  threshold_gain = db_to_gain (threshold_db);
  attack_coeff = (attack_ms <= 0.0f) ? 0.0f : 
                 expf (-1.0f / (0.001f * attack_ms * samplerate));
  release_coeff = (release_ms <= 0.0f) ? 0.0f :
                  expf (-1.0f / (0.001f * release_ms * samplerate));
  hold_coeff = (int) (hold_ms * 0.001f * samplerate);
  coeffs_dirty = false;
}

float c_noisegate::get_current_gain () {
  return display_gain.load (std::memory_order_relaxed);
}

float c_noisegate::get_current_db () {
  return gain_to_db (display_gain.load (std::memory_order_relaxed));
}

void c_noisegate::set_samplerate (int new_sr) {
  if (new_sr > 0 && new_sr <= 192000) {
    samplerate = new_sr;
    coeffs_dirty = true;
  }
}

void c_noisegate::set_threshold (float new_thresh_db) {
  threshold_db = std::clamp (
    new_thresh_db,
    NOISEGATE_THRESH_MIN,
    NOISEGATE_THRESH_MAX);
  coeffs_dirty = true;
}

void c_noisegate::set_attack (float new_attack_ms) {
  if (new_attack_ms < 0)
    return;
  attack_ms = new_attack_ms;
  coeffs_dirty = true;
}

void c_noisegate::set_hold (float new_hold_ms) {
  if (new_hold_ms < 0)
    return;
  hold_ms = new_hold_ms;
  coeffs_dirty = true;
}

void c_noisegate::set_release (float new_release_ms) {
  if (new_release_ms < 0)
    return;
  release_ms = new_release_ms;
  coeffs_dirty = true;
}

// will process in-place if out == in or !out
void c_noisegate::process_block (float *in, float *out, uint32_t nframes) {
  if (!in || nframes == 0)
    return;
  
  if (coeffs_dirty)
    update_coeffs ();
  
  //const int hold_total = (int) (hold_ms * 0.001f * samplerate);
  float *dst = out ? out : in;
  
  for (uint32_t i = 0; i < nframes; ++i) {
    float x = fabsf (in [i]);

    float coeff = (x > env) ? attack_coeff : release_coeff;
    env = coeff * env + (1.0f - coeff) * x;
    
    float target = 0.0f;
    
    if (env >= threshold_gain) {
      target = 1.0f;
      hold_samples = hold_coeff;
    } else if (hold_samples > 0) {
      target = 1.0f;
      hold_samples--;
    }
    
    float gcoeff = (target > gain) ? attack_coeff : release_coeff;
    gain = gcoeff * gain + (1.0f - gcoeff) * target;
    
    dst [i] = in [i] * gain;
  }
  display_gain.store (gain, std::memory_order_relaxed);
}


////////////////////////////////////////////////////////////////////////////////
// c_delayline

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

////////////////////////////////////////////////////////////////////////////////
// c_convolver

#ifdef HAVE_FFTW

static bool read_wav (const char *filename, std::vector<float> &v, int channel, int *sr) {
  debug ("start");
  
  v.clear ();
  
  SF_INFO info {};
  
  SNDFILE *f = sf_open (filename, SFM_READ, &info);
  if (!f) {
    std::cerr << "Error: failed to open WAV file: " << filename << "\n";
    return false;
  }
  
  if (sr) *sr = info.samplerate;
  int chans = info.channels;
  sf_count_t frames = info.frames;

  if (frames <= 0 || chans <= 0) {
    std::cerr << "Error: invalid WAV format in " << filename << "\n";
    sf_close (f);
    return false;
  }
  
  std::vector<float> buf (frames * chans);
  sf_count_t readframes = sf_readf_float (f, buf.data (), frames);
  sf_close (f);

  if (readframes <= 0) {
    std::cerr << "Error: no samples read from " << filename << "\n";
    return false;
  }
  
  if (channel < 0 || channel >= chans) {
    std::cerr << "Error: " << filename << " does not have channel " << channel << "\n";
    return false;
  }
  
  v.resize ((size_t) readframes);

  // read every 'channel' sample out of 'chans' into v
  for (sf_count_t i = 0; i < readframes; i++) {
    v [i] = buf [chans * i + channel];
  }
  
  return true;
}

c_convolver::c_convolver () { CP }
c_convolver::~c_convolver () { CP
  clear ();
}

static uint32_t ceil_div_u32 (size_t a, uint32_t b) {
  if (!b)
    return 0;

  return (uint32_t) ((a + (size_t) b - 1) / (size_t) b);
}

static void convolver_remove_dc (std::vector<float> &v) {
  if (v.empty ())
    return;

  double sum = 0.0;
  for (float f : v)
    sum += (double) f;

  const float dc = (float) (sum / (double) v.size ());
  for (float &f : v)
    f -= dc;
}

static void convolver_trim_trailing_silence (
    std::vector<float> &v,
    float threshold = 1.0e-7f) {

  while (!v.empty () && fabsf (v.back ()) <= threshold)
    v.pop_back ();
}

static bool convolver_resample_ir (
    const std::vector<float> &src,
    std::vector<float> &dst,
    uint32_t src_samplerate,
    uint32_t dst_samplerate,
    float semitones) {

  if (src.empty ())
    return false;

  semitones = std::clamp (semitones, -12.0f, 12.0f);
  const double pitch_ratio = pow (2.0, (double) semitones / 12.0);
  if (!std::isfinite (pitch_ratio) || pitch_ratio <= 0.0)
    return false;

  const double sr_ratio =
    src_samplerate && dst_samplerate
      ? (double) dst_samplerate / (double) src_samplerate
      : 1.0;
  const double resample_ratio = sr_ratio / pitch_ratio;
  if (!std::isfinite (resample_ratio) || resample_ratio <= 0.0)
    return false;

  const size_t out_len =
    std::max (
      (size_t) 1,
      (size_t) llround ((double) src.size () * resample_ratio));
  dst.resize (out_len);

  if (src.size () == 1) {
    std::fill (dst.begin (), dst.end (), src [0]);
    return true;
  }

#ifdef HAVE_SAMPLERATE
  std::vector<float> in = src;
  SRC_DATA data {};
  data.data_in = in.data ();
  data.input_frames = (long) in.size ();
  data.data_out = dst.data ();
  data.output_frames = (long) dst.size ();
  data.src_ratio = resample_ratio;

  const int err = src_simple (&data, SRC_SINC_MEDIUM_QUALITY, 1);
  if (!err && data.output_frames_gen > 0) {
    dst.resize ((size_t) data.output_frames_gen);
    convolver_trim_trailing_silence (dst);
    return !dst.empty ();
  }
#endif

  for (size_t i = 0; i < out_len; ++i) {
    const double pos = (double) i / resample_ratio;
    size_t i0 = (size_t) floor (pos);
    if (i0 >= src.size () - 1) {
      dst [i] = src.back ();
      continue;
    }

    const size_t i1 = i0 + 1;
    const float frac = (float) (pos - (double) i0);
    dst [i] = src [i0] + (src [i1] - src [i0]) * frac;
  }

  convolver_trim_trailing_silence (dst);
  return !dst.empty ();
}

static inline cpx cpx_mul (const cpx &a, const cpx &b) {
  cpx ret;
  ret.r = a.r * b.r - a.i * b.i;
  ret.i = a.r * b.i + a.i * b.r;
  return ret;
}

static inline void cpx_add (cpx &a, const cpx &b) {
  a.r += b.r;
  a.i += b.i;
}

static void clear_cpx_vector (std::vector<cpx> &v) {
  for (cpx &x : v) {
    x.r = 0.0f;
    x.i = 0.0f;
  }
}

bool c_convolver::load_ir (
    const float *ir,
    uint32_t nframes,
    uint32_t samplerate) { CP

  if (!ir || nframes == 0)
    return false;

  // keep canonical copy for pitch/blocksize rebuilds
  m_ir_source.assign (ir, ir + nframes);
  m_ir_samplerate = samplerate;

  // cleanup
  convolver_remove_dc (m_ir_source);
  convolver_trim_trailing_silence (m_ir_source);
  // TODO: maybe normalize m_ir_source level?

  if (m_ir_source.empty ()) {
    clear ();
    return false;
  }

  if (!rebuild_resampled_ir ()) {
    clear ();
    return false;
  }

  m_loaded = true;
  m_ready = false;

  // if block size is already known, build realtime FFT state
  if (m_blocksize > 0) {
    if (!rebuild_for_blocksize (m_blocksize)) {
      m_ready = false;
      return false;
    }
  }

  return true;
}

bool c_convolver::load_ir_from_file (const char *filename, int channel) {
  if (!filename || !filename [0])
    return false;

  std::vector<float> ir;
  int sr = 0;

  if (!read_wav (filename, ir, channel, &sr))
    return false;

  return load_ir (ir.data (), (uint32_t) ir.size (), (uint32_t) sr);
}

void c_convolver::clear () {
  clear_fft_state ();
  m_ir_source.clear ();
  m_ir.clear ();
  m_loaded = false;
  m_pitch_semitones = 0.0f;
  m_ir_samplerate = 0;
}

void c_convolver::reset () {
  for (std::vector<cpx> &slot : m_accum_fft)
    clear_cpx_vector (slot);

  std::fill (m_overlap.begin (), m_overlap.end (), 0.0f);
  m_accum_pos = 0;
}

bool c_convolver::loaded () const {
  return m_loaded;
}

bool c_convolver::ready () const {
  return m_ready;
}

void c_convolver::set_samplerate (uint32_t samplerate) {
  if (!samplerate || samplerate == m_samplerate)
    return;

  m_samplerate = samplerate;
  if (!m_loaded)
    return;

  if (!rebuild_resampled_ir ()) {
    m_ready = false;
    return;
  }

  if (m_blocksize > 0)
    rebuild_for_blocksize (m_blocksize);
}

void c_convolver::set_blocksize (uint32_t nframes) {
  if (nframes == m_blocksize)
    return;

  m_blocksize = nframes;

  if (m_loaded && m_blocksize > 0)
    rebuild_for_blocksize (m_blocksize);
}

bool c_convolver::set_pitch_semitones (float semitones) {
  semitones = std::clamp (semitones, -12.0f, 12.0f);
  if (semitones == m_pitch_semitones)
    return true;

  m_pitch_semitones = semitones;
  if (!m_loaded)
    return true;

  if (!rebuild_resampled_ir ())
    return false;

  if (m_blocksize > 0)
    return rebuild_for_blocksize (m_blocksize);

  m_ready = false;
  return true;
}

void c_convolver::clear_fft_state () { CP
  if (m_forward_plan) {
    fftwf_destroy_plan (m_forward_plan);
    m_forward_plan = nullptr;
  }
  
  if (m_inverse_plan) {
    fftwf_destroy_plan (m_inverse_plan);
    m_inverse_plan = nullptr;
  }
  
  if (m_fftw_time_in) {
    fftwf_free (m_fftw_time_in);
    m_fftw_time_in = nullptr;
  }
  
  if (m_fftw_time_out) {
    fftwf_free (m_fftw_time_out);
    m_fftw_time_out = nullptr;
  }
  
  if (m_fftw_freq_in) {
    fftwf_free (m_fftw_freq_in);
    m_fftw_freq_in = nullptr;
  }
  
  if (m_fftw_freq_out) {
    fftwf_free (m_fftw_freq_out);
    m_fftw_freq_out = nullptr;
  }
  
  m_fft_out.clear ();
  m_ir_fft.clear ();
  m_accum_fft.clear ();
  m_overlap.clear ();
  
  m_ready = false;
  m_fft_size = 0;
  m_freq_bins = 0;
  m_num_partitions = 0;
  m_accum_pos = 0;
}

bool c_convolver::rebuild_resampled_ir () {
  return convolver_resample_ir (
    m_ir_source,
    m_ir,
    m_ir_samplerate,
    m_samplerate,
    m_pitch_semitones);
}

// THIS MUST BE CALLED FROM THE LOADER THREAD!!!
bool c_convolver::rebuild_for_blocksize (uint32_t blocksize) {
  if (m_ir.empty () || blocksize == 0)
    return false;
    
  clear_fft_state ();

  m_blocksize = blocksize;
  m_partition_size = blocksize;
  m_fft_size = m_partition_size * 2;

  m_num_partitions =
    ceil_div_u32 (m_ir.size (), m_partition_size);
  if (m_num_partitions == 0)
    return false;

  m_overlap.resize (m_partition_size);
  std::fill (m_overlap.begin (), m_overlap.end (), 0.0f);

  // frequency-domain sizes for real FFT
  m_freq_bins = (m_fft_size / 2) + 1;

  // frequency-domain IR partitions
  m_ir_fft.resize (m_num_partitions);
  for (std::vector<cpx> &partition : m_ir_fft)
    partition.resize (m_freq_bins);

  // freq-domain accumulation ring
  // 1 slot per IR partition.
  m_accum_fft.resize (m_num_partitions);
  for (std::vector<cpx> &slot : m_accum_fft) {
    slot.resize (m_freq_bins);
    clear_cpx_vector (slot);
  }
  m_fft_out.resize (m_freq_bins);
  
  // FFTW buffers/plans
  m_fftw_time_in =
    (float *) fftwf_alloc_real (m_fft_size);
  m_fftw_time_out =
    (float *) fftwf_alloc_real (m_fft_size);

  m_fftw_freq_in =
    (fftwf_complex *) fftwf_alloc_complex (m_freq_bins);
  m_fftw_freq_out =
    (fftwf_complex *) fftwf_alloc_complex (m_freq_bins);

  if (!m_fftw_time_in || !m_fftw_time_out ||
      !m_fftw_freq_in || !m_fftw_freq_out) {
    clear_fft_state ();
    return false;
  }

  m_forward_plan =
    fftwf_plan_dft_r2c_1d (
      (int) m_fft_size,
      m_fftw_time_in,
      m_fftw_freq_out,
      FFTW_MEASURE);

  m_inverse_plan =
    fftwf_plan_dft_c2r_1d (
      (int) m_fft_size,
      m_fftw_freq_in,
      m_fftw_time_out,
      FFTW_MEASURE);

  if (!m_forward_plan || !m_inverse_plan) {
    clear_fft_state ();
    return false;
  }
  
  // pre-transform each IR partition
  for (uint32_t p = 0; p < m_num_partitions; ++p) {
    std::fill (m_fftw_time_in, m_fftw_time_in + m_fft_size, 0.0f);

    const size_t ir_offset = (size_t) p * (size_t) m_partition_size;

    for (uint32_t j = 0; j < m_partition_size; ++j) {
      if (ir_offset + j < m_ir.size ())
        m_fftw_time_in [j] = m_ir [ir_offset + j];
    }

    fftwf_execute (m_forward_plan);

    // Copy FFTW output into our persistent IR partition bins.
    for (uint32_t bin = 0; bin < m_freq_bins; ++bin) {
      m_ir_fft [p] [bin].r = m_fftw_freq_out [bin] [0];
      m_ir_fft [p] [bin].i = m_fftw_freq_out [bin] [1];
    }
  }

  // reset runtime state
  reset ();
  m_ready = true;

  return true;
}

void c_convolver::process_block (
    const float *in,
    float *out,
    uint32_t nframes) {

  if (!in || !out || !nframes)
    return;

  if (!m_ready || nframes != m_blocksize) {
    if (in != out) {
      for (uint32_t i = 0; i < nframes; ++i)
        out [i] = in [i];
    }
    return;
  }

  // 1. FFT input block, zero-padded to fft_size
  std::fill (m_fftw_time_in, m_fftw_time_in + m_fft_size, 0.0f);
  for (uint32_t i = 0; i < m_blocksize; ++i)
    m_fftw_time_in [i] = in [i];

  fftwf_execute (m_forward_plan);
  for (uint32_t bin = 0; bin < m_freq_bins; ++bin) {
    m_fft_out [bin].r = m_fftw_freq_out [bin] [0];
    m_fft_out [bin].i = m_fftw_freq_out [bin] [1];
  }

  // 2. Multiply current input FFT by every IR partition.
  //    Add each result into a future accumulation slot.
  for (uint32_t p = 0; p < m_num_partitions; ++p) {
    const uint32_t slot = (m_accum_pos + p) % m_num_partitions;

    for (uint32_t bin = 0; bin < m_freq_bins; ++bin)
      cpx_add (
        m_accum_fft [slot] [bin],
        cpx_mul (m_fft_out [bin], m_ir_fft [p] [bin]));
  }

  // 3. Inverse FFT the current accumulation slot
  for (uint32_t bin = 0; bin < m_freq_bins; ++bin) {
    m_fftw_freq_in [bin] [0] = m_accum_fft [m_accum_pos] [bin].r;
    m_fftw_freq_in [bin] [1] = m_accum_fft [m_accum_pos] [bin].i;
  }
  fftwf_execute (m_inverse_plan);

  // 4. Normalize IFFT. FFTW inverse is unnormalized.
  const float scale = 1.0f / (float) m_fft_size;

  for (uint32_t i = 0; i < m_blocksize; ++i)
    out [i] = m_fftw_time_out [i] * scale + m_overlap [i];

  for (uint32_t i = 0; i < m_blocksize; ++i)
    m_overlap [i] = m_fftw_time_out [i + m_blocksize] * scale;

  // 5. Clear current accumulation slot for reuse
  clear_cpx_vector (m_accum_fft [m_accum_pos]);

  // 6. Advance ring
  m_accum_pos = (m_accum_pos + 1) % m_num_partitions;
}

#endif

////////////////////////////////////////////////////////////////////////////////
// c_neuralamp


static int nam_version_major_minor (const std::string &filename) {
  std::ifstream f (filename, std::ios::binary);
  if (!f)
    return -1;

  std::string head (8192, '\0');
  f.read (head.data (), head.size ());
  head.resize ((size_t) f.gcount ());

  size_t pos = head.find("\"version\"");
  if (pos == std::string::npos)
    return -1;

  pos = head.find (':', pos);
  if (pos == std::string::npos)
    return -1;

  pos = head.find ('"', pos);
  if (pos == std::string::npos)
    return -1;

  size_t end = head.find ('"', pos + 1);
  if (end == std::string::npos)
    return -1;

  int major = 0, minor = 0;
  if (sscanf (head.substr (pos + 1, end - pos - 1).c_str (),
             "%d.%d", &major, &minor) != 2)
    return -1;

  return major * 1000 + minor;
}

static bool nam_file_is_a2 (const std::string &filename) {
  return nam_version_major_minor (filename) >= 7; // 0.7.x
}


c_neuralamp::c_neuralamp () { CP
}

c_neuralamp::~c_neuralamp () { CP
}

void c_neuralamp::set_samplerate (uint32_t sr) { CP
  if (samplerate == sr)
    return;

  samplerate = sr;
  m_convolver.set_samplerate (sr);
  reset ();
}

void c_neuralamp::set_blocksize (uint32_t bs) { CP
  if (blocksize == bs)
    return;

  blocksize = bs;
  if (m_convolver.loaded ())
    m_convolver.set_blocksize (bs);
}

bool c_neuralamp::set_ir_pitch (float semitones) {
  semitones = std::clamp (semitones, -12.0f, 12.0f);

  std::lock_guard<std::mutex> lock (model_mutex);
  ir_pitch_semitones = semitones;

  if (m_engine_mode != ENGINE_IR)
    return true;

  const bool ret = m_convolver.set_pitch_semitones (ir_pitch_semitones);
  if (ret) {
    reset_unlocked ();
    ramp_pos = 0;
    ramp_len = xfade_samples_for_rate (samplerate);
    ramp.store (RAMP_END, std::memory_order_release);
  }

  return ret;
}

bool c_neuralamp::loaded () const {
  return m_loaded.load (std::memory_order_relaxed);
}

std::string c_neuralamp::model_filename () const {
  std::lock_guard<std::mutex> lock (model_mutex);
  try {
    return filename;
  } catch (const std::exception &e) {
    fprintf (stderr,
             "NeuralBlender: failed to copy model filename "
             "(bank=%zu lane=%zu this=%p): %s\n",
             bank, lane, (const void *) this, e.what ());
    return "";
  }
}

bool c_neuralamp::load_nam (const std::string &fn) { CP
  bool a2 = false;
  if (nam_file_is_a2 (fn))
    a2 = true;
  
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

    new_model->Reset (static_cast<double> (samplerate), MAX_BLOCK_SIZE); // revisit later

    m_nam_model = std::move (new_model);

    fprintf (stderr,
            "Loaded NAM model: %s\n",
            fn.c_str ());
    if (a2)
      m_engine_mode = ENGINE_NAM_A2;
    else
      m_engine_mode = ENGINE_NAM_A1;
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

  ramp_pos = 0;
  ramp_len = xfade_samples_for_rate (samplerate);
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

  bool ret = false;
  if (ends_with (load_fn, ".nam"))
    ret = load_nam (load_fn);
  else if (ends_with (load_fn, ".wav")) {
    m_nam_model.reset ();
    m_rtneural_model.reset ();
    m_convolver.clear ();
    m_convolver.set_pitch_semitones (ir_pitch_semitones);
    if (blocksize > 0)
      m_convolver.set_blocksize (blocksize);
    ret = m_convolver.load_ir_from_file (load_fn.c_str (), 0);
    if (ret)
      m_engine_mode = ENGINE_IR;
  } else
    ret = load_json (load_fn);

  if (ret) {
    filename = load_fn;
    m_loaded.store (true, std::memory_order_release);
    reset_unlocked ();
    ramp_pos = 0;
    ramp_len = xfade_samples_for_rate (samplerate);
    ramp.store (RAMP_WARMUP, std::memory_order_release);
  } else {
    reset_unlocked ();
    m_rtneural_model.reset ();
    m_nam_model.reset ();
    m_convolver.clear ();
    m_engine_mode = ENGINE_NONE;
    filename = "";
    m_loaded.store (false, std::memory_order_release);
    ramp_pos = 0;
    ramp_len = xfade_samples_for_rate (samplerate);
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

    case ENGINE_NAM_A1:
    case ENGINE_NAM_A2:
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

    case ENGINE_IR:
      if (!m_convolver.ready ()) {
        return 0.0f;
      }

      {
        std::vector<float> buf (nframes);
        m_convolver.process_block (data, buf.data (), (uint32_t) nframes);

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
  //size_t blocksize = 128;
  // nope, we need same blocksize that the amp is using, or else calibration of
  // IR's just sees the dry calib. sample ---> huge trim
  size_t blocksize = this->blocksize > 0 ? this->blocksize : 128;
  
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
  m_convolver.reset ();
  delay.clear ();
  warmup = WARMUP_BLOCKS;
  ramp_pos = 0;
}

void c_neuralamp::reset () {
  std::lock_guard<std::mutex> lock (model_mutex);
  reset_unlocked ();
}

void c_neuralamp::unload_model () {
  std::lock_guard<std::mutex> lock (model_mutex);
  m_loaded.store (false, std::memory_order_release);
  m_rtneural_model.reset ();
  m_nam_model.reset ();
  m_convolver.clear ();
  m_engine_mode = ENGINE_NONE;
  filename = "";
  trim = 1.0f;
  effective_trim = 1.0f;
  delay.clear ();
  warmup = WARMUP_BLOCKS;
  ramp_pos = 0;
  ramp.store (RAMP_PLAYING, std::memory_order_release);
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
        const float out_gain = output_gain * effective_trim;
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

    case ENGINE_NAM_A1:
    case ENGINE_NAM_A2:
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

        const float out_gain = output_gain * effective_trim;
        for (uint32_t i = 0; i < nframes; ++i)
          if (dcflip)
            out [i] = -1.0f * std::clamp (out [i] * out_gain, -1.0f, 1.0f);
          else
            out [i] = std::clamp (out [i] * out_gain, -1.0f, 1.0f);
      }
      //return;
    break;

    case ENGINE_IR:
      if (!m_convolver.ready ()) {
        copy_with_gain (in, out, nframes, input_gain * output_gain);
        break;
      }

      {
        if (input_gain != 1.0f) {
          for (uint32_t i = 0; i < nframes; ++i)
            in [i] *= input_gain;
        }

        m_convolver.process_block (in, out, nframes);

        const float out_gain = output_gain * effective_trim;
        for (uint32_t i = 0; i < nframes; ++i)
          if (dcflip)
            out [i] = -1.0f * std::clamp (out [i] * out_gain, -1.0f, 1.0f);
          else
            out [i] = std::clamp (out [i] * out_gain, -1.0f, 1.0f);
      }
    break;
  }

  switch (ramp.load (std::memory_order_acquire)) {
    case RAMP_START:
      //debug ("%d (%ld) RAMP_START block %ld", (int) which, (long int) this, (long int) block_counter);
      fade_block_out (out, nframes, ramp_pos, ramp_len);
      if (ramp_pos + nframes >= ramp_len) {
        ramp_pos = 0;
        ramp.store (RAMP_LOADING, std::memory_order_release);
      } else {
        ramp_pos += nframes;
      }
      break;

    case RAMP_LOADING:
      //debug ("%d (%ld) RAMP_LOADING block %ld", (int) which, (long int) this, (long int) block_counter);
      memset(out, 0, nframes * sizeof(float));
      break;

    case RAMP_WARMUP:
      //debug ("%d (%ld) RAMP_WARMUP block %ld", (int) which, (long int) this, (long int) block_counter);
      memset(out, 0, nframes * sizeof(float));
      if (warmup > 0)
        warmup--;
      if (warmup == 0) {
        ramp_pos = 0;
        ramp_len = xfade_samples_for_rate (samplerate);
        ramp.store (RAMP_END, std::memory_order_release);
      }
      break;

    case RAMP_END:
      //debug ("%d (%ld) RAMP_END block %ld", (int) which, (long int) this, (long int) block_counter);
      fade_block_in (out, nframes, ramp_pos, ramp_len);
      if (ramp_pos + nframes >= ramp_len) {
        ramp_pos = 0;
        ramp.store (RAMP_PLAYING, std::memory_order_release);
        warmup = -1;
      } else {
        ramp_pos += nframes;
      }
      break;

    case RAMP_PLAYING:
    default:
      //debug ("%d (%ld) RAMP_PLAYING block %ld", (int) which, (long int) this, (long int) block_counter);
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// c_neuralblender

c_neuralblender::c_neuralblender () { CP
  banks [BANK_PEDAL].meter_in = NULL;
  banks [BANK_AMP].meter_in = NULL;
  banks [BANK_CAB].meter_in = NULL;

  for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
    banks [bank].num_lanes = NB_NUM_MODELS;
    for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
      banks [bank].lanes [i].bank = bank;
      banks [bank].lanes [i].lane = i;
      banks [bank].lane_mute [i].store (false, std::memory_order_relaxed);
      banks [bank].meters_out [i] = NULL;
      banks [bank].lanes [i].dcflip = false;
    }
  }

  c_configfile configfile;
  if (configfile.read_file ()) {
    float calib_target_db = DB_CALIB_TARGET_DEFAULT;
    if (parse_float (
        configfile.get_item (CONFIG_KEY_NAME_CALIB_TARGET),
        calib_target_db))
      set_calib_target_db (calib_target_db);
  }
  
  m_conv_presence.load_ir ((float *) data_gx_presence_contrast_f32,
                           data_gx_presence_contrast_f32_len / sizeof (float),
                           48000);
  
  m_bypass.store (false, std::memory_order_relaxed);
  m_pedal_bypass.store (false, std::memory_order_relaxed);
  m_amp_bypass.store (false, std::memory_order_relaxed);
  m_cab_bypass.store (false, std::memory_order_relaxed);
  update_mutes ();
}

c_neuralblender::~c_neuralblender () { CP
}

bool c_neuralblender::set_master_gain (float db) {
  float gain = db_to_gain (db);
  if (gain > 10.0f) return false; // TODO: should we allow positive gain here?
  if (gain <  0.0f) return false;
  
  master_gain = gain;
  return true;
}

bool c_neuralblender::set_presence (float pres) {
  if (pres < 0.0f) return false;
  if (pres > 1.0f) return false;
  
  presence = pres;
  return true;
}

// i hate dup code :/
c_model_bank &c_neuralblender::which_bank (_lane_bank bank) {
  switch (bank) {
    case BANK_PEDAL:
      return banks [BANK_PEDAL];
    case BANK_AMP:
      return banks [BANK_AMP];
    case BANK_CAB:
      return banks [BANK_CAB];
    default:
      return banks [BANK_AMP];
  }
}

const c_model_bank &c_neuralblender::which_bank (_lane_bank bank) const {
  switch (bank) {
    case BANK_PEDAL:
      return banks [BANK_PEDAL];
    case BANK_AMP:
      return banks [BANK_AMP];
    case BANK_CAB:
      return banks [BANK_CAB];
    default:
      return banks [BANK_AMP];
  }
}

c_neuralamp &c_neuralblender::which_amp (_lane_bank bank, size_t lane) {
  //debug ("bank=%d, lane=%d", (int) bank, (int) lane);
  return which_bank (bank).lanes [lane];
}

const c_neuralamp &c_neuralblender::which_amp (_lane_bank bank, size_t lane) const {
  return which_bank (bank).lanes [lane];
}

/*void c_neuralblender::get_calib_data (std::vector<float> &v, bool bass) {
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
}*/

bool c_neuralblender::calibrate (_lane_bank bank, size_t which, bool bass) {
  debug ("bass=%d", (int) bass);
  if (which >= NB_NUM_MODELS)
    return false;
    
  float *data = (float *) data_calib_f32;
  size_t samples = data_calib_f32_len / sizeof (float);
  
  if (bass) {
    data = (float *) data_calib_bass_f32;
    samples = data_calib_bass_f32_len / sizeof (float);
  }
  
  c_neuralamp &amp = which_amp (bank, which);
  if (amp.do_calib && amp.loaded ()) {
    amp.calibrate (data, samples);
  } else {
    amp.calibrate (NULL, 0);
  }
  
  update_effective_trim ();
  return true;
}

bool c_neuralblender::calibrate_linked (_lane_bank bank, bool bass) {
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
    c_neuralamp &amp = which_amp (bank, i);
    if (amp.do_calib && amp.loaded ()) {
      amp.calibrate (data, samples);
      const float trim = amp.trim.load (std::memory_order_acquire);
      if (std::isfinite (trim) && trim > 0.0f) {
        linked_trim = std::min (linked_trim, trim);
        any_linked = true;
      }
    } else {
      amp.calibrate (NULL, 0);
    }
  }

  if (!any_linked)
    return true;

  for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
    c_neuralamp &amp = which_amp (bank, i);
    if (amp.do_calib && amp.loaded ())
      amp.trim.store (linked_trim, std::memory_order_release);
  }
  
  update_effective_trim ();
  return true;
}

void c_neuralblender::update_effective_trim () {
  size_t active_calibrated_banks = 0;

  for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
    const uint32_t active_mask = make_active_lane_mask ((_lane_bank) bank);
    if (!active_mask)
      continue;

    bool bank_has_calib = false;
    for (size_t lane = 0; lane < NB_NUM_MODELS; ++lane) {
      if (!(active_mask & (1u << lane)))
        continue;

      if (banks [bank].lanes [lane].do_calib) {
        bank_has_calib = true;
        break;
      }
    }

    if (bank_has_calib)
      ++active_calibrated_banks;
  }

  const float divisor = active_calibrated_banks > 0 ?
    (float) active_calibrated_banks : 1.0f;

  for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
    for (size_t lane = 0; lane < NB_NUM_MODELS; ++lane) {
      c_neuralamp &amp = banks [bank].lanes [lane];
      const float trim = amp.trim.load (std::memory_order_acquire);
      if (!amp.do_calib || !std::isfinite (trim) || trim <= 0.0f) {
        amp.effective_trim.store (1.0f, std::memory_order_release);
        continue;
      }

      amp.effective_trim.store (
        db_to_gain (gain_to_db (trim) / divisor),
        std::memory_order_release);
    }
  }
}

void c_neuralblender::set_samplerate (uint32_t sr) { CP
  debug ("start");
  m_samplerate = sr;
  noisegate.set_samplerate (sr);
  pitchtracker.set_samplerate (sr);
  
  for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
    if (banks [bank].meter_in)
      banks [bank].meter_in->samplerate = (int) sr;

    for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
      banks [bank].lanes [i].set_samplerate (sr);
      if (banks [bank].meters_out [i])
        banks [bank].meters_out [i]->samplerate = (int) sr;
    }
  }
  if (meter_masterin)
    meter_masterin->samplerate = (int) sr;
  if (meter_masterout)
    meter_masterout->samplerate = (int) sr;
  
  m_conv_presence.set_samplerate (sr);
  debug ("end");
}

void c_neuralblender::set_blocksize (uint32_t bs) { CP
  if (m_blocksize == bs)
    return;

  m_blocksize = bs;

  for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
    if (banks [bank].meter_in)
      banks [bank].meter_in->bufsize = (int) bs;

    for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
      banks [bank].lanes [i].set_blocksize (bs);
      if (banks [bank].meters_out [i])
        banks [bank].meters_out [i]->bufsize = (int) bs;
    }
  }
  if (meter_masterin)
    meter_masterin->bufsize = (int) bs;
  if (meter_masterout)
    meter_masterout->bufsize = (int) bs;

  for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
    m_delay_bufs [i].resize (MAX_BLOCK_SIZE);
    m_model_bufs [i].resize (MAX_BLOCK_SIZE);
  }
  
  m_conv_presence.set_blocksize (bs);
}

bool c_neuralblender::dcflip (_lane_bank bank, size_t which, bool b) {
  if (which >= NB_NUM_MODELS)
    return false;
  which_amp (bank, which).dcflip = b;
  return true;
}

bool c_neuralblender::is_dcflipped (_lane_bank bank, size_t which) {
  if (which >= NB_NUM_MODELS) return false;
  return which_amp (bank, which).dcflip;
}

bool c_neuralblender::calib_on (_lane_bank bank, size_t which, bool b) {
  if (which >= NB_NUM_MODELS)
    return false;
  which_amp (bank, which).do_calib = b;
  return true;
}

bool c_neuralblender::is_calib_on (_lane_bank bank, size_t which) {
  if (which >= NB_NUM_MODELS) return false;
  return which_amp (bank, which).do_calib;
}

bool c_neuralblender::set_delay_frames (_lane_bank bank, size_t which, uint32_t frames) { CP
  if (which >= NB_NUM_MODELS)
    return false;
  
  return which_amp (bank, which).delay.set_frames (frames);
}

bool c_neuralblender::set_gain_in (_lane_bank bank, size_t which, float g) {
  if (which >= NB_NUM_MODELS)
    return false;

  which_amp (bank, which).gain_in = clamp_gain_multiplier (g);
  return true;
}

bool c_neuralblender::set_ir_pitch (_lane_bank bank, size_t which, float semitones) {
  if (which >= NB_NUM_MODELS)
    return false;

  return which_amp (bank, which).set_ir_pitch (semitones);
}

bool c_neuralblender::set_gain_out (_lane_bank bank, size_t which, float g) {
  if (which >= NB_NUM_MODELS)
    return false;

  which_amp (bank, which).gain_out = clamp_gain_multiplier (g);
  return true;
}

bool c_neuralblender::set_dry_out (_lane_bank bank, size_t which, float g) {
  if (which >= NB_NUM_MODELS)
    return false;

  which_amp (bank, which).dry_out = clamp_dry_multiplier (g);
  return true;
}

bool c_neuralblender::set_lane_mute (_lane_bank bank, size_t which, bool muted) {
  if (which >= NB_NUM_MODELS)
    return false;

  which_bank (bank).lane_mute [which].store (
    muted, std::memory_order_relaxed);
  request_mix_update ();
  return true;
}

bool c_neuralblender::set_exclusive_lane (_lane_bank bank, int lane) {
  if (lane < 0)
    lane = 0;
  if (lane > (int) NB_NUM_MODELS)
    lane = (int) NB_NUM_MODELS;

  which_bank (bank).exclusive_lane = lane;
  request_mix_update ();
  return true;
}

void c_neuralblender::set_bypass (bool b) {
  m_bypass.store (b, std::memory_order_relaxed);
  request_mix_update ();
}

void c_neuralblender::set_pedal_bypass (bool b) {
  m_pedal_bypass.store (b, std::memory_order_relaxed);
  request_mix_update ();
}

void c_neuralblender::set_amp_bypass (bool b) {
  m_amp_bypass.store (b, std::memory_order_relaxed);
  request_mix_update ();
}

void c_neuralblender::set_cab_bypass (bool b) {
  m_cab_bypass.store (b, std::memory_order_relaxed);
  request_mix_update ();
}

bool c_neuralblender::lane_mute (_lane_bank bank, size_t which) const {
  if (which >= NB_NUM_MODELS)
    return false;

  return which_bank (bank).lane_mute [which].load (
    std::memory_order_relaxed);
}

bool c_neuralblender::bypass () const {
  return m_bypass.load (std::memory_order_relaxed);
}

bool c_neuralblender::pedal_bypass () const {
  return m_pedal_bypass.load (std::memory_order_relaxed);
}

bool c_neuralblender::amp_bypass () const {
  return m_amp_bypass.load (std::memory_order_relaxed);
}

bool c_neuralblender::cab_bypass () const {
  return m_cab_bypass.load (std::memory_order_relaxed);
}

float c_neuralblender::delay_ms (_lane_bank bank, size_t which) const {
  if (which >= NB_NUM_MODELS || !m_samplerate)
    return 0.0f;

  return (float) which_amp (bank, which).delay.frames () *
    1000.0f / (float) m_samplerate;
}

float c_neuralblender::delay_ms (size_t which) const {
  return delay_ms (BANK_AMP, which);
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
  state.bypass          = bypass ();
  state.pedal_bypass    = pedal_bypass ();
  state.amp_bypass      = amp_bypass ();
  state.cab_bypass      = cab_bypass ();
  state.do_vu           = do_vu;
  state.mute_all        = mute_all;
  state.master_gain     = master_gain;
  state.presence        = presence;
  state.tuner_on        = tuner_on;
  state.tuner_base_freq = tuner_base_freq;
  state.noisegate_on    = noisegate_on;
  state.noisethresh     = noisegate.threshold_db;
  state.noiseattack     = noisegate.attack_ms;
  state.noisehold       = noisegate.hold_ms;
  state.noiserelease    = noisegate.release_ms;
  state.calib_target_db = banks [BANK_AMP].lanes [0].calib_target_db;
  state.calib_source    = calib_source;
  
  for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
    state.banks [bank].exclusive_lane = banks [bank].exclusive_lane;
    state.banks [bank].linked_calib = banks [bank].linked_calib;

    for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
      const c_neuralamp &amp = banks [bank].lanes [i];
      c_neuralblender_lane_state &lane = state.banks [bank].lanes [i];
      debug ("before calling amp.model_filename ()");
      lane.filename = amp.model_filename ();
      debug ("after calling amp.model_filename ()");
      lane.gain_in = amp.gain_in;
      lane.ir_pitch_semitones = amp.ir_pitch_semitones;
      lane.gain_out = amp.gain_out;
      lane.dry_out = amp.dry_out;
      lane.delay_ms = (float) amp.delay.frames () * 1000.0f / (float) m_samplerate;
      lane.lane_mute = lane_mute ((_lane_bank) bank, i);
      lane.loaded = amp.loaded ();
      lane.dcflip = amp.dcflip;
      lane.do_calib = amp.do_calib;
    }
  }
}

void c_neuralblender::update_mutes () {
  for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; bank++) {
    uint32_t loaded_mask = 0;

    for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
      const bool loaded = banks [bank].lanes [i].loaded ();
      banks [bank].lanes [i].mute = !loaded;
      if (loaded)
        loaded_mask |= 1u << i;
    }

    if (!loaded_mask)
      banks [bank].lanes [0].mute = false;

    banks [bank].active_mask = loaded_mask;
  }

  // Compatibility for current amp-only process_block().
  // loaded_lane_mask == 0: no models loaded, passthrough
  // loaded_lane_mask != 0 but active mask is 0: models exist but are
  // muted/excluded, silence/mix logic
  loaded_lane_mask.store (
    banks [BANK_AMP].active_mask, std::memory_order_release);

  request_mix_update ();
}

bool c_neuralblender::set_calib_target_db (float f) { CP
  bool ret = true;
  
  if (!std::isfinite (f))
    return false;
    
  f = clamp_calib_target_db (f);
  
  for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank)
    for (size_t i = 0; i < NB_NUM_MODELS; i++)
      banks [bank].lanes [i].calib_target_db = f;
  
  return ret;
}

bool c_neuralblender::load_model (_lane_bank bank, size_t which, const char *fn) { CP
  if (which >= NB_NUM_MODELS) {
    debug ("which >= %d", NB_NUM_MODELS);
    return false;
  }

  c_neuralamp &amp = which_amp (bank, which);
  debug ("LOAD MODEL, block %ld", (long int) amp.block_counter);

  const bool requested = amp.request_load_model (fn);
  if (!requested) {
    update_mutes ();
    return false;
  }

  int wait_ms = 0;
  while (!amp.ready_to_load () && wait_ms < 250) {
    std::this_thread::sleep_for (std::chrono::milliseconds (1));
    wait_ms++;
  }

  if (!amp.ready_to_load ())
    amp.ramp.store (RAMP_LOADING, std::memory_order_release);

  const bool ret = amp.load_model ();

  int bf = 0;
  for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
    if (banks [bank].lanes [i].loaded ())
      bf |= (1 << i);
  }

  debug ("mute bitfield 0x%02x", bf);
  update_mutes ();
  return ret;
}

bool c_neuralblender::unload_model (_lane_bank bank, size_t which) { CP
  if (which >= NB_NUM_MODELS)
    return false;

  banks [bank].lanes [which].unload_model ();
  if (banks [bank].meters_out [which])
    banks [bank].meters_out [which]->update ();
  update_mutes ();
  return true;
}

bool c_neuralblender::set_delay_ms (_lane_bank bank, size_t which, float ms) {
  const uint32_t frames =
    (uint32_t) ((ms * m_samplerate) / 1000.0f + 0.5f); // round to nearest int

  return set_delay_frames (bank, which, frames);
}

void c_neuralblender::update_loaded_output_meters (_lane_bank bank) {
  if (!do_vu)
    return;

  for (size_t lane = 0; lane < NB_NUM_MODELS; ++lane) {
    if (which_amp (bank, lane).loaded () && which_bank (bank).meters_out [lane])
      which_bank (bank).meters_out [lane]->update ();
  }
}

int c_neuralblender::tuner_freq () {
  if (!pitchtracker.analyze ())
    return 0;

  return (int) lrintf (
    pitchtracker.detected_freq.load (std::memory_order_acquire));
}

// mixing related static funcs, these are pretty self explanatory

static inline float ramp_in_gain (uint32_t i, uint32_t n) {
  return n > 1 ? (float) i / (float) (n - 1) : 1.0f;
}

static inline float ramp_out_gain (uint32_t i, uint32_t n) {
  return n > 1 ? 1.0f - ((float) i / (float) (n - 1)) : 0.0f;
}

static void overlay_lane (float *dst, const float *src, uint32_t n) {
  for (uint32_t i = 0; i < n; ++i)
    dst[i] += src[i];
}

static void overlay_lane_xfade_in (
    float *dst, const float *src, uint32_t n,
    uint32_t xfade_pos, uint32_t xfade_len) {

  for (uint32_t i = 0; i < n; ++i)
    dst [i] += src [i] * xfade_in_gain (xfade_pos + i, xfade_len);
}

static void overlay_lane_xfade_out (
    float *dst, const float *src, uint32_t n,
    uint32_t xfade_pos, uint32_t xfade_len) {

  for (uint32_t i = 0; i < n; ++i)
    dst [i] += src [i] * xfade_out_gain (xfade_pos + i, xfade_len);
}

static bool bank_exclusive_empty (const c_model_bank &bank) {
  if (bank.exclusive_lane <= 0)
    return false;

  const size_t lane = (size_t) (bank.exclusive_lane - 1);
  return lane >= NB_NUM_MODELS || !bank.lanes [lane].loaded ();
}

static void final_clamp (float *out, uint32_t n, float master_gain) {
  if (!out)
    return;

  for (uint32_t i = 0; i < n; ++i)
    out [i] = std::clamp (out [i] * master_gain, -1.0f, 1.0f);
}

static bool bank_is_bypassed (
    const c_neuralblender &blender,
    _lane_bank bank) {

  switch (bank) {
    case BANK_PEDAL:
      return blender.pedal_bypass ();
    case BANK_AMP:
      return blender.amp_bypass ();
    case BANK_CAB:
      return blender.cab_bypass ();
    default:
      return false;
  }
}

uint32_t c_neuralblender::make_active_lane_mask (_lane_bank bank) const {
  if (m_bypass.load (std::memory_order_relaxed))
    return 0;

  if (mute_all)
    return 0;
  
  if (bank == BANK_PEDAL &&
      m_pedal_bypass.load (std::memory_order_relaxed))
    return 0;
    
  if (bank == BANK_AMP &&
      m_amp_bypass.load (std::memory_order_relaxed))
    return 0;
    
  if (bank == BANK_CAB &&
      m_cab_bypass.load (std::memory_order_relaxed))
    return 0;
    
  const c_model_bank &b = which_bank (bank);
  if (b.exclusive_lane > 0) {
    const size_t lane = (size_t) (b.exclusive_lane - 1);
    if (lane >= NB_NUM_MODELS)
      return 0;

    if (!b.lanes [lane].loaded ())
      return 0;

    if (b.lanes [lane].mute.load ())
      return 0;

    return 1u << lane;
  }

  uint32_t mask = 0;

  for (size_t lane = 0; lane < NB_NUM_MODELS; ++lane) {
    if (!b.lanes [lane].loaded ())
      continue;

    if (b.lanes [lane].mute.load () ||
        b.lane_mute [lane].load (std::memory_order_relaxed))
      continue;

    mask |= 1u << lane;
  }

  return mask;
}

// called when our mixing topology changes
void c_neuralblender::request_mix_update () {
  pending_lane_mask.store (make_active_lane_mask (BANK_AMP),
                           std::memory_order_release);
  update_effective_trim ();
  xfade_pending.store (true, std::memory_order_release);
}

// alsp applies noise gate if it's enabled
float *c_neuralblender::prepare_input_buffer (
    float *in, float *out, uint32_t nframes) {
  
  if (!in || nframes == 0)
    return in;
  
  if (in != out) {
    if (noisegate_on) {
      if (m_input_buf.size () < nframes)
        m_input_buf.resize (nframes);
      noisegate.process_block (in, m_input_buf.data (), nframes);
      return m_input_buf.data ();
    }

    return in;
  }
  
  if (m_input_buf.size () < nframes)
    m_input_buf.resize (nframes);

  if (noisegate_on) {
    noisegate.process_block (in, m_input_buf.data (), nframes);
    //CP
  } else {
    memcpy (m_input_buf.data (), in, nframes * sizeof (float));
    //CP
  }
  
  return m_input_buf.data ();
}

static void update_meter_data (c_vudata *meter, const float *in, uint32_t nframes) {
  if (!meter || !in)
    return;

  for (uint32_t i = 0; i < nframes; i++)
    meter->sample (in [i], 0.0f);
  meter->update ();
}

void c_neuralblender::update_input_meter (_lane_bank bank, float *in, uint32_t nframes) {
  c_vudata *meter = nullptr;
  switch (bank) {
    case BANK_PEDAL: meter = banks [BANK_PEDAL].meter_in; break;
    case BANK_AMP:   meter = banks [BANK_AMP].meter_in;   break;
    case BANK_CAB:   meter = banks [BANK_CAB].meter_in;   break;
    default:         meter = nullptr;         break;
  }

  if (do_vu)
    update_meter_data (meter, in, nframes);
}

void c_neuralblender::render_lane (_lane_bank bank,
    size_t lane, float *in, uint32_t nframes) {

  if (lane >= NB_NUM_MODELS)
    return;

  if (m_delay_bufs [lane].size () < nframes)
    m_delay_bufs [lane].resize (nframes);

  if (m_model_bufs [lane].size () < nframes)
    m_model_bufs [lane].resize (nframes);

  banks [bank].lanes [lane].delay.process_block (in, m_delay_bufs [lane].data (), nframes);
  banks [bank].lanes [lane].process_block (m_delay_bufs [lane].data (),
                             m_model_bufs [lane].data (),
                             nframes);
  
  const float dry_gain = clamp_dry_multiplier (banks [bank].lanes [lane].dry_out);
  if (dry_gain > 0.0f) {
    const float *dry = m_delay_bufs [lane].data ();
    float *dst = m_model_bufs [lane].data ();

    for (uint32_t i = 0; i < nframes; i++)
      dst [i] += dry [i] * dry_gain;
  }

  if (banks [bank].meters_out [lane] && do_vu) {
    for (uint32_t i = 0; i < nframes; ++i)
      banks [bank].meters_out [lane]->sample (m_model_bufs [lane] [i], 0.0f);
    banks [bank].meters_out [lane]->update ();
  }
}

void c_neuralblender::render_mix (float *in, float *out, uint32_t nframes,
                                  _lane_bank bank,
                                  uint32_t old_mask, uint32_t new_mask,
                                  uint32_t xfade_pos, uint32_t xfade_len) {
  const uint32_t relevant = old_mask | new_mask;

  for (size_t lane = 0; lane < NB_NUM_MODELS; ++lane) {
    const uint32_t bit = 1u << lane;

    if (!(relevant & bit)) {
      if (banks [bank].meters_out [lane] && do_vu)
        banks [bank].meters_out [lane]->update ();

      continue;
    }

    render_lane (bank, lane, in, nframes);

    const bool was = old_mask & bit;
    const bool now = new_mask & bit;

    if (was && now)
      overlay_lane (out, m_model_bufs [lane].data (), nframes);
    else if (was && !now)
      overlay_lane_xfade_out (out, m_model_bufs[lane].data (), nframes,
                          xfade_pos, xfade_len);
    else if (!was && now)
      overlay_lane_xfade_in (out, m_model_bufs[lane].data (), nframes,
                         xfade_pos, xfade_len);
  }
}

void c_neuralblender::render_bank (
    _lane_bank bank,
    float *in,
    float *out,
    uint32_t nframes,
    uint32_t old_mask,
    uint32_t new_mask,
    uint32_t xfade_pos,
    uint32_t xfade_len) {

  if (!in || !out || !nframes)
    return;

  update_input_meter (bank, in, nframes);
  std::fill (out, out + nframes, 0.0f);

  if (bank_is_bypassed (*this, bank)) {
    if (in != out)
      memcpy (out, in, nframes * sizeof (float));
    update_loaded_output_meters (bank);
    return;
  }

  if (!banks [bank].active_mask) {
    if (in != out)
      memcpy (out, in, nframes * sizeof (float));
    update_loaded_output_meters (bank);
    return;
  }

  if (!(old_mask | new_mask)) {
    if (bank_exclusive_empty (banks [bank])) {
      if (in != out)
        memcpy (out, in, nframes * sizeof (float));
      update_loaded_output_meters (bank);
      return;
    }

    update_loaded_output_meters (bank);
    return;
  }

  render_mix (
    in,
    out,
    nframes,
    bank,
    old_mask,
    new_mask,
    xfade_pos,
    xfade_len);

  if (!new_mask && old_mask && bank_exclusive_empty (banks [bank]))
    overlay_lane_xfade_in (out, in, nframes, xfade_pos, xfade_len);
}

void c_neuralblender::process_block (float *in, float *out, uint32_t nframes) {
  if (!in || !out || !nframes)
    return;

  if (do_vu)
    update_meter_data (meter_masterin, in, nframes);
  
  if (tuner_on) {
    pitchtracker.process_block (in, nframes);
  }

  float *process_in = prepare_input_buffer (in, out, nframes);
  bool do_presence = true;

  uint32_t new_mask =
    pending_lane_mask.load (std::memory_order_acquire);
  
  // cross fade
  const bool requested_transition =
    xfade_pending.exchange (false, std::memory_order_acq_rel);

  if (requested_transition) {
    const uint32_t old_mask =
      xfade_active
        ? xfade_new_mask
        : active_lane_mask.load (std::memory_order_acquire);

    if (new_mask != old_mask) {
      xfade_old_mask = old_mask;
      xfade_new_mask = new_mask;
      xfade_pos = 0;
      xfade_len = xfade_samples_for_rate (m_samplerate);
      xfade_active = true;
    }
  } else if (!xfade_active) {
    const uint32_t old_mask =
      active_lane_mask.load (std::memory_order_acquire);
    new_mask = make_active_lane_mask (BANK_AMP);
    if (new_mask != old_mask) {
      xfade_old_mask = old_mask;
      xfade_new_mask = new_mask;
      xfade_pos = 0;
      xfade_len = xfade_samples_for_rate (m_samplerate);
      xfade_active = true;
    }
  }

  std::fill (out, out + nframes, 0.0f);

  if (m_bypass.load (std::memory_order_relaxed)) {
    update_input_meter (BANK_PEDAL, process_in, nframes);
    update_input_meter (BANK_AMP, process_in, nframes);
    update_input_meter (BANK_CAB, process_in, nframes);
    if (process_in != out)
      memcpy (out, process_in, nframes * sizeof (float));
    update_loaded_output_meters (BANK_PEDAL);
    update_loaded_output_meters (BANK_AMP);
    update_loaded_output_meters (BANK_CAB);
    if (do_vu)
      update_meter_data (meter_masterout, out, nframes);
    return;
  }
  
  // here we should keep realloc/resizing to a minimum
  if (m_stage_buf_a.size () < nframes)
    m_stage_buf_a.resize (nframes);
  if (m_stage_buf_b.size () < nframes)
    m_stage_buf_b.resize (nframes);
  if (m_presence_buf.size () < nframes)
    m_presence_buf.resize (nframes);

  if (!xfade_active) {
    const uint32_t mask = active_lane_mask.load (std::memory_order_acquire);
    const uint32_t pedal_mask = make_active_lane_mask (BANK_PEDAL);
    const uint32_t cab_mask = make_active_lane_mask (BANK_CAB);

    render_bank (
      BANK_PEDAL, process_in, m_stage_buf_a.data (), nframes,
      pedal_mask, pedal_mask, 0, 1);
    render_bank (
      BANK_AMP, m_stage_buf_a.data (), m_stage_buf_b.data (), nframes,
      mask, mask, 0, 1);
    render_bank (
      BANK_CAB, m_stage_buf_b.data (), out, nframes,
      cab_mask, cab_mask, 0, 1);
  } else {
    const uint32_t pedal_mask = make_active_lane_mask (BANK_PEDAL);
    const uint32_t cab_mask = make_active_lane_mask (BANK_CAB);

    render_bank (
      BANK_PEDAL, process_in, m_stage_buf_a.data (), nframes,
      pedal_mask, pedal_mask, 0, 1);
    render_bank (
      BANK_AMP, m_stage_buf_a.data (), m_stage_buf_b.data (), nframes,
      xfade_old_mask, xfade_new_mask, xfade_pos, xfade_len);
    render_bank (
      BANK_CAB, m_stage_buf_b.data (), out, nframes,
      cab_mask, cab_mask, 0, 1);
    
    if (xfade_pos + nframes >= xfade_len) {
      active_lane_mask.store (xfade_new_mask, std::memory_order_release);
      xfade_active = false;
      xfade_pos = 0;
    } else {
      xfade_pos += nframes;
    }
  }

  const float presence_mix = std::clamp (presence, 0.0f, 1.0f);
  if (presence_mix > 0.0f && do_presence && m_conv_presence.ready ()) {
    m_conv_presence.process_block (out, m_presence_buf.data (), nframes);

    constexpr float half_pi = 1.57079632679489661923f;
    const float angle = presence_mix * half_pi;
    const float dry_gain = cosf (angle);
    const float wet_gain = sinf (angle);

    for (uint32_t i = 0; i < nframes; i++)
      out [i] = out [i] * dry_gain + m_presence_buf [i] * wet_gain;
  }

  final_clamp (out, nframes, master_gain);
  if (do_vu)
    update_meter_data (meter_masterout, out, nframes);
}
