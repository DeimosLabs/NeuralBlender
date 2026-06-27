
/* NeuralBlender - meter widget.
 * Original wxWidgets version was written for DIRT (Delt's Impulse
 * Response Tool) - see https://github.com/DeimosLabs/dirt
 *
 * Translated from wxWidgets to Cairo/xputty by Codex.
 */


#include "meter.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#ifndef METER_DATA_ONLY
#include <X11/Xlib.h>

#include "meter.h"

#include "xputty.h"
#include "xwidgets.h"
#endif

// why does xputty define this?
#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#ifdef CMDLINE_DEBUG
#include "cmdline/cmdline.h"
#define debug(...) cmdline_debug(stderr,ANSI_CYAN,__FILE__,__LINE__,__FUNC__,__VA_ARGS__)
#else
#define debug(...)
#define CP
#define BP
#endif

#ifdef METER_DATA_ONLY

////////////////////////////////////////////////////////////////////////////////
// c_vudata

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

void c_vudata::set_headroom (float db) {
  m_headroom_db.store (std::max (0.0f, db));
  needs_redraw.store (true);
}

/*float c_vudata::db_scaled (float f) const {
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
}*/

// new version: takes m_headroom_db into account
float c_vudata::db_scaled (float f) const {
  const float min_db = m_db_scale.load ();

  if (min_db >= 0.0f)
    return clamp01 (f);

  const float max_db = m_headroom_db.load ();
  if (max_db <= min_db)
    return clamp01 (f);

  const float db = linear_to_db_meter (f);
  return clamp01 ((db - min_db) / (max_db - min_db));
}


float c_vudata::display_to_linear (float f) const {
  const float min_db = m_db_scale.load ();
  f = clamp01 (f);

  if (min_db >= 0.0f)
    return f;

  const float max_db = m_headroom_db.load ();
  if (max_db <= min_db)
    return f;

  const float db = min_db + f * (max_db - min_db);
  return powf (10.0f, db / 20.0f);
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

  int redraw_every = (int) (redraw_interval * bufs_sec);
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
    const float abs_l = std::max (std::fabs (plus_l), std::fabs (minus_l));
    const float abs_r = std::max (std::fabs (plus_r), std::fabs (minus_r));

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
  m_l.store (std::max (0.0f, level));
  m_peak_l.store (std::max (0.0f, hold));
  clip_l.store (clip);
  xrun_l.store (xrun);
  needs_redraw.store (true);
}

void c_vudata::set_r (float level, float hold, bool clip, bool xrun) {
  m_r.store (std::max (0.0f, level));
  m_peak_r.store (std::max (0.0f, hold));
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

#else

#if 0

// EXAMPLE USAGE, FROM CODEX:

// Add members somewhere persistent, for example in c_neuralblender_ui or c_lane_widgets:

c_vudata meter_data;
c_meterwidget meter;

// Create it after you have a valid xputty parent Widget_t *:

meter.create(parent_widget, "Input", 20, 20, 180, 18);
meter.set_vudata(&meter_data);
meter.set_stereo(true);

// Feed values from your UI/audio-side bridge:

meter_data.set_l(0.45f, 0.70f, false);
meter_data.set_r(0.35f, 0.62f, false);

Then from your UI idle/timer path:

meter.on_ui_timer();

// For your current UI code, a quick test in c_neuralblender_ui::create() would look like:

// ui.h member:
c_vudata test_meter_data;
c_meterwidget test_meter;

// ui.cpp, after main_widget is created:
test_meter.create(main_widget, "Test", 20, 600, 600, 20);
test_meter.set_vudata(&test_meter_data);
test_meter.set_stereo(true);

test_meter_data.set_l(0.35f, 0.65f);
test_meter_data.set_r(0.55f, 0.80f);

// And in c_neuralblender_ui::idle():

test_meter.on_ui_timer();

// If you want vertical meters, just make the widget taller than it is wide:

test_meter.create(main_widget, "Out", 600, 80, 20, 200);


#endif

#ifdef OLD_METER_CODE

//#define DONT_USE_ANSI

#ifdef DONT_USE_ANSI

#ifdef DEBUG
#define ANSI_DUMMY //CP
#else
#define ANSI_DUMMY
#endif

static void print_vu_meter (float level, float hold,
                             bool clip, bool xrun)
                                            { printf ("level=%f, hold=%f\n", level, hold); }
//                                            { ANSI_DUMMY }

static void ansi_cursor_move_x (int n)                { ANSI_DUMMY }
static void ansi_cursor_move_to_x (int n)             { ANSI_DUMMY }
static void ansi_cursor_move_y (int n)                { ANSI_DUMMY }
static void ansi_cursor_hide ()                       { ANSI_DUMMY }
static void ansi_cursor_show ()                       { ANSI_DUMMY }
static void ansi_cursor_save ()                       { ANSI_DUMMY }
static void ansi_cursor_restore ()                    { ANSI_DUMMY }
static void ansi_clear_screen ()                      { ANSI_DUMMY }
static void ansi_clear_to_endl ()                     { ANSI_DUMMY }

// yep, i remember this...
#define FUCK char*//(char*)std::string("") //const

char *g_ansi_colors [32] = { NULL };
  //FUCK, FUCK, FUCK, FUCK, FUCK, FUCK, FUCK, FUCK, 
  //FUCK, FUCK, FUCK, FUCK, FUCK, FUCK, FUCK, FUCK, 
  //FUCK, FUCK, FUCK, FUCK, FUCK, FUCK, FUCK, FUCK, 
  //FUCK, FUCK, FUCK, FUCK, FUCK, FUCK, FUCK, FUCK };

#else

#ifndef __CMDLINE_H
char ANSI_BLACK [] =          "\x1B[0;30m";  //  0
char ANSI_DARK_RED [] =       "\x1B[0;31m";  //  1
char ANSI_DARK_GREEN [] =     "\x1B[0;32m";  //  2
char ANSI_DARK_YELLOW [] =    "\x1B[0;33m";  //  3
char ANSI_DARK_BLUE [] =      "\x1B[0;34m";  //  4
char ANSI_DARK_MAGENTA [] =   "\x1B[0;35m";  //  5
char ANSI_DARK_CYAN [] =      "\x1B[0;36m";  //  6
char ANSI_GREY [] =           "\x1B[0;37m";  //  7
char ANSI_DARK_GREY [] =      "\x1B[1;30m";  //  8
char ANSI_RED [] =            "\x1B[1;31m";  //  9
char ANSI_GREEN [] =          "\x1B[1;32m";  // 10
char ANSI_YELLOW [] =         "\x1B[1;33m";  // 11
char ANSI_BLUE [] =           "\x1B[1;34m";  // 12
char ANSI_MAGENTA [] =        "\x1B[1;35m";  // 13
char ANSI_CYAN [] =           "\x1B[1;36m";  // 14
char ANSI_WHITE [] =          "\x1B[1;37m";  // 15
char ANSI_RESET [] =          "\x1B[0m";
#endif

char *g_ansi_colors [] = {
  ANSI_BLACK,       ANSI_DARK_RED,       ANSI_DARK_GREEN,     ANSI_DARK_YELLOW,
  ANSI_DARK_BLUE,   ANSI_DARK_MAGENTA,   ANSI_DARK_CYAN,      ANSI_DARK_GREY,
  ANSI_GREY,        ANSI_RED,            ANSI_GREEN,          ANSI_YELLOW,
  ANSI_BLUE,        ANSI_MAGENTA,        ANSI_CYAN,           ANSI_WHITE,
  ANSI_RESET  
};

static void ansi_cursor_move_x (int n) {
  if (n == 0)
    printf ("\x1b[G"); // special case: start of line
  else if (n < 0)
    printf ("\x1b[%dD", -n); // left
  else
    printf ("\x1b[%dC", n); // right
}

static void ansi_cursor_move_y (int n) {
  if (n < 0)
    printf ("\x1b[%dA", -n);
  else if (n > 0)
    printf ("\x1b[%dB", n);
}

static void ansi_cursor_move_to_x (int n) { printf ("\x1b[%dG", n); }
static void ansi_clear_screen ()    { printf ("\x1b[2J");   }
static void ansi_clear_to_endl ()   { printf ("\x1b[K");    }
static void ansi_cursor_hide ()     { printf ("\x1b[?25l"); }
static void ansi_cursor_show ()     { printf ("\x1b[?25h"); }
static void ansi_cursor_save ()     { printf ("\x1b[s");    }
static void ansi_cursor_restore ()  { printf ("\x1b[u");    }

static void print_vu_meter (float level, float hold, bool clip, bool xrun) {
  ansi_clear_to_endl ();
  if (level < 0) level = 0;
  if (level > 1) level = 1;
  if (hold > 1) hold = 1;
  //debug ("level=%f hold=%f, %s, %s", level, hold,
  //       clip ? "clip" : "!clip", xrun ? "xrun" : "!xrun");
  //return;
  int i, size = ANSI_VU_METER_MIN_SIZE;
  char buf [size];
  for (i = 0; i < size; i++) buf [i] = ' ';
  char colors [size];
  for (i = 0; i < size; i++) colors [i] = 8;
  int right = size - 6;
  int yellow = (right * 2) / 3;
  int red = (right * 5) / 6;
  int n = (int) ((float) (level) * (float) (right));
  if (n > size) n = size;
  if (n < 0) n = 0;
  
  for (i = 1; i < n && i < yellow; i++)    { buf [i] = '='; colors [i] = 10; }
  for (; i < n && i < red; i++)            { buf [i] = '='; colors [i] = 11; }
  for (; i < n && i < right; i++)          { buf [i] = '='; colors [i] = 9; }
  for (i = n; i < yellow; i++)             { buf [i] = '-'; colors [i] = 2; }   
  for (; i < red; i++)                     { buf [i] = '-'; colors [i] = 3; }
  for (; i < right; i++)                   { buf [i] = '-'; colors [i] = 1; }
  //for (; i < size - 5; i++)  { buf [i] = '-'; colors [i] = 0x07; }
  int holdpos = (int) (hold * (float) right);
  if (holdpos > right - 1) holdpos = right - 1;
  if (holdpos > 0) { 
    if (holdpos > 0 && holdpos < right) {
      buf [holdpos] = (holdpos == right - 1) ? '!' : '|';
      colors [holdpos] = (holdpos == right - 1) ? 9 : 16;
    }
  }

  
  // lazyyyyyy... who cares
  if (xrun) {
    buf [right + 1] = 'X';
    buf [right + 2] = 'R';
    buf [right + 3] = 'U';
    buf [right + 4] = 'N';
  } else if (clip) {
    buf [right + 1] = 'C';
    buf [right + 2] = 'L';
    buf [right + 3] = 'I';
    buf [right + 4] = 'P';
  } else  {
    buf [right + 1] = ' ';
    buf [right + 2] = 'O';
    buf [right + 3] = 'K';
    buf [right + 4] = ' ';
    colors [right + 1] = 10;
    colors [right + 2] = 10;
    colors [right + 3] = 10;
    colors [right + 4] = 10;
  }
  if (xrun || clip) {
    colors [right + 1] = 9;
    colors [right + 2] = 9;
    colors [right + 3] = 9;
    colors [right + 4] = 9;
  }

  buf [0] = '[';
  buf [right] = ']';
  colors [0] = 16;
  colors [right] = 16;
  
  buf [size - 1] = 0;
  
  std::string output = ""; 
  int col = -1;
  for (i = 0; buf [i]; i++) {
    if (colors [i] != col) {
      output += g_ansi_colors [colors [i]];
      col = colors [i];
    }
    output += buf [i];
  }
  
  output += ANSI_RESET;
    
  //printf ("%s", buf);
  std::cout << output << " \n" << std::flush;
}

static void vu_wait (c_vudata &vu, std::string str) {
  ansi_cursor_move_x (0);
  int move_up = 2;
  print_vu_meter (vu.abs_l, vu.hold_l, vu.clip_l, vu.xrun);
  if (vu.is_stereo) {
    move_up++;
    print_vu_meter (vu.abs_r, vu.hold_r, vu.clip_r, vu.xrun);
  }
  ansi_clear_to_endl ();
  std::cout << str << std::endl;
  ansi_cursor_move_y (-1 * move_up);
  vu.acknowledge ();
  usleep (33333); 
}

#endif // DONT_USE_ANSI

#endif

static c_customwidget *custom_from_widget (void *w_) {
  Widget_t *w = (Widget_t *) w_;
  if (!w)
    return nullptr;

  return (c_customwidget *) w->parent_struct;
}

////////////////////////////////////////////////////////////////////////////////
// c_customwidget

void c_customwidget::create (Widget_t *parent_,
                             const char *label_,
                             int x, int y, int w, int h) {
  parent = parent_;
  label = label_ ? label_ : "";
  width = w;
  height = h;

  if (!parent || !parent->app)
    return;

  widget = create_widget (parent->app, parent, x, y, w, h);
  if (!widget)
    return;
  
  widget->label = label;
  widget->parent_struct = this;
  widget->scale.gravity = CENTER;
  widget->func.expose_callback = draw_cb;
  widget->func.enter_callback = enter_cb;
  widget->func.leave_callback = leave_cb;
  widget->func.button_press_callback = button_press_cb;
  widget->func.button_release_callback = button_release_cb;
  widget->func.motion_callback = motion_cb;
  widget->func.key_press_callback = key_press_cb;
  widget->func.key_release_callback = key_release_cb;
  widget->func.configure_notify_callback = configure_cb;

  for (int i = 0; i < 8; ++i)
    mousedown_x [i] = mousedown_y [i] = -16384;
}

void c_customwidget::set_opacity (int opacity_) {
  opacity = std::clamp (opacity_, 0, 255);
  invalidate_base ();
}

void c_customwidget::inspect () {
  debug ("widget=%p size=%dx%d", widget, width, height);
}

void c_customwidget::invalidate_base () {
  base_image_valid = false;
  expose ();
}

void c_customwidget::invalidate_overlay () {
  expose ();
}

void c_customwidget::invalidate_overlay_rect (int x, int y, int w, int h) {
  (void) x;
  (void) y;
  (void) w;
  (void) h;
  expose ();
}

void c_customwidget::expose () {
  if (widget)
    expose_widget (widget);
}

bool c_customwidget::button_left_down () const {
  return mouse_buttons & 0x01;
}

bool c_customwidget::button_middle_down () const {
  return mouse_buttons & 0x02;
}

bool c_customwidget::button_right_down () const {
  return mouse_buttons & 0x04;
}

bool c_customwidget::check_click_distance (int which) const {
  if (which < 0 || which >= 8)
    return false;

  return std::abs (mouse_x - mousedown_x [which]) <= click_distance &&
         std::abs (mouse_y - mousedown_y [which]) <= click_distance;
}

void c_customwidget::sync_metrics () {
  if (!widget)
    return;

  Metrics_t metrics;
  os_get_window_metrics (widget, &metrics);
  if (metrics.width != width || metrics.height != height) {
    width = metrics.width;
    height = metrics.height;
    base_image_valid = false;
    on_resize (width, height);
  }
  visible = metrics.visible;
}

void c_customwidget::render_base (cairo_t *cr) {
  cairo_set_source_rgb (cr, 0.08, 0.08, 0.08);
  cairo_paint (cr);

  cairo_set_source_rgb (cr, 0.55, 0.55, 0.55);
  cairo_move_to (cr, 0, 0);
  cairo_line_to (cr, width, height);
  cairo_move_to (cr, width, 0);
  cairo_line_to (cr, 0, height);
  cairo_stroke (cr);
}

void c_customwidget::render_overlay (cairo_t *cr) {
  (void) cr;
}

void c_customwidget::draw_cb (void *w_, void *userdata) {
  (void) userdata;
  Widget_t *w = (Widget_t *) w_;
  c_customwidget *cw = custom_from_widget (w_);
  if (!w || !cw || !w->crb)
    return;

  cw->sync_metrics ();
  if (cw->width <= 0 || cw->height <= 0)
    return;

  cairo_save (w->crb);
  cairo_set_operator (w->crb, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (w->crb, 0.0, 0.0, 0.0, cw->opacity / 255.0);
  cairo_paint (w->crb);
  cairo_set_operator (w->crb, CAIRO_OPERATOR_OVER);
  cw->render_base (w->crb);
  cw->on_paint (w->crb);
  cw->render_overlay (w->crb);
  cairo_restore (w->crb);

  cw->base_image_valid = true;
}

void c_customwidget::configure_cb (void *w_, void *userdata) {
  (void) userdata;
  Widget_t *w = (Widget_t *) w_;
  c_customwidget *cw = custom_from_widget (w_);
  if (!w || !cw)
    return;

  cw->sync_metrics ();

  Metrics_t metrics;
  os_get_window_metrics (w, &metrics);
  if (w->width == metrics.width && w->height == metrics.height)
    cw->expose ();
}

void c_customwidget::leave_cb (void *w_, void *userdata) {
  (void) userdata;
  c_customwidget *cw = custom_from_widget (w_);
  if (!cw)
    return;

  cw->mouse_x = -1;
  cw->mouse_y = -1;
  cw->on_mouseleave ();
  cw->expose ();
}

void c_customwidget::button_press_cb (void *w_, void *event_, void *userdata) {
  (void) userdata;
  c_customwidget *cw = custom_from_widget (w_);
  XEvent *event = (XEvent *) event_;
  if (!cw || !event)
    return;

  const int button = event->xbutton.button;
  const int idx = button == Button1 ? 0 : button == Button2 ? 1 : button == Button3 ? 2 : -1;
  cw->mouse_x = event->xbutton.x;
  cw->mouse_y = event->xbutton.y;

  if (idx >= 0) {
    cw->mousedown_x [idx] = cw->mouse_x;
    cw->mousedown_y [idx] = cw->mouse_y;
    cw->mouse_buttons |= (1 << idx);
    cw->on_mousedown (idx);
  }

  if (button == Button1)
    cw->on_mousedown_left ();
  else if (button == Button2)
    cw->on_mousedown_middle ();
  else if (button == Button3)
    cw->on_mousedown_right ();
  else if (button == Button4)
    cw->on_mousewheel_v (1);
  else if (button == Button5)
    cw->on_mousewheel_v (-1);
  else if (button == 6)
    cw->on_mousewheel_h (1);
  else if (button == 7)
    cw->on_mousewheel_h (-1);

  cw->expose ();
}

void c_customwidget::button_release_cb (void *w_, void *event_, void *userdata) {
  (void) userdata;
  c_customwidget *cw = custom_from_widget (w_);
  XEvent *event = (XEvent *) event_;
  if (!cw || !event)
    return;

  const int button = event->xbutton.button;
  const int idx = button == Button1 ? 0 : button == Button2 ? 1 : button == Button3 ? 2 : -1;
  cw->mouse_x = event->xbutton.x;
  cw->mouse_y = event->xbutton.y;

  if (idx >= 0) {
    cw->mouse_buttons &= ~(1 << idx);
    cw->on_mouseup (idx);
  }

  if (button == Button1)
    cw->on_mouseup_left ();
  else if (button == Button2)
    cw->on_mouseup_middle ();
  else if (button == Button3)
    cw->on_mouseup_right ();

  cw->expose ();
}

void c_customwidget::motion_cb (void *w_, void *event_, void *userdata) {
  (void) userdata;
  c_customwidget *cw = custom_from_widget (w_);
  XEvent *event = (XEvent *) event_;
  if (!cw || !event)
    return;

  cw->mouse_x = event->xmotion.x;
  cw->mouse_y = event->xmotion.y;
  cw->on_mousemove (cw->mouse_x, cw->mouse_y);
}

void c_customwidget::key_press_cb (void *w_, void *event_, void *userdata) {
  (void) userdata;
  c_customwidget *cw = custom_from_widget (w_);
  XEvent *event = (XEvent *) event_;
  if (!cw || !event)
    return;

  cw->on_keydown ((int) event->xkey.keycode, false);
}

void c_customwidget::key_release_cb (void *w_, void *event_, void *userdata) {
  (void) userdata;
  c_customwidget *cw = custom_from_widget (w_);
  XEvent *event = (XEvent *) event_;
  if (!cw || !event)
    return;

  cw->on_keyup ((int) event->xkey.keycode);
}

void c_customwidget::enter_cb (void *w_, void *userdata) {
  (void) userdata;
  c_customwidget *cw = custom_from_widget (w_);
  if (!cw)
    return;

  cw->on_visible ();
}

////////////////////////////////////////////////////////////////////////////////
// c_testwidget

void c_testwidget::render_base (cairo_t *cr) {
  cairo_set_source_rgb (cr, 0.1, 0.1, 0.1);
  cairo_paint (cr);
  cairo_set_source_rgb (cr, 0.7, 0.7, 0.7);
  cairo_move_to (cr, 0, 0);
  cairo_line_to (cr, width, height);
  cairo_move_to (cr, width, 0);
  cairo_line_to (cr, 0, height);
  cairo_stroke (cr);
}

void c_testwidget::on_paint (cairo_t *cr) {
  const char *msg = "c_customwidget";
  cairo_text_extents_t extents;
  cairo_select_font_face (cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size (cr, 13.0);
  cairo_text_extents (cr, msg, &extents);
  cairo_set_source_rgb (cr, 0.85, 0.85, 0.85);
  cairo_move_to (cr,
                 (width - extents.width) * 0.5 - extents.x_bearing,
                 (height - extents.height) * 0.5 - extents.y_bearing);
  cairo_show_text (cr, msg);
}

////////////////////////////////////////////////////////////////////////////////
// c_meterwidget

void c_meterwidget::create (Widget_t *parent,
                            const char *label,
                            int x, int y, int w, int h) {
  c_customwidget::create (parent, label, x, y, w, h);
  set_db_scale (db_scale);
  update_geometry ();
}

void c_meterwidget::set_db_scale (float f) {
  db_scale = f;
  if (data)
    data->set_db_scale (f);
}

void c_meterwidget::set_headroom (float f) {
  headroom = std::max (0.0f, f);
  if (data)
    data->set_headroom (headroom);
}

void c_meterwidget::set_vudata (c_vudata *v) {
  data = v;
  set_db_scale (db_scale);
  set_headroom (headroom);
}

c_vudata *c_meterwidget::get_vudata () {
  return data;
}

bool c_meterwidget::needs_redraw () {
  return data && data->needs_redraw.exchange (false);
}

void c_meterwidget::on_ui_timer () {
/*  if (needs_redraw ())
    expose ();*/
  
  if (needs_redraw () && widget)
    transparent_draw (widget, NULL);
  
}

void c_meterwidget::set_stereo (bool b) { CP
  stereo = b;
  invalidate_base ();
}

void c_meterwidget::set_l (float level, float hold, bool clip, bool xrun) {
  if (!data)
    return;
  data->set_l (level, hold, clip, xrun);
}

void c_meterwidget::set_r (float level, float hold, bool clip, bool xrun) {
  if (!data)
    return;
  data->set_r (level, hold, clip, xrun);
}

void c_meterwidget::on_resize (int w, int h) {
  width = w;
  height = h;
  update_geometry ();
  invalidate_base ();
}

void c_meterwidget::update_geometry () {
  if (width <= 0 || height <= 0)
    return;

  vertical = width < height;
  ln = vertical ? height : width;
  th = vertical ? width : height;
  met_len = ln - clip_size - rec_size;
  if (met_len < 1)
    met_len = 1;

  if (stereo) {
    t1 = 0;
    int gap = th / 50;
    if (gap < 1)
      gap = 1;
    t2 = (th / 2) - gap;
    t4 = th;
    t3 = th - t2;
    tp = th / 20;
    if (tp <= 1)
      tp = 1;
  } else {
    t1 = (int) (th * 0.1);
    t2 = th - t1;
    t3 = -1;
    t4 = -1;
    tp = (th / 20) + 1;
  }
}

void c_meterwidget::render_base (cairo_t *cr) {
  update_geometry ();

  cairo_set_source_rgb (cr, 0.05, 0.05, 0.05);
  cairo_paint (cr);

  cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
  if (vertical) {
    cairo_rectangle (cr, t1, clip_size, t2 - t1, met_len);
    if (stereo)
      cairo_rectangle (cr, t3, clip_size, t4 - t3, met_len);
  } else {
    cairo_rectangle (cr, rec_size, t1, met_len, t2 - t1);
    if (stereo)
      cairo_rectangle (cr, rec_size, t3, met_len, t4 - t3);
  }
  cairo_fill (cr);

  struct meter_line {
    float pos;
    double r;
    double g;
    double b;
    double a;
  };

  const meter_line lines [] = {
    { 0.5f,   0.0, 1.0, 0.0, 0.25 },
    { 0.75f,  1.0, 1.0, 0.0, 0.25 },
    { 0.875f, 1.0, 0.5, 0.0, 0.25 },
    { 1.0f,   1.0, 0.0, 0.0, 0.25 },
  };

  cairo_set_line_width (cr, 1.0);
  for (const auto &line : lines) {
    cairo_set_source_rgba (cr, line.r, line.g, line.b, line.a);
    if (vertical) {
      int ppos = (int) (met_len * line.pos);
      int y = height - ppos - rec_size;
      cairo_move_to (cr, t1, y + 0.5);
      cairo_line_to (cr, t2, y + 0.5);
      if (stereo) {
        cairo_move_to (cr, t3, y + 0.5);
        cairo_line_to (cr, t4, y + 0.5);
      }
    } else {
      int x = rec_size + (int) (met_len * line.pos);
      cairo_move_to (cr, x + 0.5, t1);
      cairo_line_to (cr, x + 0.5, t2 - 1);
      if (stereo) {
        cairo_move_to (cr, x + 0.5, t3);
        cairo_line_to (cr, x + 0.5, t4 - 1);
      }
    }
    cairo_stroke (cr);
  }
}

void c_meterwidget::draw_bar (cairo_t *cr, int at, int bar_th, float level, float hold) {
  level = std::clamp (level, 0.0f, 1.0f);
  hold = std::clamp (hold, 0.0f, 1.0f);

  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
  int bar_len = (int) (met_len * level);

  if (bar_len > 0) {
    if (vertical) {
      const int meter_bottom = height - rec_size;
      x = at + tp;
      y = meter_bottom - bar_len + tp;
      w = bar_th - (tp * 2);
      h = bar_len - (tp * 2);
    } else {
      x = rec_size + tp;
      y = at + tp;
      w = bar_len - (tp * 2);
      h = bar_th - (tp * 2);
    }

    if (w > 0 && h > 0) {
      cairo_pattern_t *gradient = nullptr;
      if (vertical)
        gradient = cairo_pattern_create_linear (0, height - rec_size, 0, clip_size);
      else
        gradient = cairo_pattern_create_linear (rec_size, 0, width - clip_size, 0);

      cairo_pattern_add_color_stop_rgb (gradient, 0.00, 0.0, 0.9, 0.0);
      cairo_pattern_add_color_stop_rgb (gradient, 0.66, 1.0, 1.0, 0.0);
      cairo_pattern_add_color_stop_rgb (gradient, 1.00, 1.0, 0.0, 0.0);
      cairo_rectangle (cr, x, y, w, h);
      cairo_set_source (cr, gradient);
      cairo_fill (cr);
      cairo_pattern_destroy (gradient);
    }
  }

  //const int holdpos = (int) (hold * met_len);
  if (hold > 0) {
    int holdpos = (int) (hold * met_len);
    holdpos = std::clamp(holdpos, 1, met_len);

    cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.75);
    cairo_set_line_width (cr, 1.0);
    if (vertical) {
      const double py = height - rec_size - holdpos + 0.5;
      cairo_move_to (cr, at, py);
      cairo_line_to (cr, at + bar_th, py);
    } else {
      const double px = rec_size + holdpos + 0.5;
      cairo_move_to (cr, px, at);
      cairo_line_to (cr, px, at + bar_th);
    }
    cairo_stroke (cr);
  }
}

void c_meterwidget::draw_warning_text (cairo_t *cr, const char *text,
                                       double x, double y,
                                       double w, double h) {
  cairo_save (cr);
  cairo_select_font_face (cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size (cr, std::max (8.0, h * 0.55));

  cairo_text_extents_t extents;
  cairo_text_extents (cr, text, &extents);
  cairo_set_source_rgb (cr, 0.9, 0.0, 0.0);
  cairo_move_to (cr,
                 x + (w - extents.width) * 0.5 - extents.x_bearing,
                 y + (h - extents.height) * 0.5 - extents.y_bearing);
  cairo_show_text (cr, text);
  cairo_restore (cr);
}

void c_meterwidget::on_paint (cairo_t *cr) {
  if (!data)
    return;

  if (stereo) {
    draw_bar (cr, t1, t2 - t1, data->l (), data->peak_l ());
    draw_bar (cr, t3, t4 - t3, data->r (), data->peak_r ());
  } else {
    draw_bar (cr, t1, t2 - t1, data->l (), data->peak_l ());
  }

  const bool clipped = data->clip_l.load () || data->clip_r.load ();
  const bool xruns = data->xrun_l.load () || data->xrun_r.load ();

  if (clipped) {
    cairo_set_source_rgb (cr, 0.9, 0.0, 0.0);
    if (clip_size > 0) {
      if (vertical)
        cairo_rectangle (cr, 0, 0, width, clip_size);
      else
        cairo_rectangle (cr, width - clip_size, 0, clip_size, height);
      cairo_fill (cr);
    } else {
      draw_warning_text (cr, "CLIP", width * 0.72, 0, width * 0.28, height);
    }
  }

  if (xruns)
    draw_warning_text (cr, "XRUN", width * 0.36, 0, width * 0.28, height);

  if (rec_enabled) {
    cairo_set_source_rgb (cr, 0.9, 0.0, 0.0);
    if (rec_size > 0) {
      const int rp = (vertical ? width : height) / 4;
      if (vertical)
        cairo_arc (cr, width * 0.5, height - rec_size * 0.5,
                   std::max (1, width / 2 - rp), 0, 2.0 * M_PI);
      else
        cairo_arc (cr, rec_size * 0.5, height * 0.5,
                   std::max (1, height / 2 - rp), 0, 2.0 * M_PI);
      cairo_fill (cr);
    } else {
      draw_warning_text (cr, "REC", 0, 0, width * 0.22, height);
    }
  }

  data->acknowledge ();
}

/* More info from Codex about LV2 dsp/ui sync:


› how do plugins normally get peak data across from the DSP side to UI side of a LV2 plugin?


• Normally: an LV2 atom output port from DSP to UI, same general mechanism you’re already using for model path
  notifications.

  Typical pattern:

  1. DSP computes cheap meter values in run()
     Peak/RMS values are accumulated per audio block or over a small interval.

  2. DSP periodically writes a small atom message to an output atom:AtomPort
     Usually not every sample and often not every block. Something like 20-60 Hz is enough.

  3. UI receives it in port_event()
     The UI decodes the atom and updates meter state.

  For meters, you generally do not use normal LV2 control ports, because those are plugin parameters and hosts may
  automate/store/display them. Meter values are transient telemetry.

  A common message shape is patch:Set with a custom property:

  nb:MeterA
      a lv2:Parameter ;
      rdfs:range atom:Vector .

  Then DSP sends maybe four floats:

  [l_peak, r_peak, l_hold, r_hold]

  or for your lanes:

  [lane_index, in_peak, out_peak, clip]

  In C terms, from run() you’d forge something like:

  lv2_atom_forge_frame_time(&forge, 0);
  lv2_atom_forge_object(&forge, &frame, 0, urid_patch_Set);

  lv2_atom_forge_key(&forge, urid_patch_property);
  lv2_atom_forge_urid(&forge, urid_meter);

  lv2_atom_forge_key(&forge, urid_patch_value);
  lv2_atom_forge_vector(&forge, sizeof(float), urid_atom_Float, n_values, values);

  lv2_atom_forge_pop(&forge, &frame);

  Then in the UI:

  if (property == urid_meter && value->type == urid_atom_Vector) {
    const LV2_Atom_Vector *vec = (const LV2_Atom_Vector *) value;
    const float *f = (const float *) LV2_ATOM_BODY(&vec->body);
    meter_data.set_l(f[0], f[1], ...);
  }

  Practical rules:

  - Send meters at a throttled rate, e.g. every 1024-4096 samples, not every block if block size is tiny.
  - Keep messages tiny and fixed-layout.
  - If the atom forge write fails, just drop that meter frame.
  - Never block, allocate, or lock for meter reporting.
  - Don’t send peak data while UI is closed unless needed; although in LV2, often the host just ignores/discards UI
    notification data if no UI is listening.

  For NeuralBlender, I’d probably add a second custom atom notification property for all meter data, separate from
  model path messages, and send one compact vector:

  master_in_l, master_in_r,
  lane0_out_l, lane0_out_r,
  lane1_out_l, lane1_out_r,
  lane2_out_l, lane2_out_r,
  lane3_out_l, lane3_out_r,
  master_out_l, master_out_r

  Or start simpler:

  input_peak, output_peak

  Then expand once the drawing is stable.


› yeah i try to keep edits to the .ttl to a minimum, since it's a language i don't really fully understand... btw
  what marks these ports as "transient" and not "state data" to the host?


• For LV2, the main thing is: don’t expose meter values as lv2:ControlPorts / plugin parameters.

  If you send them as atom messages on an atom:AtomPort, they are just runtime events. The host does not treat those
  as saved state unless you explicitly model them as state/parameters and store them.

  For .ttl, the transient-ish part is usually implied by structure:

  [
      a lv2:OutputPort , atom:AtomPort ;
      atom:bufferType atom:Sequence ;
      atom:supports patch:Message ;
      lv2:index 20 ;
      lv2:symbol "NOTIFY" ;
      lv2:name "Notify"
  ]

  That port is an event stream, not a parameter. Ardour/jalv won’t save “last atom message” as plugin state.

  What becomes state/parameter-like is:

  - lv2:ControlPort with lv2:InputPort
  - things declared as lv2:Parameter and used with patch:Set for persistent properties
  - values your save() callback stores via LV2_State_Store_Function

  Your model paths are state because you explicitly store them in save():

  store(handle, self->urid_model[i], stored_path, ...);

  A meter atom property would not be saved unless you add it there.

  There is also a port property for meters:

  lv2:portProperty lv2:reportsLatency

  No, wrong one for meters. For control ports, plugins often use:

  lv2:portProperty lv2:notOnGUI

  or plugin-specific conventions, but that’s for hiding/display hints, not persistence.

  For an LV2 meter as a control output, you might see:

  a lv2:OutputPort , lv2:ControlPort ;
  lv2:portProperty lv2:connectionOptional ;

  Some ecosystems also use pprops:notOnGUI, lv2:designation, units, etc. But output control ports are still
  “reported values”, not usually saved state. The host may display them generically though, which you probably don’t
  want for a bunch of fast meter values.

  So for your case: if the meters go over the existing NOTIFY atom output port, and you do not include them in
  save(), they are transient runtime telemetry.


  STANDALONE VERSION:
  
  For standalone, the simplest path is:

  - DSP thread owns/updates a c_vudata or one per meter.
  - UI thread calls meter.on_ui_timer() from c_neuralblender_ui::idle().
  - c_vudata uses atomics, so the UI can read without locks.

  Something like:

  // standalone UI class members
  c_vudata input_vu;
  c_meterwidget input_meter;

  Create:

  input_meter.create(main_widget, "Input", 20, 600, 600, 18);
  input_meter.set_vudata(&input_vu);

  DSP callback:

  for (uint32_t i = 0; i < nframes; ++i)
    input_vu.sample(input[i], input[i]);

  input_vu.update();

  UI idle:

  c_neuralblender_ui::idle();
  input_meter.on_ui_timer();

  That gives you the full meter pipeline without involving LV2 atoms yet. Once it looks right, the LV2 version is
  just replacing the shared c_vudata * with decoded atom messages into UI-owned c_vudata.

*/

#endif // METER_DATA_ONLY
