

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
  release_pointer_grab ();
  clear_curve_surface ();
}

void c_eqgraph::render_base (cairo_t *cr) {
  int i;

  cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);

  cairo_rectangle (cr, 0, 0, w, h);
  cairo_set_source_rgba (cr, 0, 0, 0.05, 1);
  cairo_fill (cr);
  for (i = freq_min; i <= freq_max;) {
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
  cairo_set_source_rgba (cr, 0, 0, 0.5, 0.5);
  cairo_stroke (cr);

  int y = db_to_y (0);
  cairo_move_to (cr, 0, y);
  cairo_line_to (cr, w, y);
  cairo_set_source_rgba (cr, 1, 1, 0, 1);
  cairo_set_line_width (cr, 1);
  cairo_stroke (cr);
  
  cairo_set_source_rgba, (cr, 0, 0, 1, 1);
  cairo_set_font_size (cr, 11);
  
  cairo_text_extents_t ext;
  cairo_text_extents (cr, "Ay", &ext);
  int label_y = h + 6 - ext.height;
  
  cairo_set_source_rgba (cr, 0.3, 0.3, 1.0, 0.3);
  char buf [32];
  for (int i : { 50, 200, 1000, 4000, 8000 }) {
    if (i < 1000)
      snprintf (buf, 31, "%d", i);
    else
      snprintf (buf, 31, "%dK", i / 1000);
    float fx = freq_to_x (i);
    cairo_move_to (cr, fx, 0);
    cairo_line_to (cr, fx, h);
    cairo_move_to (cr, fx + 2, label_y);
    cairo_show_text (cr, buf);
  }
  cairo_stroke (cr);
  
  int x = freq_to_x (1000);
  cairo_move_to (cr, x, 0);
  cairo_line_to (cr, x, h);
  cairo_set_source_rgba (cr, 0, 0, 0.9, 1);
  cairo_stroke (cr);
}

void c_eqgraph::on_paint (cairo_t *cr) {
  if (!state)
    return;
  
  //static nbtk::c_printfps p ("on_paint: ");
  //p.tick ();
  
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

  const int band = mouse_handle >= 0 ? mouse_handle : highlighted_band;
  if (band >= 0 && band < EQ_NUM_BANDS && curve_surfaces [band]) {
    if (band_curve_surfaces_dirty [band])
      render_band_curve_surface (band);
    cairo_set_source_surface (cr, curve_surfaces [band], 0, 0);
    cairo_paint (cr);
  }

  const bool draw_handles = always_show_handles || mouse_in () || drag_handle >= 0;
  if (draw_handles) {
    for (int i = 0; i < EQ_NUM_BANDS; i++) {
      if (!state->enabled [i])
        continue;

      int x = freq_to_x (state->freq [i]);
      int y = db_to_y (state->gain_db [i]);

      cairo_rectangle (cr,
                       0.5 + x - handle_size / 2,
                       0.5 + y - handle_size / 2,
                       handle_size, handle_size);

      if (i == drag_handle)
        cairo_set_source_rgba (cr, 1, 1, 0, 1);
      else
        cairo_set_source_rgba (cr, 0.7, 1.0, 0.7, 1);

      cairo_set_line_width (cr, 1.0f);
      cairo_stroke (cr);
    }
  }

  if (draw_handles && mouse_handle_x >= 0 && mouse_handle_y >= 0) {
    cairo_set_line_width (cr, 2.0f);
    cairo_set_source_rgba (cr, 1.0, 1.0, 0.0, 0.25);
    cairo_rectangle (cr,
                     mouse_handle_x - handle_size / 2,
                     mouse_handle_y - handle_size / 2,
                     handle_size, handle_size);
    cairo_fill (cr);
  }
  
  if (mouse_x >= 0 && mouse_y >= 0 &&
      band >= 0 && band < EQ_NUM_BANDS) {
    int label_x = mouse_x + 24;
    int label_y = std::clamp (mouse_y - 16, 0, h - 32);
    if (w - mouse_x < 80)
      label_x = mouse_x - 80;
      
    print_label (cr, label_x, label_y, band);
  }

}

void c_eqgraph::print_label (cairo_t *cr, int x, int y, int band) {
  char buf [64];
  
  if (!state || band < 0 || band >= EQ_NUM_BANDS)
    return;
  
  cairo_save (cr);
  
  float freq = state->freq [band];
  float gain = state->gain_db [band];
  float q = state->q [band];
  std::string hz_unit = "Hz";
  std::string hz_format = "%.3fHz";
  if (freq >= 1000) {
    freq /= 1000;
    hz_unit = "KHz";
  } else {
    hz_format = freq < 100 ? "%.2fHz" : "%.1fHz";
  }
  
  cairo_set_source_rgba (cr, 0.5, 0.5, 1.0, 1.0);
  cairo_set_font_size (cr, 11);
  cairo_select_font_face (cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_text_extents_t ext;
  cairo_text_extents (cr, "Ay", &ext);
  
  snprintf (buf, 63, hz_format.c_str (), freq);
  cairo_move_to (cr, x, y + ext.height);
  cairo_show_text (cr, buf);
  
  snprintf (buf, 63, "%s%.1fdB", gain < 0.0f ? "-" : "+", 
            fabs (gain));
  cairo_move_to (cr, x, y + ext.height * 2);
  cairo_show_text (cr, buf);
  
  snprintf (buf, 63, "Q=%.2f", q);
  cairo_move_to (cr, x, y + ext.height * 3);
  cairo_show_text (cr, buf);
  
  cairo_restore (cr);
}

void c_eqgraph::set_state (c_eq_state *state_) {
  state = state_;
  state_changed ();
}

void c_eqgraph::state_changed () {
  mark_curves_dirty (true);
}

void c_eqgraph::set_highlighted_band (int band) {
  band = std::clamp (band, -1, EQ_NUM_BANDS - 1);
  if (highlighted_band == band)
    return;

  highlighted_band = band;
  invalidate ();
}

void c_eqgraph::set_samplerate (int sr) {
  samplerate = sr;
  state_changed ();
}

void c_eqgraph::render_curve_surface () {
  if (!ensure_curve_surface () || !curve_cr)
    return;

  //static nbtk::c_printfps p ("render_curve_surface: ");
  //p.tick ();
  
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

  mark_band_curve_surfaces_dirty ();

  cairo_surface_flush (curve_surface);
}

void c_eqgraph::mark_band_curve_surfaces_dirty () {
  for (int i = 0; i < EQ_NUM_BANDS; ++i)
    band_curve_surfaces_dirty [i] = true;
}

void c_eqgraph::render_band_curve_surface (int band) {
  if (band < 0 || band >= EQ_NUM_BANDS || !curve_surfaces [band])
    return;

  cairo_t *cr = cairo_create (curve_surfaces [band]);
  if (!cr || cairo_status (cr) != CAIRO_STATUS_SUCCESS) {
    if (cr)
      cairo_destroy (cr);
    return;
  }

  cairo_save (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);
  cairo_restore (cr);

  if (state &&
      state->enabled [band] &&
      !curves [band].empty ()) {
    cairo_set_line_width (cr, 1.0f);
    cairo_set_source_rgba (cr, 0.35, 0.95, 0.95, 0.95);
    path_curve (cr, curves [band]);
    cairo_stroke (cr);
  }

  cairo_destroy (cr);
  cairo_surface_flush (curve_surfaces [band]);
  band_curve_surfaces_dirty [band] = false;
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

  for (int i = 0; i < EQ_NUM_BANDS; ++i) {
    if (curve_surfaces [i]) {
      cairo_surface_destroy (curve_surfaces [i]);
      curve_surfaces [i] = NULL;
    }
    band_curve_surfaces_dirty [i] = true;
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
      cairo_status (curve_cr) == CAIRO_STATUS_SUCCESS) {
    bool band_surfaces_ok = true;
    for (int i = 0; i < EQ_NUM_BANDS; ++i) {
      band_surfaces_ok =
        band_surfaces_ok &&
        curve_surfaces [i] &&
        cairo_surface_status (curve_surfaces [i]) == CAIRO_STATUS_SUCCESS;
    }
    if (band_surfaces_ok)
      return true;
  }

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

  for (int i = 0; i < EQ_NUM_BANDS; ++i) {
    curve_surfaces [i] = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
    if (!curve_surfaces [i] ||
        cairo_surface_status (curve_surfaces [i]) != CAIRO_STATUS_SUCCESS) {
      clear_curve_surface ();
      return false;
    }
    band_curve_surfaces_dirty [i] = true;
  }

  curve_surface_w = w;
  curve_surface_h = h;
  return true;
}

void c_eqgraph::path_curve (cairo_t *cr, const std::vector<float> &v) {
  if (!v.size () || !cr)
    return;

  //static nbtk::c_printfps p ("path curve: ");
  //p.tick ();
  
  const size_t n = v.size ();
  const float denom = (n > 1) ? (float) (n - 1) : 1.0f;
  const float xscale = (w > 1) ? (float) (w - 1) / denom : 0.0f;

  cairo_move_to (cr, 0.0f, db_to_y (v [0]));
  for (size_t i = 1; i < n; i++)
    cairo_line_to (cr, (float) i * xscale, db_to_y (v [i]));
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
// yeah yeah, could just use a log function for this
static int eqgraph_oversampling_for_q (float q) {
  if (q >= 64.0f) return 16;
  if (q >= 32.0f) return 12;
  if (q >= 16.0f) return 8;
  if (q >= 8.0f)  return 4;
  if (q >= 4.0f)  return 2;
  return 1;
}

void c_eqgraph::generate_curves () { CP
  if (!state || w <= 0)
    return;
  
  //static nbtk::c_printfps p ("generate_curves: ");
  //p.tick ();
  
  curve.assign (CURVE_POINTS, 0.0f);
  for (int band = 0; band < EQ_NUM_BANDS; ++band)
    curves [band].assign (CURVE_POINTS, 0.0f);

  if (!state || samplerate <= 0)
    return;

  curve.assign (CURVE_POINTS, state->master_gain_db);

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

    for (int x = 0; x < CURVE_POINTS; ++x) {
      float db = 0.0f;

      int oversampling = 1;

      if (state->mode [band] == EQ_CURVE) {
        const float center = state->freq [band];
        const float q = std::max (state->q [band], 0.01f);
        const float range_hz = center / q * 4.0f;

        if (fabsf (curve_x_to_freq ((float) x) - center) <= range_hz)
          oversampling = eqgraph_oversampling_for_q (q);
      }

      const float oversamp_jump = 1.0f / (float) oversampling;

      for (int i = 0; i < oversampling; ++i) {
        const float sub_x =
          (float) x - 0.5f + ((float) i + 0.5f) * oversamp_jump;

        const float db1 =
          biquad.response_db (curve_x_to_freq (sub_x), samplerate);

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

float c_eqgraph::curve_x_to_freq (float x) const {
  const float max_x = (float) (CURVE_POINTS - 1);
  const float t = std::max (0.0f, std::min (x / max_x, 1.0f));
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

void c_eqgraph::mark_curves_dirty (bool force_redraw) {
  curves_dirty = true;

  const uint64_t now = nbtk::now_ms ();
  if (force_redraw ||
      !curve_surface ||
      now - curves_last_ms >= CURVE_RECALC_MS) {
    invalidate ();
  }
}

void c_eqgraph::set_mouse_handle (int handle, bool redraw) {
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

  if (redraw)
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

bool c_eqgraph::point_inside_graph (int x, int y) const {
  return x >= 0 && y >= 0 && x < w && y < h;
}

void c_eqgraph::render_curves_now () {
  generate_curves ();
  render_curve_surface ();
  curves_last_ms = nbtk::now_ms ();
  curves_dirty = false;
  invalidate ();
}

void c_eqgraph::restore_dragged_handle () {
  if (!state || drag_handle < 0 || drag_handle >= EQ_NUM_BANDS)
    return;

  state->freq [drag_handle] = drag_orig_freq;
  state->gain_db [drag_handle] = drag_orig_gain_db;
  set_mouse_handle (drag_handle);
  render_curves_now ();
  emit_band_action (drag_handle);
}

void c_eqgraph::release_pointer_grab () {
  if (!pointer_grabbed)
    return;

  if (app && app->backend && toplevel)
    app->backend->ungrab_pointer (toplevel->widget);
  pointer_grabbed = false;
}

void c_eqgraph::update_dragged_handle (int x, int y, bool force_redraw) {
  if (!state || drag_handle < 0 || drag_handle >= EQ_NUM_BANDS)
    return;

  if (!point_inside_graph (x, y)) {
    if (!drag_outside) {
      drag_outside = true;
      restore_dragged_handle ();
    }
    return;
  }

  drag_outside = false;

  state->freq [drag_handle] = std::clamp (
    x_to_freq ((float) x),
    freq_min,
    freq_max);
  state->gain_db [drag_handle] = y_to_db ((float) y);
  mark_curves_dirty (force_redraw);
  set_mouse_handle (drag_handle, force_redraw);
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
  drag_outside = false;
  if (drag_handle >= 0 && state) {
    drag_orig_freq = state->freq [drag_handle];
    drag_orig_gain_db = state->gain_db [drag_handle];
    if (app)
      app->set_focus (this);
    if (app && app->backend && toplevel)
      pointer_grabbed = app->backend->grab_pointer (toplevel->widget);
  }
  return true;
}

bool c_eqgraph::on_mouse_up (int mouse_x, int mouse_y, int button) {
  if (button == Button1 && drag_handle >= 0) {
    if (point_inside_graph (mouse_x, mouse_y))
      update_dragged_handle (mouse_x, mouse_y, true);
    else
      restore_dragged_handle ();
  }
  
  invalidate ();
  
  nbtk::c_canvas::on_mouse_up (mouse_x, mouse_y, button);

  if (button == Button1) {
    drag_handle = -1;
    drag_outside = false;
    release_pointer_grab ();
  }
  
  update_mouse_handle (mouse_x, mouse_y);
  return true;
}

bool c_eqgraph::on_mouse_move (int mouse_x, int mouse_y) {
  const bool was_mouse_in = mouse_in ();

  nbtk::c_canvas::on_mouse_move (mouse_x, mouse_y);

  if (drag_handle >= 0)
    update_dragged_handle (mouse_x, mouse_y, false);
  else
    update_mouse_handle (mouse_x, mouse_y);

  if (!was_mouse_in && mouse_in ())
    invalidate ();

  return true;
}

void c_eqgraph::on_mouse_enter () {
  nbtk::c_canvas::on_mouse_enter ();
  invalidate ();
}

void c_eqgraph::on_mouse_leave () {
  if (drag_handle >= 0) {
    if (!drag_outside) {
      drag_outside = true;
      restore_dragged_handle ();
    }
    return;
  }

  clear_mouse_handle ();
  nbtk::c_canvas::on_mouse_leave ();
  invalidate ();
}
