
/* NeuralBlender - 
 * First attempt at implementing EQ graph.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cmath>
#include <atomic>

#include "neuralblender.h"
#include "nbtk.h"

#define CURVE_RECALC_MS 30
#define CURVE_POINTS    640

class c_eqgraph : public nbtk::c_canvas {
public:
  c_eqgraph ();
  ~c_eqgraph ();
  
  void set_samplerate (int sr);
  void render_base (cairo_t *cr);
  void on_paint (cairo_t *cr);
  void set_state (c_eq_state *state);
  void state_changed ();
  void set_highlighted_band (int band);
  void on_resize (int w, int h);
  bool on_mouse_down (int x, int y, int button) override;
  bool on_mouse_up (int x, int y, int button) override;
  bool on_mouse_move (int x, int y) override;
  void on_mouse_enter () override;
  void on_mouse_leave () override;
  
  bool always_show_handles = false;
  float freq_min = 20.0f;     // NOTE TO SELF: check for hard coded <---
  float freq_max = 20000.0f; 
  float db_range = 36.0f;
  float handle_size = 12.0f;
  
  
private:
  float freq_to_x (float freq) const;
  float x_to_freq (float x) const;
  float curve_x_to_freq (float x) const;
  float db_to_y (float gain_db) const;
  float y_to_db (float y) const;
  
  inline bool mouse_in () { return mouse_x >= 0 && mouse_y >= 0; }
  
  int find_handle (int x, int y) const;
  void mark_curves_dirty (bool force_redraw);
  void set_mouse_handle (int handle, bool redraw = true);
  void update_mouse_handle (int x, int y);
  void clear_mouse_handle ();
  void emit_band_action (int band);
  bool point_inside_graph (int x, int y) const;
  void render_curves_now ();
  void restore_dragged_handle ();
  void release_pointer_grab ();
  void update_dragged_handle (int x, int y, bool force_redraw);
  
  void generate_curves ();
  void clear_curve_surface ();
  bool ensure_curve_surface ();
  void mark_band_curve_surfaces_dirty ();
  void render_curve_surface ();
  void render_band_curve_surface (int band);
  void path_curve (cairo_t *cr, const std::vector<float> &v);
  void print_label (cairo_t *cr, int x, int y, int band);
  
  int samplerate = 0;
  
  c_eq_state *state = NULL;
  std::vector<float> curves [EQ_NUM_BANDS];
  std::vector<float> curve;
  
  bool curves_dirty = true;
  uint64_t curves_last_ms = 0;
  //static constexpr uint64_t CURVE_RECALC_MS = 50;
  
  cairo_surface_t *curve_surface = NULL;
  cairo_surface_t *curve_surfaces [EQ_NUM_BANDS] = { NULL };
  cairo_t *curve_cr = NULL;
  int curve_surface_w = 0;
  int curve_surface_h = 0;
  int highlighted_band = -1;
  bool band_curve_surfaces_dirty [EQ_NUM_BANDS] = {};
  
  int mouse_handle = -1;
  int mouse_handle_x = -1;
  int mouse_handle_y = -1;
  int drag_handle = -1;
  int drag_orig_x = -1;
  int drag_orig_y = -1;
  float drag_orig_freq = 0.0f;
  float drag_orig_gain_db = 0.0f;
  bool drag_outside = false;
  bool pointer_grabbed = false;
}; 
