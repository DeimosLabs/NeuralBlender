

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
  if (state) {
  } else {
    for (i = 20; i <= 20000;) {
      debug ("i=%d", i);
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
    for (i = 0; i < 36; i += 6) {
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
  }
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

void c_eqgraph::on_paint (cairo_t *cr) {
  
}

int c_eqgraph::freq_to_x (float freq) {
  freq = std::max (freq_min, std::min (freq, freq_max));
  float t = std::log (freq / freq_min) / std::log (freq_max / freq_min);
  return t * w;

}

float c_eqgraph::x_to_freq (int x) {
  float t = std::max (0.0f, std::min ((float) x / (float) w, 1.0f));
  return freq_min * std::pow (freq_max / freq_min, t);
}

int c_eqgraph::db_to_y (float db) {
  float half = h / 2;
  return half - ((float) (db * h) / 36.0f);
}

float c_eqgraph::y_to_db (int y) {
  float half = h / 2;
  return -1;
}
