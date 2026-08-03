
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

#define CURVE_RECALC_MS 20
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
  void on_resize (int w, int h);
  bool on_mouse_down (int x, int y, int button) override;
  bool on_mouse_up (int x, int y, int button) override;
  bool on_mouse_move (int x, int y) override;
  void on_mouse_leave () override;
  
  float freq_min = 20.0f;
  float freq_max = 20000.0f;
  float handle_size = 12.0f;
  
private:
  float freq_to_x (float freq) const;
  float x_to_freq (float x) const;
  float curve_x_to_freq (float x) const;
  float db_to_y (float gain_db) const;
  float y_to_db (float y) const;
  
  int find_handle (int x, int y) const;
  void mark_curves_dirty (bool force_redraw);
  void set_mouse_handle (int handle, bool redraw = true);
  void update_mouse_handle (int x, int y);
  void clear_mouse_handle ();
  void emit_band_action (int band);
  void update_dragged_handle (int x, int y, bool force_redraw);
  
  void generate_curves ();
  void clear_curve_surface ();
  bool ensure_curve_surface ();
  void render_curve_surface ();
  void path_curve (cairo_t *cr, const std::vector<float> &v);
  int samplerate = 0;
  
  c_eq_state *state = NULL;
  std::vector<float> curves [EQ_NUM_BANDS];
  std::vector<float> curve;
  
  bool curves_dirty = true;
  uint64_t curves_last_ms = 0;
  //static constexpr uint64_t CURVE_RECALC_MS = 50;
  
  cairo_surface_t *curve_surface = NULL;
  cairo_t *curve_cr = NULL;
  int curve_surface_w = 0;
  int curve_surface_h = 0;
  
  int mouse_handle = -1;
  int mouse_handle_x = -1;
  int mouse_handle_y = -1;
  int drag_handle = -1;
  int drag_orig_x = -1;
  int drag_orig_y = -1;
}; 
