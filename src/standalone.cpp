
/* NeuralBlender - RTNeural / NAM based amp modeler
 *
 * -----------------------------------------------------------------------------
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 * ----------------------------------------------------------------------------
 *
 * Standalone wrapper for NeuralBlender
*/

#include <jack/jack.h>
#include <signal.h>
#include <unistd.h>
#include "neuralblender.h"
//#include "timestamp.h"
#include "config.h"

#ifdef HAVE_GUI
#include <thread>
#include "ui.h"
#endif

#include "gzip.h"
#include "data.h"

#define CMDLINE_IMPLEMENTATION
#define CMDLINE_DEBUG_COLOR ANSI_RED
#include "cmdline_debug.h"

extern const char *g_build_timestamp;

static c_neuralblender g_blender;
//const char *g_build_timestamp = BUILD_TIMESTAMP;

/******************************************************************************
 * JACK stuff
 */
 
static jack_client_t *jack_client = nullptr;
static jack_port_t *jack_in = nullptr;
static jack_port_t *jack_out = nullptr;
static volatile bool g_running = true;

static int jack_process (jack_nframes_t nframes, void *) {
  float *in = (float *) jack_port_get_buffer (jack_in, nframes);
  float *out = (float *) jack_port_get_buffer (jack_out, nframes);

  g_blender.process_block (in, out, nframes);
  return 0;
}

static void jack_shutdown (void *) {
  g_running = false;
}

static void signal_handler (int) {
  g_running = false;
}

/******************************************************************************
 * UI
 */

#ifdef HAVE_GUI

class c_standalone_ui : public c_neuralblender_ui {
public:
  c_standalone_ui (c_neuralblender *b) 
  : c_neuralblender_ui () {
    blender = b;
  }
  bool load_model (size_t which, const char *filename);
  void on_gain_in (c_widget *w, float f);
  void on_gain_out (c_widget *w, float f);
  void on_dry_out (c_widget *w, float f);
  void on_delay (c_widget *w, float f);
  void on_filebrowse (c_widget *w);
  void on_fileselected (c_widget *w, const char *path);
  void on_fileclear (c_widget *w);
  void on_mute (c_widget *w, bool b);
  void on_dcflip (c_widget *w, bool b);
  void on_calibrate (c_widget *w, bool b);
  void on_muteall (c_widget *w, bool b);
  void on_vu (c_widget *w, bool);
  void on_linked_calib (c_widget *w, bool b);
  void on_calib_bass (c_widget *w, bool b);
  void on_noisegate (c_widget *w, bool b);
  void on_noisethresh (c_widget *w, float f);
  void on_noiseattack (c_widget *w, float f);
  void on_noisehold (c_widget *w, float f);
  void on_noiserelease (c_widget *w, float f);
  void on_threshgain (c_widget *w, float f);
  void on_tuner (c_widget *w, bool b);
  //void on_excl (c_widget *w, int which);
  void on_bypass (c_widget *w, bool b);
  void on_about (c_widget *w);
  void apply_prefs (t_prefs &p) override;
  void write_prefs_to (t_prefs &p) override;
  void apply_effective_controls () override;
  int idle () override;
};

bool c_standalone_ui::load_model (size_t which, const char *filename) {
  debug ("which=%d, filename='%s'", (int) which, filename);
  const bool loaded = blender->load_model (which, filename);
  if (which < NB_NUM_MODELS) {
    state.lanes [which].loaded = loaded;
    state.lanes [which].filename = loaded && filename ? filename : "";
  }
  apply_effective_controls ();
  if (which < NB_NUM_MODELS) {
    if (blender->linked_calib)
      blender->calibrate_linked (blender->calib_source == 1);
    else
      blender->calibrate (which, blender->calib_source == 1);
    stats [which * 2] = (float) blender->delays [which].frames ();
    for (size_t i = 0; i < NB_NUM_MODELS; ++i)
      stats [i * 2 + 1] = blender->amps [i].trim;
  }
  sync_widgets_from_state (state);
  return loaded;
}

void c_standalone_ui::on_gain_in (c_widget *w, float f) {
  debug ("lane %d, f=%f", w->lane, f);
  g_blender.set_gain_in (w->lane, f);
}

void c_standalone_ui::on_gain_out (c_widget *w, float f) {
  debug ("lane %d, f=%f", w->lane, f);
  g_blender.set_gain_out (w->lane, f);
}

void c_standalone_ui::on_dry_out (c_widget *w, float f) {
  debug ("lane %d, f=%f", w->lane, f);
  g_blender.set_dry_out (w->lane, f);
}

void c_standalone_ui::on_delay (c_widget *w, float f) {
  debug ("lane %d, f=%f", w->lane, f);
  g_blender.set_delay_ms (w->lane, f);
  if (w->lane < NB_NUM_MODELS)
    stats [w->lane * 2] = (float) g_blender.delays [w->lane].frames ();
  update_stats ();
}

void c_standalone_ui::on_filebrowse (c_widget *w) {
  debug ("lane %d", w->lane);
}

void c_standalone_ui::on_fileselected (c_widget *w, const char *path) {
  debug ("lane %d, path='%s'", w->lane, path);
  // is this the right place for this?
  //g_blender.amps [w->lane].calibrate (NULL, 0);
}

void c_standalone_ui::on_fileclear (c_widget *w) {
  debug ("lane %d", w->lane);
  g_blender.unload_model (w->lane);
  clear_lane_model_ui (w->lane);
  if (w->lane >= 0 && w->lane < (int) NB_NUM_MODELS)
    state.lanes [w->lane].loaded = false;
  apply_effective_controls ();
}

void c_standalone_ui::on_mute (c_widget *w, bool b) {
  state.lanes [w->lane].lane_mute = b;
  apply_effective_controls ();
}

void c_standalone_ui::on_dcflip (c_widget *w, bool b) {
  state.lanes [w->lane].dcflip = b;
  apply_effective_controls ();
}

void c_standalone_ui::on_calibrate (c_widget *w, bool b) { CP
  if (!w)
    return;
    
  size_t which = w->lane;
  
  state.lanes [which].do_calib = b;
  apply_effective_controls ();
  
  if (g_blender.linked_calib)
    g_blender.calibrate_linked (g_blender.calib_source == 1);
  else
    g_blender.calibrate (which, g_blender.calib_source == 1);
  for (size_t i = 0; i < NB_NUM_MODELS; i++) {
    stats [i * 2 + 1] = g_blender.amps [i].trim;
  }
  update_stats ();
}

void c_standalone_ui::on_muteall (c_widget *w, bool b) {
  debug ("lane %d, b=%d", w->lane, (int) b);
  g_blender.mute_all = b;
}

void c_standalone_ui::on_vu (c_widget *w, bool b) {
  debug ("b=%d", (int) b);
  g_blender.do_vu = b;
}

void c_standalone_ui::on_noisegate (c_widget *w, bool b) {
  (void) w;
  g_blender.noisegate_on = b;
}

void c_standalone_ui::on_noisethresh (c_widget *w, float value) {
  g_blender.noisegate.set_threshold (value);
}

void c_standalone_ui::on_noiseattack (c_widget *w, float value) {
  g_blender.noisegate.set_attack (value);
}

void c_standalone_ui::on_noisehold (c_widget *w, float value) {
  g_blender.noisegate.set_hold (value);
}

void c_standalone_ui::on_noiserelease (c_widget *w, float value) {
  g_blender.noisegate.set_release (value);
}

void c_standalone_ui::on_threshgain (c_widget *w, float f) {
  (void) w;
  set_threshgain (f);
}

void c_standalone_ui::on_tuner (c_widget *w, bool b) {
  (void) w;
  g_blender.tuner_on = b;
}

void c_standalone_ui::on_linked_calib (c_widget *w, bool b) {
  (void) w;
  prefs.linked_calib = b;
  g_blender.linked_calib = b;
}

void c_standalone_ui::on_calib_bass (c_widget *w, bool b) {
  (void) w;
  prefs.calib_source = b ? 1 : 0;
  if (blender)
    blender->calib_source = prefs.calib_source;
}

/* these are UI only
void c_standalone_ui::on_excl (c_widget *w, int n) {
  debug ("lane %d, b=%d", w->lane, n);
}

void c_standalone_ui::on_bypass (c_widget *w, bool b) {
  debug ("lane %d, b=%d", w->lane, (int) b);
  g_blender.set_bypass (!b);
}*/

void c_standalone_ui::on_bypass(c_widget *w, bool b) {
  state.bypass = !b; // because Enabled button true means not bypassed
  apply_effective_controls();
}

void c_standalone_ui::on_about (c_widget *w) {
  debug ("lane %d", w->lane);
}

void c_standalone_ui::apply_prefs (t_prefs &p) {
  if (blender)
    blender->set_calib_target_db (p.calib_target_db);

  c_neuralblender_ui::apply_prefs (p);

  if (blender)
    blender->do_vu = p.vu_on;
  if (blender)
    blender->linked_calib = p.linked_calib;
  if (blender)
    blender->calib_source = p.calib_source;
}

void c_standalone_ui::write_prefs_to (t_prefs &p) {
  c_neuralblender_ui::write_prefs_to (p);

  if (blender)
    p.calib_target_db = blender->amps [0].calib_target_db;
  if (blender)
    p.linked_calib = blender->linked_calib;
  if (blender)
    p.calib_source = blender->calib_source;
}

void c_standalone_ui::apply_effective_controls () {
  if (!blender)
    return;

  const bool exclusive_on = state.exclusive_lane > 0;
  const size_t excl = exclusive_on ? (size_t) (state.exclusive_lane - 1) : 0;
  const bool exclusive_empty =
    exclusive_on &&
    (excl >= NB_NUM_MODELS ||
     (!state.lanes [excl].loaded && state.lanes [excl].filename.empty ()));

  blender->set_bypass (state.bypass || exclusive_empty);

  for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
    const bool mute =
      exclusive_on && !exclusive_empty ? i != excl : state.lanes [i].lane_mute;
    blender->set_lane_mute (i, mute);
    blender->dcflip (i, state.lanes [i].dcflip);
    blender->calib_on (i, state.lanes [i].do_calib);
  }
}

int c_standalone_ui::idle () {
  const float gain = g_blender.noisegate_on
    ? g_blender.noisegate.get_current_gain ()
    : 1.0f;

  set_threshgain (gain);

  return c_neuralblender_ui::idle ();
}

static std::thread ui_thread;

static c_standalone_ui g_ui (&g_blender);

static void ui_main () {
  fprintf (stderr, "Creating UI...\n");
  g_ui.create (0);        // no LV2 parent, so root/toplevel
  c_neuralblender_state state;
  g_blender.get_state (state);
  if (g_ui.calib_default) {
    for (size_t i = 0; i < NB_NUM_MODELS; ++i)
      state.lanes [i].do_calib = true;
  }
  g_ui.sync_widgets_from_state (state);
  g_ui.apply_effective_controls ();
  fprintf (stderr, "UI running...\n");
  
  CP
  //main_run (&g_ui.app);   // blocking xputty loop
  while (g_running && g_ui.app.run) {
    g_ui.idle ();
    usleep (16777);
  }
  CP
  g_running = false;
  //exit (0);
}

#endif
 
/******************************************************************************
 * args, main etc
 */
 
void do_usage (int argc, char **argv) {
  if (argc < 1)
    return;
  char *c = argv [0];
  
  //while (*c == '.' || *c == '/')
  //  c++;
    
  printf ("NeuralBlender (%s) build timestamp %s\n", c, g_build_timestamp);
}

bool parse_args (int argc, char **argv, c_neuralblender *blender) {
  int i;
  CP
  for (i = 1; i < argc; i++) {
    if (!strcmp (argv [i], "-h") || !strcmp (argv [i], "--help")) {
      do_usage (argc, argv);
      exit (0);
    } else if (!strcmp (argv [i], "-a")) {
      if (argv [i + 1]) {
        blender->amps [0].filename = argv [++i];
      } else {
        printf ("-a needs a filename argument\n");
        return false;
      }
    } else if (!strcmp (argv [i], "-b")) {
      if (argv [i + 1]) {
        blender->amps [1].filename = argv [++i];
      } else {
        printf ("-b needs a filename argument\n");
        return false;
      }
    } else {
      printf ("don't know what to do with '%s'\n", argv [i]);
      return false;
    }
  }
  return true;
}

int main (int argc, char **argv) {
#ifndef HAVE_GUI
  signal (SIGINT, signal_handler);
  signal (SIGTERM, signal_handler);
#endif

  /*
  // tuner test
  c_tuner tuner;
  
  std::vector<float> f;
  f.resize (64);
  for (int i = 0; i < f.size (); i++)
    f [i] = sinf (2.0f * M_PI * i / 64.0f);
    //f [i] = i - 8;
  
  tuner.set_samplerate (48000);
  for (int i = 0; i < 100; i++)
    tuner.process_block (f.data (), f.size ());
  
  tuner.dump ();
  exit (0);
  */
  
  if (!parse_args (argc, argv, &g_blender)) {
    printf ("Error parsing command line\n");
    do_usage (argc, argv);
    return 1;
  }
  
  if (g_blender.amps [0].filename != "")
    g_blender.load_model (0, g_blender.amps [0].filename.c_str ());
  if (g_blender.amps [1].filename != "")
    g_blender.load_model (1, g_blender.amps [1].filename.c_str ());

  jack_client = jack_client_open ("NeuralBlender", JackNullOption, nullptr);
  if (!jack_client) {
    fprintf (stderr, "could not open JACK client\n");
    return 1;
  }

  jack_set_process_callback (jack_client, jack_process, &g_blender);
  jack_on_shutdown (jack_client, jack_shutdown, &g_blender);

  jack_in = jack_port_register (
    jack_client,
    "in",
    JACK_DEFAULT_AUDIO_TYPE,
    JackPortIsInput,
    0
  );

  jack_out = jack_port_register (
    jack_client,
    "out",
    JACK_DEFAULT_AUDIO_TYPE,
    JackPortIsOutput,
    0
  );

  if (!jack_in || !jack_out) {
    fprintf (stderr, "could not register JACK ports\n");
    jack_client_close (jack_client);
    return 1;
  }
  
  g_blender.set_samplerate (jack_get_sample_rate (jack_client));
  g_blender.set_blocksize (jack_get_buffer_size (jack_client));
  
  if (jack_activate (jack_client)) {
    fprintf (stderr, "could not activate JACK client\n");
    jack_client_close (jack_client);
    return 1;
  }
  
#ifdef HAVE_GUI
  auto ui_thead = std::thread (ui_main);
  CP
#else
  fprintf(stderr, "NeuralBlender running. Connect ports manually. Press ctrl+C to quit.\n");
#endif
  while (g_running)
    usleep (10000);
  CP
  //ui_thread.join ();
  CP
  jack_client_close (jack_client);
  CP
  exit (0);
  //return 0;
}
