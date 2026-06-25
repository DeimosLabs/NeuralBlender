#include "meter.h"

#include <algorithm>
#include <cmath>

float c_vudata::clamp01 (float f) {
  if (f < 0.0f)
    return 0.0f;
  if (f > 1.0f)
    return 1.0f;
  return f;
}

void c_vudata::atomic_max (std::atomic<float> &dst, float value) {
  float old = dst.load ();
  while (value > old && !dst.compare_exchange_weak (old, value)) {}
}

void c_vudata::atomic_min (std::atomic<float> &dst, float value) {
  float old = dst.load ();
  while (value < old && !dst.compare_exchange_weak (old, value)) {}
}

static float linear_to_db_meter (float f) {
  if (f <= 0.0000000001f)
    return -200.0f;

  return 20.0f * log10f (f);
}

float c_vudata::db_scaled (float f) const {
  const float scale = m_db_scale.load ();
  float ret = 0.0f;

  if (scale == 0.0f) {
    ret = f;
  } else if (scale > 0.0f) {
    ret = f;
  } else {
    float db = linear_to_db_meter (f);
    db += scale;
    ret = 2.0f - (db / scale);
  }

  return clamp01 (ret);
}

float c_vudata::display_to_linear (float f) const {
  const float scale = m_db_scale.load ();
  f = clamp01 (f);

  if (scale >= 0.0f)
    return f;

  const float db = scale * (1.0f - f);
  return clamp01 (powf (10.0f, db / 20.0f));
}

bool c_vudata::sample (float l, float r) {
  const float old_plus_l = m_plus_l.load ();
  const float old_minus_l = m_minus_l.load ();
  const float old_plus_r = m_plus_r.load ();
  const float old_minus_r = m_minus_r.load ();

  atomic_max (m_plus_l, l);
  atomic_min (m_minus_l, l);
  atomic_max (m_plus_r, r);
  atomic_min (m_minus_r, r);

  return l > old_plus_l || l < old_minus_l ||
         r > old_plus_r || r < old_minus_r;
}

bool c_vudata::update () {
  if (bufsize <= 0)
    return false;

  int bufs_sec = samplerate / bufsize;
  if (bufs_sec < 1)
    bufs_sec = 1;

  int redraw_every = (int) (VU_REDRAW_EVERY * bufs_sec);
  if (redraw_every < 1)
    redraw_every = 1;

  bool changed = false;
  if (m_bufcount % (size_t) redraw_every == 0) {
    const size_t now = m_bufcount / (size_t) redraw_every;
    const size_t peak_hold_frames = std::max (1, (int) (VU_PEAK_HOLD * bufs_sec / redraw_every));
    const size_t clip_hold_frames = std::max (1, (int) (VU_CLIP_HOLD * bufs_sec / redraw_every));
    const size_t xrun_hold_frames = std::max (1, (int) (VU_XRUN_HOLD * bufs_sec / redraw_every));

    const float plus_l = m_plus_l.exchange (0.0f);
    const float minus_l = m_minus_l.exchange (0.0f);
    const float plus_r = m_plus_r.exchange (0.0f);
    const float minus_r = m_minus_r.exchange (0.0f);
    const float abs_l = clamp01 (std::max (std::fabs (plus_l), std::fabs (minus_l)));
    const float abs_r = clamp01 (std::max (std::fabs (plus_r), std::fabs (minus_r)));

    const float old_l = m_l.load ();
    const float old_r = m_r.load ();
    const float shown_l = db_scaled (old_l);
    const float shown_r = db_scaled (old_r);
    const float target_l = db_scaled (abs_l);
    const float target_r = db_scaled (abs_r);
    const float next_l = target_l >= shown_l
      ? target_l
      : std::max (target_l, shown_l - VU_FALL_SPEED);
    const float next_r = target_r >= shown_r
      ? target_r
      : std::max (target_r, shown_r - VU_FALL_SPEED);
    const float level_l = display_to_linear (next_l);
    const float level_r = display_to_linear (next_r);

    if (level_l != old_l || level_r != old_r)
      changed = true;

    m_l.store (level_l);
    m_r.store (level_r);

    if (now - m_timestamp_hold_l > peak_hold_frames)
      m_peak_l.store (0.0f);
    if (now - m_timestamp_hold_r > peak_hold_frames)
      m_peak_r.store (0.0f);

    if (abs_l > m_peak_l.load ()) {
      m_peak_l.store (abs_l);
      m_timestamp_hold_l = now;
      changed = true;
    }
    if (abs_r > m_peak_r.load ()) {
      m_peak_r.store (abs_r);
      m_timestamp_hold_r = now;
      changed = true;
    }

    if (abs_l > 0.999f)
      m_timestamp_clip_l = now;
    if (abs_r > 0.999f)
      m_timestamp_clip_r = now;
    if (xrun_l.load ())
      m_timestamp_xrun_l = now;
    if (xrun_r.load ())
      m_timestamp_xrun_r = now;

    const bool held_clip_l = m_timestamp_clip_l && now - m_timestamp_clip_l < clip_hold_frames;
    const bool held_clip_r = m_timestamp_clip_r && now - m_timestamp_clip_r < clip_hold_frames;
    const bool held_xrun_l = m_timestamp_xrun_l && now - m_timestamp_xrun_l < xrun_hold_frames;
    const bool held_xrun_r = m_timestamp_xrun_r && now - m_timestamp_xrun_r < xrun_hold_frames;

    if (held_clip_l != clip_l.load () ||
        held_clip_r != clip_r.load () ||
        held_xrun_l != xrun_l.load () ||
        held_xrun_r != xrun_r.load ())
      changed = true;

    clip_l.store (held_clip_l);
    clip_r.store (held_clip_r);
    xrun_l.store (held_xrun_l);
    xrun_r.store (held_xrun_r);
  }

  ++m_bufcount;
  if (changed)
    needs_redraw.store (true);

  return changed;
}

void c_vudata::set_db_scale (float db) {
  m_db_scale.store (db);
  needs_redraw.store (true);
}

void c_vudata::set_l (float level, float hold, bool clip, bool xrun) {
  m_l.store (clamp01 (level));
  m_peak_l.store (clamp01 (hold));
  clip_l.store (clip);
  xrun_l.store (xrun);
  needs_redraw.store (true);
}

void c_vudata::set_r (float level, float hold, bool clip, bool xrun) {
  m_r.store (clamp01 (level));
  m_peak_r.store (clamp01 (hold));
  clip_r.store (clip);
  xrun_r.store (xrun);
  needs_redraw.store (true);
}

float c_vudata::l () const {
  return db_scaled (m_l.load ());
}

float c_vudata::r () const {
  return db_scaled (m_r.load ());
}

float c_vudata::peak_l () const {
  return db_scaled (m_peak_l.load ());
}

float c_vudata::peak_r () const {
  return db_scaled (m_peak_r.load ());
}

void c_vudata::acknowledge () {
  clip_l.store (false);
  clip_r.store (false);
  xrun_l.store (false);
  xrun_r.store (false);
}
