
/*
 * NeuralBlender - meter widget.
 * Original wxWidgets version was written for DIRT (Delt's Impulse
 * Response Tool) - see https://github.com/DeimosLabs/dirt
 *
 * Translated from wxWidgets to Cairo/xputty by Codex.
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

//#include "xputty.h"
//#include "xwidgets.h"

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#define DEFAULT_VU_DB   -36.0f
#define VU_REDRAW_EVERY 0.033333333f
#define VU_PEAK_HOLD    1.000000000f
#define VU_CLIP_HOLD    1.000000000f
#define VU_XRUN_HOLD    1.000000000f
#define VU_FALL_SPEED   0.030000000f

typedef struct _cairo cairo_t;
//struct cairo_t;
class Widget_t;

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

  float l () const;
  float r () const;
  float peak_l () const;
  float peak_r () const;
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
  std::atomic<float> m_peak_l { 0.0f };
  std::atomic<float> m_peak_r { 0.0f };
  std::atomic<float> m_db_scale { DEFAULT_VU_DB };
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

class c_customwidget {
public:
  c_customwidget () = default;
  virtual ~c_customwidget () = default;

  virtual void create (Widget_t *parent,
                       const char *label,
                       int x, int y, int w, int h);

  virtual void set_opacity (int opacity);
  virtual void inspect ();
  virtual void on_ui_timer () {}
  void invalidate_base ();
  void invalidate_overlay ();
  void invalidate_overlay_rect (int x, int y, int w, int h);
  void expose ();

  Widget_t *widget = nullptr;
  Widget_t *parent = nullptr;
  const char *label = "";
  int width = 0;
  int height = 0;

protected:
  virtual void render_base (cairo_t *cr);
  virtual void render_overlay (cairo_t *cr);
  virtual void on_resize (int w, int h) {}
  virtual void on_mousemove (int x, int y) {}
  virtual void on_mousedown (int which) {}
  virtual void on_mouseup (int which) {}
  virtual void on_mousedown_left () {}
  virtual void on_mouseup_left () {}
  virtual void on_mousedown_middle () {}
  virtual void on_mouseup_middle () {}
  virtual void on_mousedown_right () {}
  virtual void on_mouseup_right () {}
  virtual void on_mouseleave () {}
  virtual void on_mousewheel_v (int howmuch) {}
  virtual void on_mousewheel_h (int howmuch) {}
  virtual void on_keydown (int keycode, bool is_repeat) {}
  virtual void on_keyup (int keycode) {}
  virtual void on_visible () {}
  virtual void on_paint (cairo_t *cr) {}

  bool button_left_down () const;
  bool button_middle_down () const;
  bool button_right_down () const;
  bool check_click_distance (int which_button) const;

  int opacity = 255;
  int mouse_x = -1;
  int mouse_y = -1;
  int mousedown_x [8] = { 0 };
  int mousedown_y [8] = { 0 };
  int mouse_buttons = 0;
  int click_distance = 5;
  bool base_image_valid = false;
  bool visible = true;

private:
  void sync_metrics ();

  static void draw_cb (void *w, void *userdata);
  static void configure_cb (void *w, void *userdata);
  static void enter_cb (void *w, void *userdata);
  static void leave_cb (void *w, void *userdata);
  static void button_press_cb (void *w, void *event, void *userdata);
  static void button_release_cb (void *w, void *event, void *userdata);
  static void motion_cb (void *w, void *event, void *userdata);
  static void key_press_cb (void *w, void *event, void *userdata);
  static void key_release_cb (void *w, void *event, void *userdata);
};

class c_testwidget : public c_customwidget {
protected:
  void render_base (cairo_t *cr) override;
  void on_paint (cairo_t *cr) override;
};

class c_meterwidget : public c_customwidget {
public:
  void create (Widget_t *parent,
               const char *label,
               int x, int y, int w, int h) override;

  void set_db_scale (float f);
  void set_vudata (c_vudata *v);
  c_vudata *get_vudata ();
  bool needs_redraw ();
  void on_ui_timer () override;

  void set_stereo (bool b);
  void set_l (float level, float hold, bool clip = false, bool xrun = false);
  void set_r (float level, float hold, bool clip = false, bool xrun = false);

  bool vertical = false;
  int clip_size = 0;
  int rec_size = 0;
  bool rec_enabled = false;
  c_vudata *data = nullptr;
  float db_scale = DEFAULT_VU_DB;

protected:
  void render_base (cairo_t *cr) override;
  void on_paint (cairo_t *cr) override;
  void on_resize (int w, int h) override;

private:
  void draw_bar (cairo_t *cr, int at, int th, float level, float hold);
  void draw_warning_text (cairo_t *cr, const char *text, double x, double y,
                          double w, double h);
  void update_geometry ();

  bool stereo = true;
  int met_len = -1;
  int ln = 0;
  int th = 0;
  int t1 = 0;
  int t2 = 0;
  int t3 = 0;
  int t4 = 0;
  int tp = 1;
};
