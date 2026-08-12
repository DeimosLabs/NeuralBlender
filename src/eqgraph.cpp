

#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstring>

#include "eqgraph.h"

#define CMDLINE_DEBUG_COLOR ANSI_DARK_BLUE
#include "cmdline/debug.h"

c_eqgraph::c_eqgraph () { CP
  generate_spectrum_frequencies (
    spectrum_frequencies.data (), spectrum_frequencies.size ());
  reset_hold ();
  spectrum_input_db.fill (spectrum_floor_db);
  spectrum_output_db.fill (spectrum_floor_db);
  spectrum_input_hold.fill (0.0f);
  spectrum_output_hold.fill (0.0f);
  spectrum_last_update_ms = nbtk::now_ms ();
}

c_eqgraph::~c_eqgraph () {
  CP
  release_pointer_grab ();
  clear_spectrum_surface ();
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
  for (i = 0; i <= db_range; i += 12) {
    int y = std::clamp (db_to_y (i), 0.0f, (float) (h - 1));
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
  
  cairo_set_source_rgba, (cr, 0, 0, 1, 1);
  cairo_set_font_size (cr, get_app_font_size () * nbtk::TEXTSIZE_SMALL);
  
  cairo_text_extents_t ext;
  cairo_text_extents (cr, "Ay", &ext);
  int label_y = h + 6 - ext.height;
  
  cairo_set_source_rgba (cr, 0.3, 0.3, 1.0, 0.3);
  char buf [32];
  for (int i : { 20, 200, 1000, 4000, 8000, 20000 }) {
    int fx = std::clamp (freq_to_x (i), 0.0f, (float) (w - 1));
    cairo_move_to (cr, fx + 0.5f, 0);
    cairo_line_to (cr, fx + 0.5f, h);
    if (i < 19999) {
      if (i < 1000)
        snprintf (buf, 31, "%d", i);
      else
        snprintf (buf, 31, "%dK", i / 1000);
      cairo_move_to (cr, fx + 2, label_y);
      cairo_show_text (cr, buf);
    }
  }
  cairo_stroke (cr);
  
  // 1KHz line?
  int x = freq_to_x (1000);
  cairo_move_to (cr, x, 0);
  cairo_line_to (cr, x, h);
  cairo_set_source_rgba (cr, 0, 0, 0.9, 1);
  cairo_stroke (cr);
  
  int y = db_to_y (0);
  cairo_move_to (cr, 0, y);
  cairo_line_to (cr, w, y);
  cairo_set_source_rgba (cr, 1, 1, 0, 1);
  cairo_set_line_width (cr, 1);
  cairo_stroke (cr);
}

void c_eqgraph::on_paint (cairo_t *cr) {
  if (!state)
    return;
  
  //debugfps ("on_paint");
  
  const uint64_t now = nbtk::now_ms ();
  last_paint_ms = now;
  if (!curve_surface ||
      (curves_dirty && now - curves_last_ms >= CURVE_RECALC_MS)) {
    generate_curves ();
    render_curve_surface ();
    curves_last_ms = now;
    curves_dirty = false;
  }
  
  if (!curve_surface)
    return;
  
  if (spectrum_surface_dirty ||
      spectrum_surface_w != w || spectrum_surface_h != h) {
    render_spectrum_surface ();
  }
  
  if (spectrum_surface) {
    cairo_set_source_surface (cr, spectrum_surface, 0, 0);
    cairo_paint (cr);
  }
  
  cairo_set_source_surface (cr, curve_surface, 0, 0);
  cairo_paint (cr);
  
  const int band = mouse_handle >= 0 ? mouse_handle : highlighted_band;
  if (band >= 0 && band < EQ_NUM_BANDS && curve_surfaces [band]) {
    if (band_curve_surfaces_dirty [band])
      render_band_curve_surface (band);
    cairo_set_source_surface (cr, curve_surfaces [band], 0, 0);
    cairo_paint (cr);
  }
  
  const bool draw_handles =
    always_show_handles ||
    mouse_in () ||
    drag_handle >= 0 ||
    highlighted_band >= 0;
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
      
      if (i == drag_handle || i == band)
        cairo_set_source_rgba (cr, 1, 1, 0, 1);
      else
        cairo_set_source_rgba (cr, 0.7, 1.0, 0.7, 1);
      
      cairo_set_line_width (cr, i == band ? 2.0f : 1.0f);
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
  
  if (band >= 0 && band < EQ_NUM_BANDS) {
    int label_x = mouse_x + 24;
    int label_y = std::clamp (mouse_y - 16, 0, h - 32);
    //if (w - mouse_x < 80)
    //  label_x = mouse_x - 80;
    
    if (state->enabled [band]) {
      draw_label (cr, band);
    }
  }

}

void c_eqgraph::draw_spectrum (cairo_t *cr) {
  if (!cr || !spectrum_valid || w <= 1 || h <= 1)
    return;
  
  cairo_save (cr);
  cairo_rectangle (cr, 0, 0, w, h);
  cairo_clip (cr);
  cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
  cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
  
  cairo_set_source_rgba (cr, 0.28f, 0.48f, 0.75f, 0.2f);
  if (m_hold) {
    path_spectrum (cr, spectrum_hold.data (), spectrum_hold.size (), true);
    cairo_fill (cr);
  }
  
  cairo_set_line_width (cr, 0.5f);
  cairo_set_source_rgba (cr, 0.28f, 0.48f, 0.95f, 0.75f);
  path_spectrum (cr, spectrum_input_db.data (), spectrum_input_db.size ());
  cairo_stroke (cr);
  
  cairo_set_line_width (cr, 1.0f);
  cairo_set_source_rgba (cr, 1.00f, 0.40f, 0.40f, 0.75f);
  path_spectrum (cr, spectrum_output_db.data (), spectrum_output_db.size ());
  cairo_stroke (cr);
  
  cairo_restore (cr);
}

void c_eqgraph::path_spectrum (cairo_t *cr, const float *values,
                               size_t count, bool fill) {
  if (!cr || !values || count == 0)
    return;
  
  const float baseline_y = (float) (h - 1);
  float x = freq_to_x (spectrum_frequencies [0]);
  float y = spectrum_db_to_y (
    std::isfinite (values [0]) ? values [0] : spectrum_floor_db);
  
  cairo_new_path (cr);
  if (fill) {
    cairo_move_to (cr, x, baseline_y);
    cairo_line_to (cr, x, y);
  } else {
    cairo_move_to (cr, x, y);
  }
  
  for (size_t i = 1; i < count; ++i) {
    const float db = std::isfinite (values [i]) ? values [i] : spectrum_floor_db;
    x = freq_to_x (spectrum_frequencies [i]);
    y = spectrum_db_to_y (db);
    cairo_line_to (cr, x, y);
  }
  
  if (fill) {
    cairo_line_to (cr, x, baseline_y);
    cairo_close_path (cr);
  }
}

void c_eqgraph::draw_label (cairo_t *cr, int band) {
  char buf [5] [64];
  static const char *modenames [] = {
    "Off",
    "Hipass",
    "Low shelf",
    "Bell",
    "Hi shelf",
    "Lowpass",
    NULL
  };
  
  int x = 5;
  int y = 5;
  
  if (!state || band < 0 || band >= EQ_NUM_BANDS)
    return;
  
  cairo_save (cr);
  
  float freq = state->freq [band];
  float gain = state->gain_db [band];
  float q = state->q [band];
  std::string hz_unit = "Hz";
  std::string hz_format = "%.3f%s";
  float showfreq = freq;
  if (showfreq >= 1000) {
    showfreq /= 1000;
    hz_unit = "KHz";
  } else {
    hz_format = showfreq < 100 ? "%.2f%s" : "%.1f%s";
  }
  
  cairo_set_font_size (cr, get_app_font_size () * nbtk::TEXTSIZE_MINI);
  cairo_select_font_face (cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_text_extents_t ext;
  cairo_text_extents (cr, "Ay", &ext);
  int h = ext.height * 1.1f;
  int w = 0;
  
  float cents;
  int note = freq_to_midi_note (freq, &cents, 440); // borrowed from tuner
  
  snprintf (buf [0], 63, "%d: %s", band + 1, modenames [state->mode [band]]);
  snprintf (buf [1], 63, hz_format.c_str (), showfreq, hz_unit.c_str ());
  snprintf (buf [3], 63, "%s%.1fdB", gain < 0.0f ? "-" : "+", fabs (gain));
  snprintf (buf [4], 63, "Q=%.2f", q);
  if (note >= 8 && note <= 184) {
    snprintf (buf [2], 63, "%s%d (%s%.3f)", 
              g_note_names_sharps [note % 12],
              (int) (note / 12 - 1),
              cents < 0 ? "-" : "+", fabs (cents));
  } else {
    snprintf (buf [2], 63, "");
  }
  
  cairo_text_extents (cr, "(A-0 (9999.9Hz)", &ext);
  w = ext.width;
  for (int i = 0; i < 5; i++) {
    cairo_text_extents (cr, buf [i], &ext);
    w = std::max (w, (int) ext.width);
  }
  
  //if (x > mouse_x && x < mouse_x + w)
  //  x -= w;
  if (mouse_x >= 0 && mouse_x < w + 16 && mouse_y < h * 5 + 12)
    x = this->w - w - 5;
  
  x = std::clamp (x, 4, (int) this->w - w - 8);
  y = std::clamp (y, 4, (int) this->h - (h * 5) - 8);
  
  cairo_rectangle (cr, x - 2, y - 2, w + 6, h * 5 + 6);
  cairo_set_source_rgba (cr, 0, 0, 0, 0.5);
  cairo_fill (cr);
  cairo_set_source_rgba (cr, 0.5, 0.5, 1.0, 1.0);
  
  for (int i = 0; i < 5; i++) {
    cairo_move_to (cr, x, y + (h * (i + 1)));
    cairo_show_text (cr, buf [i]);
  }
  
  cairo_restore (cr);
}

void c_eqgraph::set_state (c_eq_state *state_) {
  state = state_;
  state_changed ();
}

void c_eqgraph::do_hold (bool b) {
  m_hold = b;
  if (!b)
    reset_hold ();
  
  spectrum_surface_dirty = true;
  invalidate ();
}

void c_eqgraph::reset_hold () {
  spectrum_hold.fill (spectrum_floor_db);
  spectrum_surface_dirty = true;
  invalidate ();
}

void c_eqgraph::update_one_falling_curve (
    const float *incoming,
    std::array<float, SPECTRUM_BINS> &curve,
    std::array<float, SPECTRUM_BINS> &hold,
    float dt) {
  
  for (size_t i = 0; i < SPECTRUM_BINS; ++i) {
    const float db = std::isfinite (incoming [i]) ? incoming [i] : spectrum_floor_db;
    
    if (db >= curve [i]) {
      curve [i] = db;
      hold [i] = spectrum_hold_seconds;
      continue;
    }
    
    float fall_dt = dt;
    
    if (hold [i] > 0.0f) {
      fall_dt = std::max (0.0f, dt - hold [i]);
      hold [i] = std::max (0.0f, hold [i] - dt);
    }
    
    curve [i] = std::max (
      db,
      curve [i] - spectrum_fall_db_per_second * fall_dt);
  }
}

void c_eqgraph::update_falling_curves (
    const float *input_db,
    const float *output_db) {
  
  const uint64_t now = nbtk::now_ms ();
  
  float dt = spectrum_last_update_ms
    ? static_cast<float> (now - spectrum_last_update_ms) / 1000.0f
    : 0.0f;
  
  spectrum_last_update_ms = now;
  //dt = std::clamp (dt, 0.0f, 0.1f);
  
  update_one_falling_curve (
    input_db,
    spectrum_input_db,
    spectrum_input_hold,
    dt);
  
  update_one_falling_curve (
    output_db,
    spectrum_output_db,
    spectrum_output_hold,
    dt);
}

void c_eqgraph::set_spectrum (
    const float *input_db, const float *output_db, size_t count) {
  if (!input_db || !output_db || count < SPECTRUM_BINS)
    return;
  
  for (size_t i = 0; i < SPECTRUM_BINS; ++i) {
    const float db = std::isfinite (output_db [i])
      ? output_db [i]
      : spectrum_floor_db;
    spectrum_hold [i] = std::max (spectrum_hold [i], db);
  }
  
  update_falling_curves (input_db, output_db);
  
  spectrum_valid = true;
  spectrum_surface_dirty = true;
  if (nbtk::now_ms () - last_paint_ms >= CURVE_RECALC_MS)
    invalidate ();
}

void c_eqgraph::tick () {
  if (!is_visible () || (!curves_dirty && !spectrum_surface_dirty))
    return;
  
  const uint64_t now = nbtk::now_ms ();
  if (now - last_paint_ms >= CURVE_RECALC_MS)
    invalidate ();
}

void c_eqgraph::clear_spectrum_surface () {
  if (spectrum_cr) {
    cairo_destroy (spectrum_cr);
    spectrum_cr = NULL;
  }
  
  if (spectrum_surface) {
    cairo_surface_destroy (spectrum_surface);
    spectrum_surface = NULL;
  }
  
  spectrum_surface_w = 0;
  spectrum_surface_h = 0;
  spectrum_surface_dirty = true;
}

bool c_eqgraph::ensure_spectrum_surface () {
  if (w <= 0 || h <= 0)
    return false;
  
  if (spectrum_surface &&
      spectrum_cr &&
      spectrum_surface_w == w &&
      spectrum_surface_h == h &&
      cairo_surface_status (spectrum_surface) == CAIRO_STATUS_SUCCESS &&
      cairo_status (spectrum_cr) == CAIRO_STATUS_SUCCESS) {
    return true;
  }
  
  clear_spectrum_surface ();
  spectrum_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
  if (!spectrum_surface ||
      cairo_surface_status (spectrum_surface) != CAIRO_STATUS_SUCCESS) {
    clear_spectrum_surface ();
    return false;
  }
  
  spectrum_cr = cairo_create (spectrum_surface);
  if (!spectrum_cr || cairo_status (spectrum_cr) != CAIRO_STATUS_SUCCESS) {
    clear_spectrum_surface ();
    return false;
  }
  
  spectrum_surface_w = w;
  spectrum_surface_h = h;
  spectrum_surface_dirty = true;
  return true;
}

void c_eqgraph::render_spectrum_surface () {
  if (!ensure_spectrum_surface () || !spectrum_cr)
    return;
  
  cairo_save (spectrum_cr);
  cairo_set_operator (spectrum_cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (spectrum_cr);
  cairo_restore (spectrum_cr);
  
  if (spectrum_valid)
    draw_spectrum (spectrum_cr);
  
  cairo_surface_flush (spectrum_surface);
  spectrum_surface_dirty = false;
}

static bool eqgraph_states_equal (
    const c_eq_state &a,
    const c_eq_state &b) {
  
  if (a.on != b.on || a.master_gain_db != b.master_gain_db)
    return false;
  
  for (int i = 0; i < EQ_NUM_BANDS; ++i) {
    if (a.enabled [i] != b.enabled [i] ||
        a.mode [i] != b.mode [i] ||
        a.slope [i] != b.slope [i] ||
        a.freq [i] != b.freq [i] ||
        a.gain_db [i] != b.gain_db [i] ||
        a.q [i] != b.q [i]) {
      return false;
    }
  }
  
  return true;
}

void c_eqgraph::state_changed (bool force_redraw) {
  if (state &&
      rendered_state_valid &&
      eqgraph_states_equal (*state, rendered_state)) {
    return;
  }
  
  mark_curves_dirty (force_redraw);
}

void c_eqgraph::set_highlighted_band (int band) {
  band = std::clamp (band, -1, EQ_NUM_BANDS - 1);
  if (highlighted_band == band)
    return;
  
  highlighted_band = band;
  invalidate ();
}

void c_eqgraph::set_samplerate (int sr) {
  if (samplerate == sr)
    return;
  
  samplerate = sr;
  rendered_state_valid = false;
  state_changed ();
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
static int eqgraph_oversampling_for_q (float q) {
  if (q >= 64.0f) return 16;
  if (q >= 32.0f) return 12;
  if (q >= 16.0f) return 8;
  if (q >= 8.0f)  return 4;
  if (q >= 4.0f)  return 2;
  return 1;
}

void c_eqgraph::generate_curves () {
  if (!state || w <= 0)
    return;
  
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
    biquad.slope = state->slope [band];
    biquad.set_peak (
      samplerate,
      state->freq [band],
      state->gain_db [band],
      state->q [band],
      state->mode [band]);
    
    for (int x = 0; x < CURVE_POINTS; ++x) {
      float db = 0.0f;
      
      int oversampling = 1;
      
      if (state->mode [band] == EQ_BELL) {
        const float center = state->freq [band];
        const float q = std::max (state->q [band], 0.01f);
        const float range_hz = center / q * 4.0f;
        
        if (fabsf (curve_x_to_freq ((float) x) - center) <= range_hz)
          oversampling = eqgraph_oversampling_for_q (q);
      }
      
      const float oversamp_jump = 1.0f / (float) oversampling;
      
      for (int i = 0; i < oversampling; ++i) {
        const float sub_x = (float) x - 0.5f + ((float) i + 0.5f) * oversamp_jump;
        const float db1 = biquad.response_db (curve_x_to_freq (sub_x), samplerate);
        
        if (fabsf (db1) > fabsf (db))
          db = db1;
      }
      
      //curves [band] [x] = std::clamp (db, -1.0f * db_range, db_range);
      curves [band] [x] = db;
      curve [x] += db;
    }
  }
  
  //for (float &db : curve)
  //  db = std::clamp (db, -1.0f * db_range, db_range);
  
  rendered_state = *state;
  rendered_state_valid = true;
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
  return half - ((float) (db * h) / (db_range * 2));
}

float c_eqgraph::y_to_db (float y) const {
  if (h <= 0)
    return 0.0f;
  
  const float half = h / 2.0f;
  const float db = (half - (float) y) * (db_range * 2) / (float) h;
  return std::clamp (db, -1.0f * db_range, db_range);
}

/*float c_eqgraph::spectrum_db_to_y (float db) const {
  if (h <= 1 || spectrum_floor_db >= 0.0f)
    return 0.0f;
  
  float normalized = (db - spectrum_floor_db) / -spectrum_floor_db;
  if (spectrum_normalize) 
    normalized = std::clamp (
      normalized,
      0.0f,
      1.0f);
  
  return (1.0f - normalized) * (float) (h - 1);
}*/

float c_eqgraph::spectrum_db_to_y (float db) const {
  if (h <= 1 || spectrum_ceiling_db <= spectrum_floor_db)
    return 0.0f;
  
  const float range = spectrum_ceiling_db - spectrum_floor_db;
  float normalized = (db - spectrum_floor_db) / range;
  
  if (spectrum_normalize)
    normalized = std::clamp (normalized, 0.0f, 1.0f);
  
  const float scale = std::max (spectrum_scale, 0.001f);
  
  // Signed power preserves values outside the graph when not clamping.
  normalized = std::copysign (
    std::pow (std::abs (normalized), scale),
    normalized);
  
  return (1.0f - normalized) * static_cast<float> (h - 1);
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
  if (force_redraw)
    curves_last_ms = 0;
  
  const uint64_t now = nbtk::now_ms ();
  if (force_redraw ||
      !curve_surface ||
      now - last_paint_ms >= CURVE_RECALC_MS) {
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
  
  if (toplevel)
    toplevel->on_hover_changed (this);
  
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
  
  if (button == Button2 || button == Button3) {
    reset_hold ();
    return true;
  }
  
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
