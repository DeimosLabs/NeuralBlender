

#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstring>

#include "eqgraph.h"

#define CMDLINE_DEBUG_COLOR ANSI_DARK_BLUE
#include "cmdline_debug.h"

c_eqgraph::c_eqgraph () { CP }
c_eqgraph::~c_eqgraph () { CP }

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
}

void c_eqgraph::on_paint (cairo_t *cr) {
  if (!state)
    return;
  
  generate_curves ();
  cairo_set_source_rgba (cr, 1, 1, 1, 1);
  cairo_set_line_width (cr, 2.0f);
  path_curve (cr, curve);
  cairo_stroke (cr);
  
  cairo_set_source_rgba (cr, 0.7, 1.0, 0.7, 1);
  cairo_set_line_width (cr, 1.0f);
  for (int i = 0; i < EQ_NUM_BANDS; i++) {
    if (state->enabled [i]) {
      int x = freq_to_x (state->freq [i]);
      int y = db_to_y (state->gain_db [i]);
      
      cairo_rectangle (cr, 0.5 + x - anchor_size / 2, 0.5 + y - anchor_size / 2, 
                       anchor_size, anchor_size);
      cairo_stroke (cr);
    }
  }
}

void c_eqgraph::path_curve (cairo_t *cr, std::vector<float> v) {
  if (!v.size () || !cr)
    return;
  
  cairo_move_to (cr, 0, db_to_y (v [0]));
  for (int i = 1; i < v.size () && i < w; i++) {
    cairo_line_to (cr, i, db_to_y (v [i]));
  }
}

void c_eqgraph::on_resize (int w, int h) {
  generate_curves ();
}

static int eqgraph_oversampling_for_q (float q) {
  if (q >= 60.0f) return 16;
  if (q >= 20.0f) return 8;
  if (q >= 10.0f) return 4;
  if (q >= 4.0f)  return 2;
  return 1;
}

// c_biquad already does the heavy math for us
void c_eqgraph::generate_curves () { CP
  if (w <= 0)
    return;

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

float c_eqgraph::freq_to_x (float freq) {
  freq = std::max (freq_min, std::min (freq, freq_max));
  float t = std::log (freq / freq_min) / std::log (freq_max / freq_min);
  return t * w;

}

float c_eqgraph::x_to_freq (float x) {
  float t = std::max (0.0f, std::min ((float) x / (float) w, 1.0f));
  return freq_min * std::pow (freq_max / freq_min, t);
}

float c_eqgraph::db_to_y (float db) {
  float half = h / 2;
  return half - ((float) (db * h) / 72.0f);
}

float c_eqgraph::y_to_db (float y) {
  if (h <= 0)
    return 0.0f;

  const float half = h / 2.0f;
  const float db = (half - (float) y) * 72.0f / (float) h;
  return std::clamp (db, -36.0f, 36.0f);
}
