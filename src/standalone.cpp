
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
#include <cmath>
#include <signal.h>
#include <unistd.h>
#include "neuralblender.h"
//#include "timestamp.h"
#include "config.h"

#ifdef HAVE_GUI
#include <atomic>
#include <thread>
#include "ui.h"
#endif

#include "gzip.h"
#include "data.h"

#define CMDLINE_IMPLEMENTATION
#define CMDLINE_DEBUG_COLOR ANSI_RED
#include "cmdline_debug.h"

extern const char *g_build_timestamp;

static c_neuralblender *g_blender = nullptr;
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

  g_blender->process_block (in, out, nframes);
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
  bool load_model (_lane_bank bank, size_t which, const char *filename) override;
  void on_gain_in (c_widget *w, float f);
  void on_ir_pitch (c_widget *w, float f);
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
  void on_tuner_base_freq (c_widget *w, float f);
  void on_calib_target_db (c_widget *w, float f);
  void on_master_gain (c_widget *w, float f);
  void on_presence (c_widget *w, float f);
  //void on_excl (c_widget *w, int which);
  void on_bypass (c_widget *w, bool b);
  void on_about (c_widget *w);
  void apply_prefs (t_prefs &p) override;
  void write_prefs_to (t_prefs &p) override;
  void apply_effective_controls () override;
  int idle () override;
};

static void refresh_bank_stats (c_neuralblender_ui *ui, _lane_bank bank);

bool c_standalone_ui::load_model (
    _lane_bank bank, size_t which, const char *filename) {
  debug ("bank=%d, which=%d, filename='%s'",
         (int) bank, (int) which, filename);
  if (bank < BANK_PEDAL || bank >= BANK_COUNT)
    bank = BANK_AMP;

  const bool loaded = blender->load_model (bank, which, filename);
  if (which < NB_NUM_MODELS) {
    state.banks [bank].lanes [which].loaded = loaded;
    state.banks [bank].lanes [which].filename =
      loaded && filename ? filename : "";
  }
  apply_effective_controls ();
  if (which < NB_NUM_MODELS) {
    if (linked_calib_for_bank (bank))
      blender->calibrate_linked (bank, blender->calib_source == 1);
    else
      blender->calibrate (bank, which, blender->calib_source == 1);
    refresh_bank_stats (this, bank);
  }
  sync_widgets_from_state (state);
  return loaded;
}

void c_standalone_ui::on_gain_in (c_widget *w, float f) {
  debug ("lane %d, f=%f", w->lane, f);
  g_blender->set_gain_in ((_lane_bank) w->bank, w->lane, f);
}

void c_standalone_ui::on_ir_pitch (c_widget *w, float f) {
  debug ("lane %d, f=%f", w->lane, f);
  g_blender->set_ir_pitch ((_lane_bank) w->bank, w->lane, f);
}

void c_standalone_ui::on_gain_out (c_widget *w, float f) {
  debug ("lane %d, f=%f", w->lane, f);
  g_blender->set_gain_out ((_lane_bank) w->bank, w->lane, f);
}

void c_standalone_ui::on_dry_out (c_widget *w, float f) {
  debug ("lane %d, f=%f", w->lane, f);
  g_blender->set_dry_out ((_lane_bank) w->bank, w->lane, f);
}

void c_standalone_ui::on_delay (c_widget *w, float f) {
  debug ("lane %d, f=%f", w->lane, f);
  const _lane_bank bank =
    w->bank < BANK_COUNT ? (_lane_bank) w->bank : BANK_AMP;
  g_blender->set_delay_ms (bank, w->lane, f);
  refresh_bank_stats (this, bank);
  update_stats ();
}

void c_standalone_ui::on_filebrowse (c_widget *w) {
  debug ("lane %d", w->lane);
}

void c_standalone_ui::on_fileselected (c_widget *w, const char *path) {
  debug ("lane %d, path='%s'", w->lane, path);
  // is this the right place for this?
  //g_blender->banks [BANK_AMP].lanes [w->lane].calibrate (NULL, 0);
}

void c_standalone_ui::on_fileclear (c_widget *w) {
  debug ("lane %d", w->lane);
  const _lane_bank bank =
    w->bank < BANK_COUNT ? (_lane_bank) w->bank : BANK_AMP;
  g_blender->unload_model (bank, w->lane);
  clear_lane_model_ui (bank, w->lane);
  if (w->lane >= 0 && w->lane < (int) NB_NUM_MODELS)
    state.banks [bank].lanes [w->lane].loaded = false;
  apply_effective_controls ();
}

void c_standalone_ui::on_mute (c_widget *w, bool b) {
  if (w->bank < BANK_COUNT && w->lane < NB_NUM_MODELS)
    state.banks [w->bank].lanes [w->lane].lane_mute = b;
  apply_effective_controls ();
}

void c_standalone_ui::on_dcflip (c_widget *w, bool b) {
  if (w->bank < BANK_COUNT && w->lane < NB_NUM_MODELS)
    state.banks [w->bank].lanes [w->lane].dcflip = b;
  apply_effective_controls ();
}

void c_standalone_ui::on_calibrate (c_widget *w, bool b) { CP
  if (!w)
    return;
    
  size_t which = w->lane;
  
  if (w->bank < BANK_COUNT && which < NB_NUM_MODELS)
    state.banks [w->bank].lanes [which].do_calib = b;
  apply_effective_controls ();
  
  const _lane_bank bank =
    w->bank < BANK_COUNT ? (_lane_bank) w->bank : BANK_AMP;
  if (linked_calib_for_bank (bank))
    g_blender->calibrate_linked (bank, g_blender->calib_source == 1);
  else
    g_blender->calibrate (bank, which, g_blender->calib_source == 1);
  refresh_bank_stats (this, bank);
  update_stats ();
}

void c_standalone_ui::on_muteall (c_widget *w, bool b) {
  debug ("lane %d, b=%d", w->lane, (int) b);
  g_blender->mute_all = b;
}

void c_standalone_ui::on_vu (c_widget *w, bool b) {
  debug ("b=%d", (int) b);
  g_blender->do_vu = b;
}

void c_standalone_ui::on_noisegate (c_widget *w, bool b) {
  (void) w;
  prefs.noisegate_on = b;
  g_blender->noisegate_on = b;
}

void c_standalone_ui::on_noisethresh (c_widget *w, float value) {
  (void) w;
  state.noisethresh = value;
  prefs.noisethresh = value;
  write_prefs_to_config (configfile, prefs);
  g_blender->noisegate.set_threshold (value);
}

void c_standalone_ui::on_noiseattack (c_widget *w, float value) {
  (void) w;
  state.noiseattack = value;
  prefs.noiseattack = value;
  write_prefs_to_config (configfile, prefs);
  g_blender->noisegate.set_attack (value);
}

void c_standalone_ui::on_noisehold (c_widget *w, float value) {
  (void) w;
  state.noisehold = value;
  prefs.noisehold = value;
  write_prefs_to_config (configfile, prefs);
  g_blender->noisegate.set_hold (value);
}

void c_standalone_ui::on_noiserelease (c_widget *w, float value) {
  (void) w;
  state.noiserelease = value;
  prefs.noiserelease = value;
  write_prefs_to_config (configfile, prefs);
  g_blender->noisegate.set_release (value);
}

void c_standalone_ui::on_threshgain (c_widget *w, float f) {
  (void) w;
  set_threshgain (f);
}

void c_standalone_ui::on_tuner (c_widget *w, bool b) {
  (void) w;
  g_blender->tuner_on = b;
}

void c_standalone_ui::on_tuner_base_freq (c_widget *w, float value) {
  (void) w;
  state.tuner_base_freq = value;
  prefs.tuner_base_freq = value;
  write_prefs_to_config (configfile, prefs);
  g_blender->tuner_base_freq = value;
  g_blender->pitchtracker.set_base_freq ((int) lrintf (value));
}

void c_standalone_ui::on_calib_target_db (c_widget *w, float value) {
  (void) w;
  prefs.calib_target_db = value;
  write_prefs_to_config (configfile, prefs);
  g_blender->set_calib_target_db (value);
}

void c_standalone_ui::on_master_gain (c_widget *w, float value) {
  (void) w;
  state.master_gain = db_to_gain (value);
  g_blender->set_master_gain (value);
}

void c_standalone_ui::on_presence (c_widget *w, float value) {
  (void) w;
  state.presence = value;
  g_blender->set_presence (value);
}

void c_standalone_ui::on_linked_calib (c_widget *w, bool b) {
  (void) w;
  set_linked_calib_for_bank (visible_bank, b);
  if (visible_bank == BANK_AMP)
    prefs.linked_calib = b;
  if (visible_bank >= BANK_PEDAL && visible_bank < BANK_COUNT)
    g_blender->banks [visible_bank].linked_calib = b;
  g_blender->linked_calib = g_blender->banks [BANK_AMP].linked_calib;
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
  g_blender->set_bypass (!b);
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
    blender->tuner_on = p.tuner_on;
  if (blender) {
    blender->noisegate_on = p.noisegate_on;
    blender->noisegate.set_threshold (p.noisethresh);
    blender->noisegate.set_attack (p.noiseattack);
    blender->noisegate.set_hold (p.noisehold);
    blender->noisegate.set_release (p.noiserelease);
  }
  if (blender) {
    blender->tuner_base_freq = p.tuner_base_freq;
    blender->pitchtracker.set_base_freq ((int) lrintf (p.tuner_base_freq));
  }
  if (blender)
    for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank)
      blender->banks [bank].linked_calib =
        linked_calib_for_bank ((_lane_bank) bank);
  if (blender)
    blender->linked_calib = blender->banks [BANK_AMP].linked_calib;
  if (blender)
    blender->calib_source = p.calib_source;
}

void c_standalone_ui::write_prefs_to (t_prefs &p) {
  c_neuralblender_ui::write_prefs_to (p);

  if (blender)
    p.calib_target_db = blender->banks [BANK_AMP].lanes [0].calib_target_db;
  if (blender)
    p.tuner_on = blender->tuner_on;
  if (blender) {
    p.noisegate_on = blender->noisegate_on;
    p.noisethresh = blender->noisegate.threshold_db;
    p.noiseattack = blender->noisegate.attack_ms;
    p.noisehold = blender->noisegate.hold_ms;
    p.noiserelease = blender->noisegate.release_ms;
  }
  if (blender)
    p.tuner_base_freq = blender->tuner_base_freq;
  if (blender)
    p.linked_calib = linked_calib_for_bank (BANK_AMP);
  if (blender)
    p.calib_source = blender->calib_source;
}

void c_standalone_ui::apply_effective_controls () {
  if (!blender)
    return;

  for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
    const _lane_bank b = (_lane_bank) bank;
    c_neuralblender_bank_state &bank_state = state.banks [bank];
    blender->banks [bank].linked_calib = bank_state.linked_calib;
    blender->set_exclusive_lane (b, bank_state.exclusive_lane);

    const int exclusive_lane = exclusive_lane_for_bank (b);
    const bool exclusive_on = exclusive_lane > 0;
    const size_t excl = exclusive_on ? (size_t) (exclusive_lane - 1) : 0;
    const bool exclusive_empty =
      exclusive_on &&
      (excl >= NB_NUM_MODELS ||
       (!bank_state.lanes [excl].loaded &&
        bank_state.lanes [excl].filename.empty ()));

    for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
      const bool mute =
        exclusive_on && !exclusive_empty
          ? i != excl
          : bank_state.lanes [i].lane_mute;
      blender->set_lane_mute (b, i, mute);
      blender->dcflip (b, i, bank_state.lanes [i].dcflip);
      blender->calib_on (b, i, bank_state.lanes [i].do_calib);
    }
  }

  blender->linked_calib = blender->banks [BANK_AMP].linked_calib;
  blender->set_bypass (state.bypass);
}

int c_standalone_ui::idle () {
  if (g_blender->tuner_on)
    g_blender->pitchtracker.analyze ();

  const float gain = g_blender->noisegate_on
    ? g_blender->noisegate.get_current_gain ()
    : 1.0f;

  set_threshgain (gain);

  return c_neuralblender_ui::idle ();
}

static std::thread ui_thread;
static std::atomic<bool> ui_started { false };

static c_standalone_ui *g_ui = nullptr;

static void refresh_bank_stats (c_neuralblender_ui *ui, _lane_bank bank) {
  if (!ui || !ui->blender)
    return;
  if (bank < BANK_PEDAL || bank >= BANK_COUNT)
    bank = BANK_AMP;

  for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
    const size_t n = i * UI_STATS_PER_LANE;
    c_neuralamp &amp = ui->blender->banks [bank].lanes [i];
    ui->stats [bank] [n] = (float) amp.delay.frames ();
    ui->stats [bank] [n + 1] = amp.trim.load (std::memory_order_acquire);
    ui->stats [bank] [n + 2] = (float) amp.engine ();
  }
}

static void ui_main () {
  fprintf (stderr, "Creating UI...\n");
  if (!g_ui->create (0)) { // no LV2 parent, so root/toplevel
    ui_started.store (true, std::memory_order_release);
    g_running = false;
    return;
  }
  g_blender->do_vu = g_ui->prefs.vu_on;
  g_blender->tuner_on = g_ui->prefs.tuner_on;
  g_blender->noisegate_on = g_ui->prefs.noisegate_on;
  g_blender->noisegate.set_threshold (g_ui->prefs.noisethresh);

  c_neuralblender_state state;
  g_blender->get_state (state);
  if (g_ui->calib_default) {
    for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank)
      for (size_t i = 0; i < NB_NUM_MODELS; ++i)
        state.banks [bank].lanes [i].do_calib = true;
  }
  g_ui->sync_widgets_from_state (state);
  g_ui->apply_effective_controls ();
  ui_started.store (true, std::memory_order_release);
  fprintf (stderr, "UI running...\n");
  
  CP
  //main_run (&g_ui->app);   // blocking xputty loop
  while (g_running && g_ui->app.run) {
    g_ui->idle ();
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
        blender->banks [BANK_AMP].lanes [0].filename = argv [++i];
      } else {
        printf ("-a needs a filename argument\n");
        return false;
      }
    } else if (!strcmp (argv [i], "-b")) {
      if (argv [i + 1]) {
        blender->banks [BANK_AMP].lanes [1].filename = argv [++i];
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
  g_blender = new c_neuralblender;

#ifdef HAVE_GUI
  g_ui = new c_standalone_ui (g_blender);
#endif

#ifndef HAVE_GUI
  signal (SIGINT, signal_handler);
  signal (SIGTERM, signal_handler);
#endif

  /*
  // tuner/pitch tracker test
  c_pitchtracker pitchtracker;
  
  std::vector<float> f;
  f.resize (64);
  for (int i = 0; i < f.size (); i++)
    f [i] = sinf (2.0f * M_PI * i / 64.0f);
    //f [i] = i - 8;
  
  pitchtracker.set_samplerate (48000);
  for (int i = 0; i < 100; i++)
    pitchtracker.process_block (f.data (), f.size ());
  
  pitchtracker.dump ();
  exit (0);
  */
  
  if (!parse_args (argc, argv, g_blender)) {
    printf ("Error parsing command line\n");
    do_usage (argc, argv);
    return 1;
  }
  
  if (g_blender->banks [BANK_AMP].lanes [0].filename != "")
    g_blender->load_model (BANK_AMP, 0, g_blender->banks [BANK_AMP].lanes [0].filename.c_str ());
  if (g_blender->banks [BANK_AMP].lanes [1].filename != "")
    g_blender->load_model (BANK_AMP, 1, g_blender->banks [BANK_AMP].lanes [1].filename.c_str ());

  jack_client = jack_client_open ("NeuralBlender", JackNullOption, nullptr);
  if (!jack_client) {
    fprintf (stderr, "could not open JACK client\n");
    return 1;
  }

  jack_set_process_callback (jack_client, jack_process, g_blender);
  jack_on_shutdown (jack_client, jack_shutdown, g_blender);

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
  
  g_blender->set_samplerate (jack_get_sample_rate (jack_client));
  g_blender->set_blocksize (jack_get_buffer_size (jack_client));
  
#ifdef HAVE_GUI
  ui_thread = std::thread (ui_main);
  while (g_running && !ui_started.load (std::memory_order_acquire))
    usleep (10000);
  CP
#endif

  if (!g_running) {
    jack_client_close (jack_client);
    jack_client = nullptr;
#ifdef HAVE_GUI
    if (ui_thread.joinable ())
      ui_thread.join ();
#endif
    return 0;
  }

  if (jack_activate (jack_client)) {
    fprintf (stderr, "could not activate JACK client\n");
    jack_client_close (jack_client);
#ifdef HAVE_GUI
    g_running = false;
    if (ui_thread.joinable ())
      ui_thread.join ();
#endif
    return 1;
  }
  
#ifndef HAVE_GUI
  fprintf(stderr, "NeuralBlender running. Connect ports manually. Press ctrl+C to quit.\n");
#endif
  while (g_running)
    usleep (10000);
  CP
  jack_client_close (jack_client);
  jack_client = nullptr;
  CP
#ifdef HAVE_GUI
  if (ui_thread.joinable ())
    ui_thread.join ();
#endif
  CP
  return 0;
}
