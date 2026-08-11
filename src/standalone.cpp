
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
#include "state.h"

#ifdef HAVE_GUI
#include <atomic>
#include <thread>
#include "ui.h"
#endif

#include "data.h"

#define CMDLINE_DEBUG_COLOR ANSI_RED
#include "cmdline/debug.h"

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

static constexpr _lane_bank STANDALONE_MODEL_BANKS [] = {
  BANK_PEDAL, BANK_AMP, BANK_CAB
};

static void apply_eq_state (
    c_eq &eq, const c_eq_state &state, bool bypass) {
  eq.on = !bypass;
  eq.set_master_gain_db (state.master_gain_db);

  for (int i = 0; i < EQ_NUM_BANDS; ++i) {
    eq.set_enabled (i, state.enabled [i]);
    eq.set_band (
      i,
      state.freq [i],
      state.gain_db [i],
      state.q [i],
      state.mode [i],
      state.slope [i]);
  }
}

class c_standalone_ui : public c_neuralblender_ui {
public:
  c_standalone_ui (c_neuralblender *b) 
  : c_neuralblender_ui () {
    blender = b;
  }
  bool load_model (_lane_bank bank, size_t which, const char *filename) override;
  void on_gain_in (nbtk::c_widget *w, float f);
  void on_ir_pitch (nbtk::c_widget *w, float f);
  void on_gain_out (nbtk::c_widget *w, float f);
  void on_dry_out (nbtk::c_widget *w, float f);
  void on_delay (nbtk::c_widget *w, float f);
  void on_filebrowse (nbtk::c_widget *w);
  void on_fileselected (nbtk::c_widget *w, const char *path);
  void on_fileclear (nbtk::c_widget *w);
  void on_mute (nbtk::c_widget *w, bool b);
  void on_dcflip (nbtk::c_widget *w, bool b);
  void on_calibrate (nbtk::c_widget *w, bool b);
  void on_muteall (nbtk::c_widget *w, bool b);
  void on_vu (nbtk::c_widget *w, bool);
  void on_linked_calib (nbtk::c_widget *w, bool b);
  void on_calib_bass (nbtk::c_widget *w, bool b);
  void on_noisegate (nbtk::c_widget *w, bool b);
  void on_noisethresh (nbtk::c_widget *w, float f);
  void on_noiseattack (nbtk::c_widget *w, float f);
  void on_noisehold (nbtk::c_widget *w, float f);
  void on_noiserelease (nbtk::c_widget *w, float f);
  void on_threshgain (nbtk::c_widget *w, float f);
  void on_tuner (nbtk::c_widget *w, bool b);
  void on_tuner_base_freq (nbtk::c_widget *w, float f);
  void on_calib_target_db (nbtk::c_widget *w, float f);
  void on_master_gain (nbtk::c_widget *w, float f);
  void on_presence (nbtk::c_widget *w, float f);
  void on_eq_band (nbtk::c_widget *w, _lane_bank bank, size_t band);
  void on_eq_master_gain (nbtk::c_widget *w, _lane_bank bank, float f);
  //void on_excl (nbtk::c_widget *w, int which);
  void on_bypass (nbtk::c_widget *w, bool b);
  void on_bank_bypass (nbtk::c_widget *w, _lane_bank bank, bool b);
  void on_about (nbtk::c_widget *w);
  void apply_prefs (t_prefs &p) override;
  void write_prefs_to (t_prefs &p) override;
  void apply_effective_controls () override;
  bool set_dsp_state (const c_neuralblender_state &src) override;
  void get_dsp_state (c_neuralblender_state &dest) override;
  int idle () override;
  _lane_bank spectrum_bank = BANK_COUNT;
};

static void refresh_bank_stats (c_neuralblender_ui *ui, _lane_bank bank);

void c_standalone_ui::get_dsp_state (c_neuralblender_state &dest) {
  blender->get_state (dest);
}

bool c_standalone_ui::set_dsp_state (
    const c_neuralblender_state &src) {
  return blender && blender->set_state (src);
}

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
    if (loaded && calib_default)
      state.banks [bank].lanes [which].do_calib = true;
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

void c_standalone_ui::on_gain_in (nbtk::c_widget *w, float f) {
  debug ("lane %d, f=%f", w->lane, f);
  g_blender->set_gain_in ((_lane_bank) w->bank, w->lane, f);
}

void c_standalone_ui::on_ir_pitch (nbtk::c_widget *w, float f) {
  debug ("lane %d, f=%f", w->lane, f);
  g_blender->set_ir_pitch ((_lane_bank) w->bank, w->lane, f);
}

void c_standalone_ui::on_gain_out (nbtk::c_widget *w, float f) {
  debug ("lane %d, f=%f", w->lane, f);
  g_blender->set_gain_out ((_lane_bank) w->bank, w->lane, f);
}

void c_standalone_ui::on_dry_out (nbtk::c_widget *w, float f) {
  debug ("lane %d, f=%f", w->lane, f);
  g_blender->set_dry_out ((_lane_bank) w->bank, w->lane, f);
}

void c_standalone_ui::on_delay (nbtk::c_widget *w, float f) {
  debug ("lane %d, f=%f", w->lane, f);
  const _lane_bank bank =
    w->bank < BANK_COUNT ? (_lane_bank) w->bank : BANK_AMP;
  g_blender->set_delay_ms (bank, w->lane, f);
  refresh_bank_stats (this, bank);
  update_stats ();
}

void c_standalone_ui::on_filebrowse (nbtk::c_widget *w) {
  debug ("lane %d", w->lane);
}

void c_standalone_ui::on_fileselected (nbtk::c_widget *w, const char *path) {
  debug ("lane %d, path='%s'", w->lane, path);
  // is this the right place for this?
  //g_blender->banks [BANK_AMP].lanes [w->lane].calibrate (NULL, 0);
}

void c_standalone_ui::on_fileclear (nbtk::c_widget *w) {
  debug ("lane %d", w->lane);
  const _lane_bank bank =
    w->bank < BANK_COUNT ? (_lane_bank) w->bank : BANK_AMP;
  g_blender->unload_model (bank, w->lane);
  clear_lane_model_ui (bank, w->lane);
  if (w->lane >= 0 && w->lane < (int) NB_NUM_MODELS)
    state.banks [bank].lanes [w->lane].loaded = false;
  apply_effective_controls ();
}

void c_standalone_ui::on_mute (nbtk::c_widget *w, bool b) {
  if (w->bank < BANK_COUNT && w->lane < NB_NUM_MODELS)
    state.banks [w->bank].lanes [w->lane].lane_mute = b;
  apply_effective_controls ();
}

void c_standalone_ui::on_dcflip (nbtk::c_widget *w, bool b) {
  if (w->bank < BANK_COUNT && w->lane < NB_NUM_MODELS)
    state.banks [w->bank].lanes [w->lane].dcflip = b;
  apply_effective_controls ();
}

void c_standalone_ui::on_calibrate (nbtk::c_widget *w, bool b) { CP
  if (!w)
    return;
    
  size_t which = w->lane;
  
  if (w->bank < BANK_COUNT && which < NB_NUM_MODELS)
    state.banks [w->bank].lanes [which].do_calib = b;
  apply_effective_controls ();
  write_calib_state_if_consistent ();
  
  const _lane_bank bank =
    w->bank < BANK_COUNT ? (_lane_bank) w->bank : BANK_AMP;
  if (linked_calib_for_bank (bank))
    g_blender->calibrate_linked (bank, g_blender->calib_source == 1);
  else
    g_blender->calibrate (bank, which, g_blender->calib_source == 1);
  refresh_bank_stats (this, bank);
  update_stats ();
}

void c_standalone_ui::on_muteall (nbtk::c_widget *w, bool b) {
  debug ("lane %d, b=%d", w->lane, (int) b);
  g_blender->mute_all = b;
}

void c_standalone_ui::on_vu (nbtk::c_widget *w, bool b) {
  (void) w;
  debug ("b=%d", (int) b);
  state.do_vu = b;
  g_blender->do_vu = b;
}

void c_standalone_ui::on_noisegate (nbtk::c_widget *w, bool b) {
  (void) w;
  state.noisegate_on = b;
  g_blender->noisegate_on = b;
}

void c_standalone_ui::on_noisethresh (nbtk::c_widget *w, float value) {
  (void) w;
  state.noisethresh = value;
  g_blender->noisegate.set_threshold (value);
}

void c_standalone_ui::on_noiseattack (nbtk::c_widget *w, float value) {
  (void) w;
  state.noiseattack = value;
  g_blender->noisegate.set_attack (value);
}

void c_standalone_ui::on_noisehold (nbtk::c_widget *w, float value) {
  (void) w;
  state.noisehold = value;
  g_blender->noisegate.set_hold (value);
}

void c_standalone_ui::on_noiserelease (nbtk::c_widget *w, float value) {
  (void) w;
  state.noiserelease = value;
  g_blender->noisegate.set_release (value);
}

void c_standalone_ui::on_threshgain (nbtk::c_widget *w, float f) {
  (void) w;
  set_threshgain (f);
}

void c_standalone_ui::on_tuner (nbtk::c_widget *w, bool b) {
  (void) w;
  state.tuner_on = b;
  g_blender->tuner_on = b;
}

void c_standalone_ui::on_tuner_base_freq (nbtk::c_widget *w, float value) {
  (void) w;
  state.tuner_base_freq = value;
  g_blender->tuner_base_freq = value;
  g_blender->pitchtracker.set_base_freq ((int) lrintf (value));
}

void c_standalone_ui::on_calib_target_db (nbtk::c_widget *w, float value) {
  (void) w;
  state.calib_target_db = value;
  g_blender->set_calib_target_db (value);
}

void c_standalone_ui::on_master_gain (nbtk::c_widget *w, float value) {
  (void) w;
  state.master_gain = db_to_gain (value);
  g_blender->set_master_gain (value);
}

void c_standalone_ui::on_presence (nbtk::c_widget *w, float value) {
  (void) w;
  state.presence = value;
  g_blender->set_presence (value);
}

void c_standalone_ui::on_eq_band (
    nbtk::c_widget *w, _lane_bank bank, size_t band) {
  (void) w;
  if (!blender || band >= EQ_NUM_BANDS)
    return;

  if (bank != BANK_EQPRE && bank != BANK_EQPOST)
    return;

  const c_eq_state &eq_state = ui_eq_state_for_bank (bank);
  blender->set_eq_band (
    bank,
    band,
    eq_state.enabled [band],
    eq_state.mode [band],
    eq_state.slope [band],
    eq_state.freq [band],
    eq_state.gain_db [band],
    eq_state.q [band]);
}

void c_standalone_ui::on_eq_master_gain (
    nbtk::c_widget *w, _lane_bank bank, float value) {
  (void) w;
  if (bank != BANK_EQPRE && bank != BANK_EQPOST)
    return;

  if (blender)
    blender->set_eq_master_gain_db (bank, value);
}

void c_standalone_ui::on_linked_calib (nbtk::c_widget *w, bool b) {
  (void) w;
  set_linked_calib_for_bank (visible_bank, b);
  if (visible_bank >= BANK_PEDAL && visible_bank < BANK_COUNT)
    g_blender->banks [visible_bank].linked_calib = b;
  g_blender->linked_calib = g_blender->banks [BANK_AMP].linked_calib;
}

void c_standalone_ui::on_calib_bass (nbtk::c_widget *w, bool b) {
  (void) w;
  state.calib_source = b ? 1 : 0;
  if (blender)
    blender->calib_source = state.calib_source;
}

/* these are UI only
void c_standalone_ui::on_excl (nbtk::c_widget *w, int n) {
  debug ("lane %d, b=%d", w->lane, n);
}

void c_standalone_ui::on_bypass (nbtk::c_widget *w, bool b) {
  debug ("lane %d, b=%d", w->lane, (int) b);
  g_blender->set_bypass (!b);
}*/

void c_standalone_ui::on_bypass(nbtk::c_widget *w, bool b) {
  state.bypass = !b; // because Enabled button true means not bypassed
  apply_effective_controls();
}

void c_standalone_ui::on_bank_bypass (nbtk::c_widget *w, _lane_bank bank, bool b) {
  (void) w;
  if (!blender)
    return;

  switch (bank) {
    case BANK_PEDAL:
      blender->set_pedal_bypass (b);
    break;

    case BANK_EQPRE:
      blender->set_eq_bypass (bank, b);
    break;

    case BANK_CAB:
      blender->set_cab_bypass (b);
    break;

    case BANK_EQPOST:
      blender->set_eq_bypass (bank, b);
    break;

    case BANK_AMP:
    default:
      blender->set_amp_bypass (b);
    break;
  }
}

void c_standalone_ui::on_about (nbtk::c_widget *w) {
  debug ("lane %d", w->lane);
}

void c_standalone_ui::apply_prefs (t_prefs &p) {
  c_neuralblender_ui::apply_prefs (p);

}

void c_standalone_ui::write_prefs_to (t_prefs &p) {
  c_neuralblender_ui::write_prefs_to (p);
}

void c_standalone_ui::apply_effective_controls () {
  if (!blender)
    return;

  for (_lane_bank b : STANDALONE_MODEL_BANKS) {
    const size_t bank = (size_t) b;
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
  blender->set_pedal_bypass (state.pedal_bypass);
  apply_eq_state (blender->eq_pre, state.eqpre, state.eqpre_bypass);
  blender->set_amp_bypass (state.amp_bypass);
  apply_eq_state (blender->eq_post, state.eqpost, state.eqpost_bypass);
  blender->set_cab_bypass (state.cab_bypass);
}

int c_standalone_ui::idle () {
  if (g_blender->tuner_on)
    g_blender->pitchtracker.analyze ();

  _lane_bank next_spectrum_bank = BANK_COUNT;
  if (visible_page == PAGE_EQPRE)
    next_spectrum_bank = BANK_EQPRE;
  else if (visible_page == PAGE_EQPOST)
    next_spectrum_bank = BANK_EQPOST;

  auto spectrum_eq = [&] (_lane_bank bank) -> c_eq * {
    if (bank == BANK_EQPRE)
      return &g_blender->eq_pre;
    if (bank == BANK_EQPOST)
      return &g_blender->eq_post;
    return NULL;
  };

  if (next_spectrum_bank != spectrum_bank) {
    if (c_eq *eq = spectrum_eq (spectrum_bank))
      eq->stop_spectra ();
    if (c_eq *eq = spectrum_eq (next_spectrum_bank))
      eq->start_spectra ();
    spectrum_bank = next_spectrum_bank;
  }

  if (c_eq *eq = spectrum_eq (spectrum_bank)) {
    if (eq->analyze_spectra ()) {
      float input_db [SPECTRUM_BINS];
      float output_db [SPECTRUM_BINS];
      if (eq->copy_spectrum_input_bins (input_db, SPECTRUM_BINS) &&
          eq->copy_spectrum_output_bins (output_db, SPECTRUM_BINS)) {
        c_eqgraph &graph =
          spectrum_bank == BANK_EQPRE
            ? eqpage_pre.graph
            : eqpage_post.graph;
        graph.set_spectrum (
          input_db, output_db, SPECTRUM_BINS);
      }
    }
  }

  const float gain = g_blender->noisegate_on
    ? g_blender->noisegate.get_current_gain ()
    : 1.0f;

  set_threshgain (gain);

  return c_neuralblender_ui::idle ();
}

static std::thread ui_thread;
static std::atomic<bool> ui_started { false };

static c_standalone_ui *g_ui = nullptr;

static void save_standalone_config () {
  if (!g_ui || !g_ui->ui_ready)
    return;

  g_ui->write_prefs_to (g_ui->prefs);
  g_ui->write_calib_state_if_consistent ();
  write_prefs_to_config (g_ui->configfile, g_ui->prefs);
}

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
  c_neuralblender_state state;
  g_blender->get_state (state);
  if (g_ui->calib_default) {
    for (_lane_bank bank_id : STANDALONE_MODEL_BANKS) {
      const size_t bank = (size_t) bank_id;
      for (size_t i = 0; i < NB_NUM_MODELS; ++i)
        state.banks [bank].lanes [i].do_calib = true;
    }
  }
  g_ui->sync_widgets_from_state (state, true);
  g_ui->apply_effective_controls ();
  ui_started.store (true, std::memory_order_release);
  fprintf (stderr, "UI running...\n");
  
  CP
  while (g_running &&
      g_ui->nbtk_app.backend &&
      g_ui->nbtk_app.backend->is_running (&g_ui->app)) {
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
  char buf [1024];
  c_neuralblender_state nbstate;
  
  g_blender = new c_neuralblender;
  
  std::string nbstate_path = std::string (getenv ("HOME")) + CONFIG_STATE_NAME;
  const bool have_saved_state = nbstate.read_from (nbstate_path);
  if (have_saved_state) {
    debug ("loaded config state");
  }

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
  
  // object parse/serialize test
  c_eq_state teststate;
  teststate.on = true;
  teststate.master_gain_db = 9.5f;
  teststate.which = EQ_PRE;

  std::string s;
  teststate.to_string (s);
  teststate.from_string (s);
  exit (0);
  */
  
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
  
  const int samplerate = jack_get_sample_rate (jack_client);
  g_blender->set_samplerate (samplerate);
#ifdef HAVE_GUI
  if (g_ui)
    g_ui->set_samplerate (samplerate);
#endif
  g_blender->set_blocksize (jack_get_buffer_size (jack_client));

  if (have_saved_state && !g_blender->set_state (nbstate))
    fprintf (stderr, "NeuralBlender: some saved state values could not be restored\n");

  std::string state_models [2] = {
    g_blender->banks [BANK_AMP].lanes [0].filename,
    g_blender->banks [BANK_AMP].lanes [1].filename
  };
  if (!parse_args (argc, argv, g_blender)) {
    printf ("Error parsing command line\n");
    do_usage (argc, argv);
    jack_client_close (jack_client);
    jack_client = nullptr;
    return 1;
  }
  for (size_t lane = 0; lane < 2; ++lane) {
    const std::string &filename =
      g_blender->banks [BANK_AMP].lanes [lane].filename;
    if (!filename.empty () && filename != state_models [lane])
      g_blender->load_model (BANK_AMP, lane, filename.c_str ());
  }
  
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
    save_standalone_config ();
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
    save_standalone_config ();
#endif
    return 1;
  }
  
#ifndef HAVE_GUI
  fprintf(stderr, "NeuralBlender running. Connect ports manually. Press ctrl+C to quit.\n");
#endif
  while (g_running)
    usleep (10000);
  CP
  
  // test serialize
  g_blender->get_state (nbstate);
  nbstate.write_to (nbstate_path);
  
  jack_client_close (jack_client);
  jack_client = nullptr;
  CP
#ifdef HAVE_GUI
  if (ui_thread.joinable ())
    ui_thread.join ();
  save_standalone_config ();
#endif
  CP
  delete g_blender;
  CP
  return 0;
}
