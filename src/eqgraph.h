
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

class c_eqgraph : public nbtk::c_canvas {
public:
  c_eqgraph ();
  ~c_eqgraph ();
  
  void render_base (cairo_t *cr);
  void on_paint (cairo_t *cr);
  void set_state (c_eq_state *state);
  void state_changed ();
  void on_resize (int w, int h);
  void set_samplerate (int sr);
  
  float freq_min = 20.0f;
  float freq_max = 20000.0f;
  float anchor_size = 12.0f;
  
private:
  float freq_to_x (float freq);
  float x_to_freq (float x);
  float db_to_y (float gain_db);
  float y_to_db (float y);
  
  void generate_curves ();
  void path_curve (cairo_t *cr, std::vector<float> v);
  int samplerate = 48000; // TODO: DON'T FORGET TO GRAB THIS FROM UI!!!
  //c_biquad biquad [EQ_NUM_BANDS];
  
  c_eq_state *state = NULL;
  std::vector<float> curves [EQ_NUM_BANDS];
  std::vector<float> curve;
}; 
