
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
  
  float freq_min = 20.0f;
  float freq_max = 20000.0f;
  
private:
  c_eq_state *state = NULL;
  
  int freq_to_x (float freq);
  float x_to_freq (int x);
  int db_to_y (float gain_db);
  float y_to_db (int y);
}; 
