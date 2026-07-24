
/* NeuralBlender - meter widget.
 * Original wxWidgets version was written for DIRT (Delt's Impulse
 * Response Tool) - see https://github.com/DeimosLabs/dirt
 *
 * Translated from wxWidgets to Cairo by Codex.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <atomic>

//#ifndef METER_DATA_ONLY
//#include "ui.h"
//#endif

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#define DEFAULT_VU_DB       -40.0f
#define DEFAULT_VU_HEADROOM 3.0f
#define VU_REDRAW_EVERY     0.033333333f
#define VU_PEAK_HOLD        1.000000000f
#define VU_CLIP_HOLD        1.000000000f
#define VU_XRUN_HOLD        1.000000000f
#define VU_FALL_SPEED       0.020000000f

typedef struct _cairo cairo_t;
//struct cairo_t;
class c_neuralblender_ui;

enum class meterwarn {
  REC,
  CLIP,
  XRUN,
  MAX
};

class c_vudata {
public:
  bool sample (float l, float r);
  bool update ();
  void set_db_scale (float db);
  void set_l (float level, float hold, bool clip = false, bool xrun = false);
  void set_r (float level, float hold, bool clip = false, bool xrun = false);
  void set_l_smooth (float level, float hold, bool clip = false, bool xrun = false);
  void set_r_smooth (float level, float hold, bool clip = false, bool xrun = false);
  void set_display_l (float level, float hold, bool clip = false, bool xrun = false);
  void set_display_r (float level, float hold, bool clip = false, bool xrun = false);
  void set_headroom (float db);

  float l () const;
  float r () const;
  float peak_l () const;
  float peak_r () const;
  float display_l () const { return m_display_l.load (); }
  float display_r () const { return m_display_r.load (); }
  float display_peak_l () const { return db_scaled (m_peak_l.load ()); }
  float display_peak_r () const { return db_scaled (m_peak_r.load ()); }
  float linear_l () const { return m_l.load (); }
  float linear_r () const { return m_r.load (); }
  float linear_peak_l () const { return m_peak_l.load (); }
  float linear_peak_r () const { return m_peak_r.load (); }
  void acknowledge ();

  std::atomic<bool> needs_redraw { false };
  std::atomic<bool> clip_l { false };
  std::atomic<bool> clip_r { false };
  std::atomic<bool> xrun_l { false };
  std::atomic<bool> xrun_r { false };
  float redraw_interval = VU_REDRAW_EVERY;
  int samplerate = 48000;
  int bufsize = 128;

private:
  static float clamp01 (float f);
  static void atomic_max (std::atomic<float> &dst, float value);
  static void atomic_min (std::atomic<float> &dst, float value);
  float db_scaled (float f) const;
  float display_to_linear (float f) const;

  std::atomic<float> m_l { 0.0f };
  std::atomic<float> m_r { 0.0f };
  std::atomic<float> m_display_l { 0.0f };
  std::atomic<float> m_display_r { 0.0f };
  std::atomic<float> m_peak_l { 0.0f };
  std::atomic<float> m_peak_r { 0.0f };
  std::atomic<float> m_db_scale { DEFAULT_VU_DB };
  std::atomic<float> m_headroom_db { DEFAULT_VU_HEADROOM };
  std::atomic<float> m_plus_l { 0.0f };
  std::atomic<float> m_plus_r { 0.0f };
  std::atomic<float> m_minus_l { 0.0f };
  std::atomic<float> m_minus_r { 0.0f };
  size_t m_bufcount = 0;
  size_t m_timestamp_hold_l = 0;
  size_t m_timestamp_hold_r = 0;
  size_t m_timestamp_clip_l = 0;
  size_t m_timestamp_clip_r = 0;
  size_t m_timestamp_xrun_l = 0;
  size_t m_timestamp_xrun_r = 0;
};
