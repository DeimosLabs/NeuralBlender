
/* NeuralBlender - tuner / pitch tracking data.
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <deque>

#include "meter.h"

#define INTUNE_THRESHOLD   2 // in cents
#define INTUNE_DELAY       5 // frames/redraws

class c_pitchtracker {
public:
  c_pitchtracker ();
  ~c_pitchtracker ();
  void set_samplerate (int sr);
  void process_block (float *in, int nframes);
  bool analyze ();
  void dump ();
  
  void set_base_freq (int f = 440);
  void set_block_size (size_t sz);
  void set_pitchtracker (c_pitchtracker *p);
  
  std::atomic<float> detected_freq  { 0.0f };
  std::atomic<float> detected_note  { 0.0f };
  std::atomic<float> detected_cents { 0.0f };
  std::atomic<bool> needs_redraw { false };
  
private:
  void publish_snapshot ();
  
  inline float get_lag_score (const std::vector<float> &buf, size_t lag) const;
  inline float sample_at (size_t i) const;
  int get_best_lag (const std::vector<float> &buf, 
                    int step, float *r_score = NULL);
  void update_note_from_freq (float freq);
  
  std::vector<float> ring;
  std::vector<float> snapshots [3];
  std::vector<float> analysis;
  std::vector<float> lag_scores;
  
  size_t count       = 0;
  int write_snapshot = 0;
  int samplerate     = 48000;
  int basefreq       = 440;
  int lastfreq       = -1;
  
  std::atomic<int> published_snapshot { -1 };
  std::atomic<uint64_t> published_seq {  0 };
};

#if defined (HAVE_GUI)

#include "widgets.h"

class c_tunerwidget : public nbtk::c_canvas {
public:
  void create (nbtk::c_widget *parent,
               const char *label,
               int x, int y, int w, int h);

  void show ();
  void hide ();
  void move_resize (int x, int y, int w, int h) override;
  void move (int x, int y) override;
  void resize (int w, int h) override;

  void set_pitchtracker (c_pitchtracker *p);
  void set_pitch (float freq, float note, float cents);
  void on_ui_timer ();
  bool needs_redraw ();
  void set_base_freq (float f) { if (pitchtracker) pitchtracker->set_base_freq (f); }

  bool created = false;
  int width = 0;
  int height = 0;

protected:
  void render_base (cairo_t *cr) override;
  void on_paint (cairo_t *cr) override;
  void on_resize (int w, int h) override;

  std::deque<int> hist_notes;
  std::deque<int> hist_cents;

  c_pitchtracker *pitchtracker = NULL;
  float current_freq = 0.0f;
  float current_note = 0.0f;
  float current_cents = 0.0f;
  std::atomic<bool> ui_needs_redraw { false };

};

#endif // HAVE_GUI
