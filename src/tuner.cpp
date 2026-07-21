
/* NeuralBlender - tuner / pitch tracking data.
 */

#include "tuner.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

#define CMDLINE_DEBUG_COLOR ANSI_DARK_GREEN
#include "cmdline_debug.h"

#ifndef METER_DATA_ONLY
#include "xputty_compat.h"
#endif

#ifdef METER_DATA_ONLY


#define TUNER_THRESH_DB -60.0f

static inline float tuner_db_to_gain (float db) {
  return powf (10.0f, db / 20.0f);
}

static float block_rms (const std::vector<float> &buf) {
  double sum = 0.0;
  for (float x : buf)
    sum += (double) x * (double) x;
  return buf.empty () ? 0.0f : (float) sqrt (sum / (double) buf.size ());
}

c_pitchtracker::c_pitchtracker () { CP }

c_pitchtracker::~c_pitchtracker () { CP }

void c_pitchtracker::set_samplerate (int sr) { CP
  if (sr < 800 || sr > 192000)
    return;

  samplerate = sr;
  set_block_size (samplerate / 10);
}

inline float c_pitchtracker::sample_at (size_t i) const {
  size_t n = ring.size ();
  if (n == 0)
    return 0.0f;
  
  size_t valid = std::min (count, n);
  size_t first = (count >= n) ? (count % n) : 0;
  
  if (i >= valid)
    return 0.0f;
  
  return ring [(first + i) % n];
}

inline float c_pitchtracker::get_lag_score (
    const std::vector<float> &buf,
    size_t lag) const {
  
  const size_t n = buf.size ();
  if (lag == 0 || lag >= n)
    return std::numeric_limits<float>::infinity ();
  
  float totdiff = 0.0f;
  const size_t end = n - lag;
  
  for (size_t i = 0; i < end; i++) {
    const float d = buf [i] - buf [i + lag];
    totdiff += d * d;
  }
  
  return totdiff / (float) end;
}

int c_pitchtracker::get_best_lag (const std::vector<float> &buf, int step,
                                  float *r_score) {
  const int min_freq = 25;
  const int max_freq = 1000;
  
  if (buf.size () < 2 || step <= 0)
    return 0;

  const int start_lag = samplerate / max_freq;
  const int end_lag =
    std::min ((int) (samplerate / min_freq), (int) (buf.size () - 1));
  
  if (start_lag >= end_lag)
    return 0;

  // this should only resize vectors on samplerate change
  if (lag_scores.size () != (size_t) end_lag + 1)
    lag_scores.resize ((size_t) end_lag + 1);
  
  float best_score = get_lag_score (buf, start_lag);
  lag_scores [start_lag] = best_score;
  int best_lag = start_lag;
  int count = 0;
  float scoreavg = 0.0;
  
  for (int lag = start_lag + step; lag < end_lag; lag += step) {
    float s = get_lag_score (buf, lag);
    scoreavg += s;
    count++;
    lag_scores [lag] = s;
    if (s < best_score) {
      best_score = s;
      best_lag = lag;
    }
  }
  scoreavg /= count;
  
  //debug ("scoreavg=%f, best_score=%f", scoreavg, best_score);
  
  int oct = best_lag / 2;
  if (oct >= start_lag) {
    float oct_score = get_lag_score (buf, oct);
    lag_scores [oct] = oct_score;

    if (oct_score <= best_score * 1.25f) {
      best_lag = oct;
      best_score = oct_score;
    }
  }

  if (r_score)
    *r_score = best_score;
  
  return best_lag;
}

bool c_pitchtracker::analyze () {
  const int idx = published_snapshot.load (std::memory_order_acquire);
  if (idx < 0 || idx >= 3)
    return false;

  analysis = snapshots [idx];

  if (block_rms (analysis) < tuner_db_to_gain (TUNER_THRESH_DB)) {
    update_note_from_freq (0.0f);
    return false;
  }
  
  float l = 0.0f;
  float m = 0.0f;
  float r = 0.0f;
  
  const int lag = get_best_lag (analysis, 1, &m);
  
  if (lag <= 1 || lag + 1 >= (int) analysis.size () || lag == samplerate / 1000) {
    update_note_from_freq (0.0f);
    return false;
  }

  // "parabolic" interpolation
  
  l = get_lag_score (analysis, lag - 1);
  r = get_lag_score (analysis, lag + 1);
  float denom = l - 2.0f * m + r;
  float offset = 0.0f;
  if (fabsf(denom) > 1e-12f)
    offset = 0.5f * (l - r) / denom;
  offset = std::clamp(offset, -0.5f, 0.5f);
  float refined_lag = lag + offset;

  const float freq = (float) samplerate / (float) refined_lag;
  update_note_from_freq (freq);

  //detected_freq.store (freq, std::memory_order_release);
  return true;
}

void c_pitchtracker::update_note_from_freq (float freq) {
  if (freq <= 0.0f || !std::isfinite (freq)) {
    const float old_freq = detected_freq.exchange (0.0f, std::memory_order_release);
    const float old_note = detected_note.exchange (0.0f, std::memory_order_release);
    const float old_cents = detected_cents.exchange (0.0f, std::memory_order_release);
    if (old_freq != 0.0f || old_note != 0.0f || old_cents != 0.0f)
      needs_redraw.store (true, std::memory_order_release);
    return;
  }

  const float midi_f =
    69.0f + 12.0f * log2f (freq / (float) basefreq);
  const int midi = (int) lroundf (midi_f);

  const float note_freq =
    (float) basefreq * powf (2.0f, ((float) midi - 69.0f) / 12.0f);

  const float cents =
    1200.0f * log2f (freq / note_freq);

  //debug ("freq=%f, midi=%d, cents=%f", freq, midi, cents);

  const float old_freq = detected_freq.exchange (freq, std::memory_order_release);
  const float old_note = detected_note.exchange ((float) midi, std::memory_order_release);
  const float old_cents = detected_cents.exchange (cents, std::memory_order_release);
  if (old_freq != freq || old_note != (float) midi || old_cents != cents)
    needs_redraw.store (true, std::memory_order_release);
}

void c_pitchtracker::publish_snapshot () {
  const size_t n = ring.size ();
  if (n == 0)
    return;

  const size_t first = count % n;

  float *dst = snapshots [write_snapshot].data ();

  const size_t front = n - first;
  memcpy (dst, &ring [first], front * sizeof (float));
  if (first)
    memcpy (dst + front, &ring [0], first * sizeof (float));

  published_snapshot.store (write_snapshot, std::memory_order_release);
  published_seq.fetch_add (1, std::memory_order_release);

  write_snapshot++;
  write_snapshot %= 3;
}

void c_pitchtracker::process_block (float *in, int nframes_) {
  size_t bs = ring.size ();

  if (bs <= 0 || !in || nframes_ <= 0)
    return;

  const size_t nframes = (size_t) nframes_;
  const size_t pos = count % bs;
  const size_t front = std::min<size_t> (nframes, bs - pos);
  const size_t back = nframes - front;

  memcpy (&ring [pos], in, front * sizeof (float));
  if (back)
    memcpy (&ring [0], in + front, back * sizeof (float));

  count += nframes;

  if (count >= bs)
    publish_snapshot ();
}

void c_pitchtracker::set_base_freq (int f) { CP
  if (f >= 220 && f <= 880)
    basefreq = f;
  CP
}

void c_pitchtracker::set_block_size (size_t sz) { CP
  ring.resize (sz);
  
  for (int i = 0; i < 3; i++)
    snapshots [i].resize (sz);
    
  analysis.resize (sz);
  count = 0;
  write_snapshot = 0;
  published_snapshot.store (-1, std::memory_order_release);
  published_seq.store (0, std::memory_order_release);

  std::fill (ring.begin (), ring.end (), 0.0f);
  for (int i = 0; i < 3; i++)
    std::fill (snapshots [i].begin (), snapshots [i].end (), 0.0f);
  std::fill (analysis.begin (), analysis.end (), 0.0f);
}

void c_pitchtracker::dump () {
  for (size_t i = 0; i < ring.size (); i++)
    printf ("sample %d=%f\n", (int) i, sample_at (i));

  debug ("lag score (63) = %f", get_lag_score (ring, 63));
  debug ("lag score (64) = %f", get_lag_score (ring, 64));
  debug ("lag score (65) = %f", get_lag_score (ring, 65));

  debug ("get_best_lag (1) = %d", get_best_lag (ring, 1));
  debug ("analyze () returned %d", analyze ());
}

////////////////////////////////////////////////////////////////////////////////
// c_tunerwidget

#else

void c_tunerwidget::create (Widget_t *parent,
                            const char *label,
                            int x, int y, int w, int h) {
  c_customwidget::create (parent, label, x, y, w, h);
}

void c_tunerwidget::set_pitchtracker (c_pitchtracker *p) {
  pitchtracker = p;
}

void c_tunerwidget::set_pitch (float freq, float note, float cents) {
  if (freq == current_freq && note == current_note && cents == current_cents)
    return;

  current_freq = freq;
  current_note = note;
  current_cents = cents;
  ui_needs_redraw.store (true, std::memory_order_release);
}

void c_tunerwidget::on_ui_timer () {
  if (!widget)
    return;

  bool dirty =
    ui_needs_redraw.exchange (false, std::memory_order_acq_rel);

  if (pitchtracker &&
      pitchtracker->needs_redraw.exchange (false, std::memory_order_acq_rel)) {
    current_freq =
      pitchtracker->detected_freq.load (std::memory_order_acquire);
    current_note =
      pitchtracker->detected_note.load (std::memory_order_acquire);
    current_cents =
      pitchtracker->detected_cents.load (std::memory_order_acquire);
    dirty = true;
  }

  if (dirty)
    transparent_draw (widget, NULL);
}

bool c_tunerwidget::needs_redraw () {
  const bool ui_dirty =
    ui_needs_redraw.exchange (false, std::memory_order_acq_rel);
  const bool tracker_dirty =
    pitchtracker && pitchtracker->needs_redraw.exchange (false);

  return ui_dirty || tracker_dirty;
}

void c_tunerwidget::render_base (cairo_t *cr) {
  cairo_set_line_width (cr, height / 50);

  cairo_set_source_rgba (cr, 0.5, 0.5, 1.0, 0.3);
  for (int i = 0; i < 5; i++) {
    int w = std::clamp ((width / 4) * i, 1, width - 1);
    cairo_move_to (cr, w, 0);
    cairo_line_to (cr, w, height);
  }
  cairo_stroke (cr);

  cairo_set_source_rgba (cr, 0.5, 0.8, 1.0, 1.0);
  cairo_move_to (cr, width / 2, 0);
  cairo_line_to (cr, width / 2, height);
  cairo_stroke (cr);
}

static const char *note_names [] = {
  "C-",
  "C#",
  "D-",
  "D#",
  "E-",
  "F-",
  "F#",
  "G-",
  "G#",
  "A-",
  "A#",
  "B-",
};

void c_tunerwidget::on_paint (cairo_t *cr) {
  char buf [32];
  cairo_text_extents_t ext;
  
  bool valid = true;
  if (current_note < 20 || current_note > 108)
    valid = false;
  if (current_freq < 20 || current_freq > 1000)
    valid = false;
  if (current_cents < -50 || current_cents > 50)
    valid = false;
  int abscents = abs (current_cents);
  
  hist_notes.push_back (current_note);
  hist_cents.push_back (current_cents);
  if (hist_notes.size () > INTUNE_DELAY) hist_notes.pop_front ();
  if (hist_cents.size () > INTUNE_DELAY) hist_cents.pop_front ();
  
  int side = height / 5;
  int bracketsize = std::max (height / 10, width * INTUNE_THRESHOLD / 100);
  bool stable_tuning = true;
  float a = 1.0 - ((float) abscents / 100.0);
  
  int i;
  for (i = 0; hist_notes.size () == INTUNE_DELAY && i < INTUNE_DELAY; i++)
    if (((int) hist_notes [i] % 12) != ((int) current_note % 12) || abs (hist_cents [i]) >= INTUNE_THRESHOLD)
      stable_tuning = false;
  
  if (valid) {
    snprintf (buf, 31, "%s%d", note_names [((int) current_note) % 12],
              (int) (-1 + current_note / 12));
    float x = (float) width / 2.0 + (current_cents / 50.0 * (float) width / 2.0);
    if (stable_tuning)
      cairo_set_source_rgba (cr, 1.0, 1.0, 0.0, 1.0);
    else
      cairo_set_source_rgba (cr, 1.0, 1.0, 0.0, a);
  
    cairo_move_to (cr, x - side, height);
    cairo_line_to (cr, x, height / 2);
    cairo_line_to (cr, x + side, height);
    cairo_close_path (cr);
    cairo_fill (cr);
  } else {
    snprintf (buf, 31, "---");
  }
  
  cairo_set_line_width (cr, height / 20);
  
  if (valid && stable_tuning) {
    cairo_set_source_rgba (cr, 1.0, 1.0, 0.0, 1.0);
  } else {
    cairo_set_source_rgba (cr, 1.0, 1.0, 0.0, 0.2);
  }
  
  cairo_move_to (cr, width / 2 - bracketsize, height / 4);
  cairo_line_to (cr, width / 2 - bracketsize, height / 7);
  cairo_line_to (cr, width / 2 + bracketsize, height / 7);
  cairo_line_to (cr, width / 2 + bracketsize, height / 4);
  cairo_stroke (cr);
  
  cairo_set_source_rgba (cr, 0.5, 1.0, 0.5, 1.0);
  cairo_set_font_size (cr, (float) height * 2.0 / 3.0);
  cairo_text_extents (cr, "A", &ext);
  cairo_move_to (cr, 8, ext.height + (height - ext.height) / 2);
  cairo_show_text (cr, buf);
  cairo_set_font_size (cr, height / 4);
  cairo_text_extents (cr, "999.00 Hz ", &ext);
  cairo_move_to (cr, width - ext.width, height - (height / 10));
  if (valid)
    snprintf (buf, 31, "%.2f Hz", current_freq);
  else
    snprintf (buf, 31, "0.00 Hz");
  cairo_show_text (cr, buf);
  
  if (valid)
    snprintf (buf, 31, "%s%d", current_cents >= 0 ? "+" : "-", abscents);
  else
    snprintf (buf, 31, "+0");
  cairo_move_to (cr, width - ext.width, height / 10 + ext.height);
  cairo_show_text (cr, buf);
  
}

void c_tunerwidget::on_resize (int w, int h) {
}

#endif // METER_DATA_ONLY
