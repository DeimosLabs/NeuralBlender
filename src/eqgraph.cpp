

#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstring>

#include "eqgraph.h"

#define CMDLINE_DEBUG_COLOR ANSI_DARK_BLUE
#include "cmdline_debug.h"

c_eqgraph::c_eqgraph () { CP }
c_eqgraph::~c_eqgraph () {
  CP
  clear_curve_surface ();
}

void c_eqgraph::render_base (cairo_t *cr) {
  int i;

  cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);

  cairo_rectangle (cr, 0, 0, w, h);
  cairo_set_source_rgba (cr, 0, 0, 0.05, 1);
  cairo_fill (cr);
  for (i = 20; i <= 20000;) {
    int x = freq_to_x ((float) i);
    if (x == w) x--;
    cairo_move_to (cr, x, 0);
    cairo_line_to (cr, x, h);
    if (i < 100)
      i += 10;
    else if (i < 1000)
      i += 100;
    else if (i < 10000)
      i += 1000;
    else
      i += 10000;
  }
  for (i = 0; i < 36; i += 12) {
    int y = db_to_y (i);
    cairo_move_to (cr, 0, y);
    cairo_line_to (cr, w, y);
    y = db_to_y (-1 * i);
    if (y == h) y--;
    cairo_move_to (cr, 0, y);
    cairo_line_to (cr, w, y);
  }
  cairo_set_line_width (cr, 0.5f);
  cairo_set_source_rgba (cr, 0, 0, 0.5, 1);
  cairo_stroke (cr);

  int x = freq_to_x (1000);
  cairo_move_to (cr, x, 0);
  cairo_line_to (cr, x, h);
  cairo_set_source_rgba (cr, 0, 0, 0.75, 1);
  cairo_stroke (cr);

  int y = db_to_y (0);
  cairo_move_to (cr, 0, y);
  cairo_line_to (cr, w, y);
  cairo_set_source_rgba (cr, 1, 1, 0, 1);
  cairo_set_line_width (cr, 1);
  cairo_stroke (cr);
}

void c_eqgraph::set_state (c_eq_state *state_) {
  state = state_;
  state_changed ();
}

void c_eqgraph::state_changed () {
  curves_dirty = true;
  invalidate ();
}

void c_eqgraph::set_samplerate (int sr) {
  samplerate = sr;
  state_changed ();
}

void c_eqgraph::on_paint (cairo_t *cr) {
  if (!state)
    return;
  
  const uint64_t now = nbtk::now_ms ();
  if (!curve_surface ||
      (curves_dirty && now - curves_last_ms >= CURVE_RECALC_MS)) {
    generate_curves ();
    render_curve_surface ();
    curves_last_ms = now;
    curves_dirty = false;
  }

  if (!curve_surface)
    return;

  cairo_set_source_surface (cr, curve_surface, 0, 0);
  cairo_paint (cr);

  if (mouse_handle_x >= 0 && mouse_handle_y >= 0) {
    cairo_set_line_width (cr, 2.0f);
    cairo_set_source_rgba (cr, 1.0, 1.0, 0.0, 0.25);
    cairo_rectangle (cr,
                     mouse_handle_x - handle_size / 2,
                     mouse_handle_y - handle_size / 2,
                     handle_size, handle_size);
    cairo_fill (cr);
  }
}

void c_eqgraph::render_curve_surface () {
  if (!ensure_curve_surface () || !curve_cr)
    return;

  cairo_save (curve_cr);
  cairo_set_operator (curve_cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (curve_cr);
  cairo_restore (curve_cr);

  if (!state || curve.empty ()) {
    cairo_surface_flush (curve_surface);
    return;
  }

  cairo_set_line_width (curve_cr, 2.0f);
  cairo_set_source_rgba (curve_cr, 1, 1, 1, 1);
  path_curve (curve_cr, curve);
  cairo_stroke (curve_cr);
  cairo_set_line_width (curve_cr, 1.0f);

  for (int i = 0; i < EQ_NUM_BANDS; i++) {
    if (state->enabled [i]) {
      int x = freq_to_x (state->freq [i]);
      int y = db_to_y (state->gain_db [i]);
      
      cairo_rectangle (curve_cr,
                       0.5 + x - handle_size / 2,
                       0.5 + y - handle_size / 2,
                       handle_size, handle_size);
      
      if (i == drag_handle)
        cairo_set_source_rgba (curve_cr, 1, 1, 0, 1);
      else
        cairo_set_source_rgba (curve_cr, 0.7, 1.0, 0.7, 1);

      cairo_stroke (curve_cr);
    }
  }
  

  cairo_surface_flush (curve_surface);
}

void c_eqgraph::clear_curve_surface () {
  if (curve_cr) {
    cairo_destroy (curve_cr);
    curve_cr = NULL;
  }

  if (curve_surface) {
    cairo_surface_destroy (curve_surface);
    curve_surface = NULL;
  }

  curve_surface_w = 0;
  curve_surface_h = 0;
}

bool c_eqgraph::ensure_curve_surface () {
  if (w <= 0 || h <= 0)
    return false;

  if (curve_surface &&
      curve_cr &&
      curve_surface_w == w &&
      curve_surface_h == h &&
      cairo_surface_status (curve_surface) == CAIRO_STATUS_SUCCESS &&
      cairo_status (curve_cr) == CAIRO_STATUS_SUCCESS)
    return true;

  clear_curve_surface ();

  curve_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
  if (!curve_surface ||
      cairo_surface_status (curve_surface) != CAIRO_STATUS_SUCCESS) {
    clear_curve_surface ();
    return false;
  }

  curve_cr = cairo_create (curve_surface);
  if (!curve_cr || cairo_status (curve_cr) != CAIRO_STATUS_SUCCESS) {
    clear_curve_surface ();
    return false;
  }

  curve_surface_w = w;
  curve_surface_h = h;
  return true;
}

void c_eqgraph::path_curve (cairo_t *cr, const std::vector<float> &v) {
  if (!v.size () || !cr)
    return;
  
  cairo_move_to (cr, 0, db_to_y (v [0]));
  for (int i = 1; i < v.size () && i < w; i++)
    cairo_line_to (cr, i, db_to_y (v [i]));
}

void c_eqgraph::on_resize (int w, int h) {
  (void) w;
  (void) h;
  generate_curves ();
  render_curve_surface ();
  curves_last_ms = nbtk::now_ms ();
  curves_dirty = false;
}

// avoid "jigsaw" up/down effect when moving band with high Q
static int eqgraph_oversampling_for_q (float q) {
  if (q >= 60.0f) return 16;
  if (q >= 20.0f) return 8;
  if (q >= 10.0f) return 4;
  if (q >= 4.0f)  return 2;
  return 1;
}

void c_eqgraph::generate_curves () { CP
  if (!state || w <= 0)
    return;
  
  //static nbtk::c_printfps p ("eqgraph curve: ");
  //p.tick ();
  
  curve.assign (w, 0.0f);
  for (int band = 0; band < EQ_NUM_BANDS; ++band)
    curves [band].assign (w, 0.0f);

  if (!state || samplerate <= 0)
    return;

  curve.assign (w, state->master_gain_db);

  for (int band = 0; band < EQ_NUM_BANDS; ++band) {
    if (!state->enabled [band] || state->mode [band] == EQ_OFF)
      continue;

    c_biquad biquad;
    biquad.set_peak (
      samplerate,
      state->freq [band],
      state->gain_db [band],
      state->q [band],
      state->mode [band]);

    for (int x = 0; x < w; ++x) {
      float db = 0.0f;

      int oversampling = 1;

      if (state->mode [band] == EQ_CURVE) {
        const float center = state->freq [band];
        const float q = std::max (state->q [band], 0.01f);
        const float range_hz = center / q * 4.0f;

        if (fabsf (x_to_freq ((float) x) - center) <= range_hz)
          oversampling = eqgraph_oversampling_for_q (q);
      }

      const float oversamp_jump = 1.0f / (float) oversampling;

      for (int i = 0; i < oversampling; ++i) {
        const float sub_x =
          (float) x - 0.5f + ((float) i + 0.5f) * oversamp_jump;

        const float db1 =
          biquad.response_db (x_to_freq (sub_x), samplerate);

        if (fabsf (db1) > fabsf (db))
          db = db1;
      }

      curves [band] [x] = std::clamp (db, -36.0f, 36.0f);
      curve [x] += db;
    }
  }

  for (float &db : curve)
    db = std::clamp (db, -36.0f, 36.0f);
}

float c_eqgraph::freq_to_x (float freq) const {
  freq = std::max (freq_min, std::min (freq, freq_max));
  float t = std::log (freq / freq_min) / std::log (freq_max / freq_min);
  return t * w;

}

float c_eqgraph::x_to_freq (float x) const {
  float t = std::max (0.0f, std::min ((float) x / (float) w, 1.0f));
  return freq_min * std::pow (freq_max / freq_min, t);
}

float c_eqgraph::db_to_y (float db) const {
  float half = h / 2;
  return half - ((float) (db * h) / 72.0f);
}

float c_eqgraph::y_to_db (float y) const {
  if (h <= 0)
    return 0.0f;

  const float half = h / 2.0f;
  const float db = (half - (float) y) * 72.0f / (float) h;
  return std::clamp (db, -36.0f, 36.0f);
}

int c_eqgraph::find_handle (int x, int y) const {
  if (!state)
    return -1;

  const float rad = handle_size * 0.5f;
  for (int i = EQ_NUM_BANDS - 1; i >= 0; --i) {
    if (!state->enabled [i])
      continue;

    const float handle_x = freq_to_x (state->freq [i]);
    const float handle_y = db_to_y (state->gain_db [i]);
    if (fabsf ((float) x - handle_x) <= rad &&
        fabsf ((float) y - handle_y) <= rad)
      return i;
  }

  return -1;
}

void c_eqgraph::set_mouse_handle (int handle) {
  handle = std::clamp (handle, -1, EQ_NUM_BANDS - 1);

  int next_x = -1;
  int next_y = -1;
  if (handle >= 0 && state) {
    next_x = (int) std::round (freq_to_x (state->freq [handle]));
    next_y = (int) std::round (db_to_y (state->gain_db [handle]));
  }

  if (handle == mouse_handle &&
      next_x == mouse_handle_x &&
      next_y == mouse_handle_y)
    return;

  mouse_handle = handle;
  mouse_handle_x = next_x;
  mouse_handle_y = next_y;

  invalidate ();
}

void c_eqgraph::update_mouse_handle (int x, int y) {
  const int handle = find_handle (x, y);

  if (handle >= 0 && state) {
    const int handle_x = (int) std::round (freq_to_x (state->freq [handle]));
    const int handle_y = (int) std::round (db_to_y (state->gain_db [handle]));
    const float rad = handle_size * 0.5f;

    if (fabsf ((float) x - (float) handle_x) <= rad &&
        fabsf ((float) y - (float) handle_y) <= rad) {
      set_mouse_handle (handle);
      return;
    }
  } else {
    set_mouse_handle (-1);
    return;
  }

  set_mouse_handle (-1);
}

void c_eqgraph::clear_mouse_handle () {
  set_mouse_handle (-1);
}

void c_eqgraph::emit_band_action (int band) {
  if (band < 0 || band >= EQ_NUM_BANDS)
    return;

  lane = (uint64_t) band;

  nbtk::t_action_event event;
  event.source = this;
  event.source_id = id;
  event.mouse_button = last_mouse_button;
  on_action (event);
}

void c_eqgraph::update_dragged_handle (int x, int y) {
  if (!state || drag_handle < 0 || drag_handle >= EQ_NUM_BANDS)
    return;

  state->freq [drag_handle] = std::clamp (
    x_to_freq ((float) x),
    freq_min,
    freq_max);
  state->gain_db [drag_handle] = y_to_db ((float) y);
  state_changed ();
  set_mouse_handle (drag_handle);
  emit_band_action (drag_handle);
}

bool c_eqgraph::on_mouse_down (int mouse_x, int mouse_y, int button) {
  if (!state || samplerate <= 0)
    return nbtk::c_canvas::on_mouse_down (mouse_x, mouse_y, button);

  last_mouse_button = button;
  invalidate ();
  
  if (button == Button4 || button == Button5) {
    const int found = find_handle (mouse_x, mouse_y);
    if (found >= 0) {
      const float qmult = button == Button4 ? 1.05f : 1.0f / 1.05f;
      state->q [found] = std::clamp (state->q [found] * qmult, 0.01f, 100.0f);
      state_changed ();
      emit_band_action (found);
    }
    return true;
  }

  nbtk::c_canvas::on_mouse_down (mouse_x, mouse_y, button);

  if (button != Button1)
    return true;

  drag_handle = find_handle (mouse_x, mouse_y);
  set_mouse_handle (drag_handle);
  drag_orig_x = mouse_x;
  drag_orig_y = mouse_y;
  return true;
}

bool c_eqgraph::on_mouse_up (int mouse_x, int mouse_y, int button) {
  if (button == Button1 && drag_handle >= 0)
    update_dragged_handle (mouse_x, mouse_y);
  
  invalidate ();
  
  nbtk::c_canvas::on_mouse_up (mouse_x, mouse_y, button);

  if (button == Button1)
    drag_handle = -1;
  
  update_mouse_handle (mouse_x, mouse_y);
  return true;
}

bool c_eqgraph::on_mouse_move (int mouse_x, int mouse_y) {
  nbtk::c_canvas::on_mouse_move (mouse_x, mouse_y);

  if (drag_handle >= 0)
    update_dragged_handle (mouse_x, mouse_y);
  else
    update_mouse_handle (mouse_x, mouse_y);

  return true;
}

void c_eqgraph::on_mouse_leave () {
  clear_mouse_handle ();
  nbtk::c_canvas::on_mouse_leave ();
}
