
/* NeuralBlender - RTNeural / NAM based amp modeler
 *
 * Shared UI code
 */

#include <string.h>
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <dirent.h>
#include <sys/stat.h>
#include <chrono>

#include "neuralblender.h"
#include "ui.h"

#include "data/data.h"

#define CMDLINE_DEBUG_COLOR ANSI_MAGENTA
#include "cmdline_debug.h"

#define MIN_WINDOW_HEIGHT (100 + (130 * NB_NUM_MODELS))
//#define DEFAULT_WINDOW_HEIGHT (12 + std::min (640, (52 + (180 * NB_NUM_MODELS))))
#define DEFAULT_WINDOW_HEIGHT MIN_WINDOW_HEIGHT
#define MIN_WINDOW_WIDTH 640
#define DEFAULT_WINDOW_WIDTH MIN_WINDOW_WIDTH

#define METER_WIDTH 6

extern const char *g_build_timestamp;

#ifdef HAVE_GZIP
#define UI_GZIP_SUFFIX(suffix) suffix "|" suffix ".gz"
#else
#define UI_GZIP_SUFFIX(suffix) suffix
#endif

static bool page_has_bank (_ui_page page);
static _lane_bank bank_for_page (_ui_page page);
static const char *cwd_config_key_for_bank_ui (_lane_bank bank);
static bool bank_bypass_for_state (
    const c_neuralblender_state &state,
    _lane_bank bank);
static void sync_bank_tab_icon (
    nbtk::c_button &button,
    const c_neuralblender_state &state,
    _lane_bank bank);

static constexpr _lane_bank MODEL_BANKS [] = {
  BANK_PEDAL, BANK_AMP, BANK_CAB
};

static bool filename_ends_with (const std::string &path, const char *suffix) {
  const size_t suffix_len = strlen (suffix);
  return path.size () >= suffix_len &&
         path.rfind (suffix) == path.size () - suffix_len;
}

static bool is_supported_model_filename_lower (const std::string &lower) {
  return filename_ends_with (lower, ".nam") ||
#ifdef HAVE_GZIP
         filename_ends_with (lower, ".nam.gz") ||
#endif
         filename_ends_with (lower, ".json") ||
#ifdef HAVE_GZIP
         filename_ends_with (lower, ".json.gz") ||
#endif
         filename_ends_with (lower, ".aidax") ||
         filename_ends_with (lower, ".wav");
#ifdef HAVE_GZIP
         filename_ends_with (lower, ".aidax") ||
         filename_ends_with (lower, ".wav.gz");
#endif
}

bool c_neuralblendermainwindow::create (
    c_neuralblender_ui *ui_,
    nbtk::t_native_window parent_,
    const char *title_,
    int x, int y, int w, int h,
    nbtk::t_native_handle owner) {

  ui = ui_;
  if (!c_toplevelwindow::create (&ui_->nbtk_app, parent_, title_, x, y, w, h, owner))
    return false;

  auto_close (false);
  auto_hide_on_close (false);
  auto_quit_on_close (true);
  ui_->window = window;
  return true;
}

void c_neuralblendermainwindow::show () {
  if (!widget)
    return;

  children_mapped = false;
  nbtk::c_toplevelwindow::show ();
}

void c_neuralblendermainwindow::show_children () {
  if (!widget || children_mapped)
    return;

  children_mapped = true;
  if (ui)
    ui->sync_page_visibility ();
}

void c_neuralblendermainwindow::on_expose () {
  if (!widget)
    return;

  nbtk::c_toplevelwindow::on_expose ();
  show_children ();
}

void c_neuralblendermainwindow::on_resize () { CP
  if (!widget || !ui)
    return;

  int width = 0;
  int height = 0;
  bool visible = false;
  if (!get_metrics (&width, &height, &visible) || !visible)
    return;

  debug ("mainwindow resize: metrics=%d,%d init=%d,%d",
         width, height,
         widget->scale.init_width, widget->scale.init_height);
  nbtk::c_toplevelwindow::on_resize ();
  ui->on_window_resize (width, height);
}

void c_neuralblendermainwindow::on_configure_notify () {
  if (!ui)
    return;

  nbtk::c_toplevelwindow::on_configure_notify ();
  ui->on_window_configured ();
}

void c_neuralblendermainwindow::on_action (nbtk::t_action_event &event) {
  if (ui)
    ui->on_action (event);
}

bool is_supported_model_filename (const std::string &path) {
  std::string lower = path;
  std::transform (lower.begin (), lower.end (), lower.begin (),
                  [] (unsigned char c) { return (char) std::tolower (c); });

  return is_supported_model_filename_lower (lower);
}

static const char *ui_model_filter () {
  return
    UI_GZIP_SUFFIX ("*.nam") "|"
    UI_GZIP_SUFFIX ("*.json") "|"
    "*.aidax";
}

static const char *ui_wav_filter () {
  return UI_GZIP_SUFFIX ("*.wav");
}

static bool parse_config_float (const std::string &s, float &value) {
  if (s.empty ())
    return false;

  char *end = NULL;
  errno = 0;
  const float parsed = std::strtof (s.c_str (), &end);
  if (errno || end == s.c_str () || *end != '\0' || !std::isfinite (parsed))
    return false;

  value = parsed;
  return true;
}

static void format_db_text (char *buf, size_t size, float value) {
  snprintf (buf, size, "%.1f", value);
}

static void format_freq_text (char *buf, size_t size, float value) {
  snprintf (buf, size, "%.3f", value);
}

bool read_prefs_from_config (c_configfile &configfile, t_prefs &prefs) {
  float vu_scale_db = prefs.vu_scale_db;
  if (parse_config_float (
      configfile.get_item (CONFIG_KEY_NAME_VU_SCALE),
      vu_scale_db) &&
      vu_scale_db <= 0.0f)
    prefs.vu_scale_db = vu_scale_db;

  float vu_headroom_db = prefs.vu_headroom_db;
  if (parse_config_float (
      configfile.get_item (CONFIG_KEY_NAME_VU_HEADROOM),
      vu_headroom_db) &&
      vu_headroom_db >= 0.0f &&
      vu_headroom_db <= 12.0f)
    prefs.vu_headroom_db = vu_headroom_db;

  std::string str = configfile.get_item (CONFIG_KEY_NAME_BYP_DCLICK);
  if (!str.empty ())
    prefs.bypass_doubleclick = c_configfile::istrue (str);

  str = configfile.get_item (CONFIG_KEY_NAME_BYP_RCLICK);
  if (!str.empty ())
    prefs.bypass_rightclick = c_configfile::istrue (str);

  str = configfile.get_item (CONFIG_KEY_NAME_CALIB);
  if (!str.empty ())
    prefs.calib_default = c_configfile::istrue (str);

  str = configfile.get_item (CONFIG_KEY_NAME_TOOLTIPS);
  if (!str.empty ())
    prefs.show_tooltips = c_configfile::istrue (str);

  return true;
}

bool write_prefs_to_config (c_configfile &configfile, const t_prefs &prefs) {
  char buf [128];
  snprintf (buf, sizeof (buf), "%.6g", prefs.vu_scale_db);
  configfile.set_item (CONFIG_KEY_NAME_VU_SCALE, buf);

  snprintf (buf, sizeof (buf), "%.6g", prefs.vu_headroom_db);
  configfile.set_item (CONFIG_KEY_NAME_VU_HEADROOM, buf);

  configfile.set_item (
    CONFIG_KEY_NAME_BYP_DCLICK, prefs.bypass_doubleclick ? "1" : "0");
  configfile.set_item (
    CONFIG_KEY_NAME_BYP_RCLICK, prefs.bypass_rightclick ? "1" : "0");
  configfile.set_item (
    CONFIG_KEY_NAME_CALIB, prefs.calib_default ? "1" : "0");
  configfile.set_item (
    CONFIG_KEY_NAME_TOOLTIPS, prefs.show_tooltips ? "1" : "0");

  return configfile.write_file ();
}

////////////////////////////////////////////////////////////////////////////////
// c_prefswindow

void c_prefswindow::create (c_neuralblender_ui *ui_) { CP
  ui = ui_;
  if (!ui || !ui->ui_ready || widget)
    return;
  
  int default_w = 550;
  int default_h = 550;
  nbtk::t_native_window root =
      ui->nbtk_app.backend
        ? ui->nbtk_app.backend->root_window (ui->mainwindow.native_handle (), false)
        : 0;
  
  if (!c_toplevelwindow::create (
      &ui->nbtk_app,
      root,
      "NeuralBlender settings",
      0, 0, default_w, default_h,
      ui->mainwindow.native_handle ()))
    return;
  set_min_size_to_current ();

  frame1.create (&root_widget, "", 12, 12, w () - 24, h () - 80);

  label_vuscale.create (&frame1, "VU meter scale dB:", 16, 32, 180, 32);
  label_vuscale.align = nbtk::TEXT_LEFT;
  label_vuheadroom.create (&frame1, "VU meter headroom dB:", 16, 72, 180, 32);
  label_vuheadroom.align = nbtk::TEXT_LEFT;
  label_spacer1.create (&frame1, "", 16, 112, 12, 12);
  text_vuscale.create (&frame1, "", 220, 28, 120, 36);
  text_vuheadroom.create (&frame1, "", 220, 68, 120, 36);

  btn_calib_default.create (
    &frame1, "Calibrate default on", 16, 152, 320, 32);
  btn_calib_default.align = nbtk::TEXT_LEFT;
  btn_bypass_doubleclick.create (
    &frame1, "Toggle bypass on doubleclick", 16, 192, 320, 32);
  btn_bypass_doubleclick.align = nbtk::TEXT_LEFT;
  btn_bypass_rightclick.create (
    &frame1, "Toggle bypass on right click", 16, 232, 320, 32);
  btn_bypass_rightclick.align = nbtk::TEXT_LEFT;
  btn_show_tooltips.create (
    &frame1, "Show tooltips", 16, 272, 320, 32);
  btn_show_tooltips.align = nbtk::TEXT_LEFT;
  btn_defaults.create (&frame1, "Reset to defaults", 12, 0, 400, 40);
  
  text_vuscale.set_tooltip ("Minimum dB value visible on VU meters");
  text_vuheadroom.set_tooltip ("Extra headroom given to VU meters above 0dB");
  btn_calib_default.set_tooltip ("Enable calibration by default for newly loaded lanes");
  btn_bypass_doubleclick.set_tooltip ("Toggle bypassing banks when clicking its top button again");
  btn_bypass_rightclick.set_tooltip ("Toggle bypassing banks when right-clicking its top button");
  btn_show_tooltips.set_tooltip ("No idea what this does");
  
  btn_cancel.create (&root_widget, "Cancel", 0, 0, 128, 40);
  btn_cancel.set_image_default (data_icon_xputty_cancel_png);
  btn_ok.create (&root_widget, "OK", 0, 0, 128, 40);
  btn_ok.set_image_default (data_icon_xputty_approved_png);
  on_resize ();
}

void c_prefswindow::on_resize () { CP
  nbtk::c_toplevelwindow::on_resize ();
  frame1.move_resize (12, 12, w () - 24, h () - 80);
  
  // bottom about/ok/cancel buttons
  btn_ok.move_resize (w () - 140, h () - 56, 128, 40);
  btn_cancel.move_resize (w () - 280, h () - 56, 128, 40);
  btn_defaults.move_resize (12, frame1.h - 50, frame1.w - 24, 40);
}

bool c_prefswindow::on_key_down (int key) {
  if (key == nbtk::KEY_ESCAPE) {
    hide ();
    return true;
  }

  return nbtk::c_toplevelwindow::on_key_down (key);
}

void c_prefswindow::on_action (nbtk::t_action_event &event) {
  if (event.mouse_button != 0 && event.mouse_button != Button1)
    return;

  if (event.source_id == btn_defaults.id) {
    load_defaults ();
    event.handled = true;
  } else if (event.source_id == btn_ok.id) {
    set_prefs_to (ui->prefs);
    ui->apply_prefs (ui->prefs);
    ui->on_prefs_ok ();
    hide ();
    event.handled = true;
  } else if (event.source_id == btn_cancel.id) {
    hide ();
    event.handled = true;
  }
}

void c_prefswindow::show () { CP
  if (!widget)
    create (ui);
  
  nbtk::c_toplevelwindow::show ();
}

void c_prefswindow::hide () { CP
  nbtk::c_toplevelwindow::hide ();
}

void c_prefswindow::load_defaults () {
  text_vuscale.set_text ("-48.0");
  text_vuheadroom.set_text ("6.0");
  btn_calib_default.set_value (false);
  btn_bypass_doubleclick.set_value (false);
  btn_bypass_rightclick.set_value (true);
  btn_show_tooltips.set_value (true);
}

void c_prefswindow::get_prefs_from (t_prefs &prefs) { CP
  if (!widget)
    create (ui);

  char buf [128];
  format_db_text (buf, sizeof (buf), prefs.vu_scale_db);
  text_vuscale.set_text (buf);

  format_db_text (buf, sizeof (buf), prefs.vu_headroom_db);
  text_vuheadroom.set_text (buf);
  
  btn_bypass_doubleclick.set_value (prefs.bypass_doubleclick);
  btn_bypass_rightclick.set_value (prefs.bypass_rightclick);
  btn_calib_default.set_value (prefs.calib_default);
  btn_show_tooltips.set_value (prefs.show_tooltips);
}

void c_prefswindow::set_prefs_to (t_prefs &prefs) {
  float vu_scale_db = 0.0f;
  if (parse_config_float (text_vuscale.value, vu_scale_db) &&
      vu_scale_db <= 0.0f)
    prefs.vu_scale_db = vu_scale_db;

  float vu_headroom_db = 0.0f;
  if (parse_config_float (text_vuheadroom.value, vu_headroom_db) &&
      vu_headroom_db >= 0.0f &&
      vu_headroom_db <= 12.0f)
    prefs.vu_headroom_db = vu_headroom_db;
  
  prefs.bypass_doubleclick = btn_bypass_doubleclick.value;
  prefs.bypass_rightclick = btn_bypass_rightclick.value;
  prefs.calib_default = btn_calib_default.value;
  prefs.show_tooltips = btn_show_tooltips.value;
}

////////////////////////////////////////////////////////////////////////////////
// c_aboutwindow

static const char *g_about_text [] = {
#ifdef LV2
  "An amp modeling plugin based on",
#else
  "An amp modeling app based on",
#endif
  "RTNeural and NeuralAmpModeler",
  "",
  "by Deimos Laboratories",
  "http://deimos.ca/neuralblender", // web link, see below
  "https://github.com/DeimosLabs/NeuralBlender",
  NULL
};

void c_aboutwindow::create (c_neuralblender_ui *ui_) { CP
  ui = ui_;
  if (!ui || !ui->ui_ready || widget)
    return;

  nbtk::t_native_window native_root =
      ui->nbtk_app.backend
        ? ui->nbtk_app.backend->root_window (ui->mainwindow.native_handle (), false)
        : 0;

  if (!c_toplevelwindow::create (
      &ui->nbtk_app,
      native_root,
      "About NeuralBlender (tk)",
      470, 0, 450, 500,
      ui->mainwindow.native_handle ()))
    return;

  set_min_size_to_current ();
  nbtk::c_widget *root = &root_widget;
  const int panel_x = 12;
  const int panel_y = 12;
  const int panel_w = 424;

  frame_main.create (root, "", panel_x, panel_y, panel_w, 420);

  image_toplogo.create (&frame_main, "", 85, 12, 256, 32);
  image_toplogo.set_png (data_textlogo_1024x128_png);

  image_logo.create (&frame_main, "", 133, 64, 160, 160);
  image_logo.set_png (data_neuralblender_logo_160_png);

  for (int i = 0; g_about_text [i]; i++) {
    label_text [i].create (
      &frame_main, g_about_text [i], 0, 240 + i * 24, panel_w, 24);
    label_text [i].fontsize = 13.0f;
  }

  /*label_link.create (
    &frame_main, "http://deimos.ca/neuralblender", 0, 336, panel_w, 24);
  label_link.fontsize = 13.0f;
  label_link.link = true;*/
  label_text [4].link = true;
  label_text [5].link = true;
  
  char buf [64];
  snprintf (buf, sizeof (buf), "Build timestamp: %s", g_build_timestamp);
  label_build.create (&frame_main, buf, 0, 390, panel_w, 20);
  label_build.fontsize = 10.0f;

  btn_ok.create (root, "OK", 310, 424, 128, 40);
  btn_ok.set_image_default (data_icon_xputty_approved_png);
}

void c_aboutwindow::show () { CP
  if (!widget)
    create (ui);
  if (!widget)
    return;

  nbtk::c_toplevelwindow::show ();
}

void c_aboutwindow::hide () { CP
  if (!widget)
    return;

  nbtk::c_toplevelwindow::hide ();
}

void c_aboutwindow::on_resize () { CP
  frame_main.move ((w () - frame_main.w) / 2, (h () - frame_main.h) / 2 - 24);
  btn_ok.move (w () - 140, h () - 50);
  c_toplevelwindow::on_resize ();
}

bool c_aboutwindow::on_key_down (int key) {
  if (key == nbtk::KEY_ESCAPE) {
    hide ();
    return true;
  }

  return nbtk::c_toplevelwindow::on_key_down (key);
}

void c_aboutwindow::on_action (nbtk::t_action_event &event) {
  if (event.source_id == btn_ok.id &&
      (event.mouse_button == 0 || event.mouse_button == Button1)) {
    hide ();
    event.handled = true;
  }
}

////////////////////////////////////////////////////////////////////////////////
// c_neuralblender_filepicker

void c_neuralblender_filepicker::create (
    c_neuralblender_ui *ui_,
    nbtk::c_app *nbtk_app,
    nbtk::t_native_window parent,
    nbtk::t_native_handle owner,
    size_t lane_,
    uint64_t bank_,
    const char *title_) {

  ui = ui_;
  nbtk::c_filepicker::create (nbtk_app, parent, owner, title_);
  lane = lane_;
  bank = bank_;
}

void c_neuralblender_filepicker::show () {
  if (current_dir.empty () && ui) {
    const _lane_bank bank_ = bank < BANK_COUNT ? (_lane_bank) bank : BANK_AMP;
    current_dir = ui->configfile.get_item (cwd_config_key_for_bank_ui (bank_));
  }
  if (current_dir.empty ())
    current_dir = CONFIG_DEFAULT_DIR;

  nbtk::c_filepicker::show ();
}

void c_neuralblender_filepicker::set_current_dir (std::string str) {
  current_dir = std::move (str);
  if (ui && !current_dir.empty ()) {
    const _lane_bank bank_ = bank < BANK_COUNT ? (_lane_bank) bank : BANK_AMP;
    ui->configfile.set_item (cwd_config_key_for_bank_ui (bank_), current_dir);
  }
  scan_current_dir ();
}

void c_neuralblender_filepicker::add_files_from_dir (
    nbtk::c_combobox *cb,
    const std::string &selected_file_) {

  std::string selected_file = selected_file_;
  if (selected_file.empty () && ui && lane < NB_NUM_MODELS) {
    const _lane_bank bank_ = bank < BANK_COUNT ? (_lane_bank) bank : BANK_AMP;
    selected_file = ui->state.banks [bank_].lanes [lane].filename;
  }

  nbtk::c_filepicker::add_files_from_dir (cb, selected_file);
}

void c_neuralblender_filepicker::on_file_select (
    const std::string &filename) {

  if (!ui || filename.empty ())
    return;

  const _lane_bank bank_ = bank < BANK_COUNT ? (_lane_bank) bank : BANK_AMP;
  if (lane >= NB_NUM_MODELS)
    return;

  current_dir = nbtk::path_dirname (filename);
  if (!current_dir.empty ())
    ui->configfile.set_item (cwd_config_key_for_bank_ui (bank_), current_dir);

  ui->state.banks [bank_].lanes [lane].filename = filename;
  ui->state.current_dir = current_dir;
  scan_current_dir ();
  ui->load_model (bank_, lane, filename.c_str ());

  nbtk::c_combobox *cb = &ui->lanes_for_bank (bank_) [lane].menu_list;
  cb->clear ();
  add_files_from_dir (cb);
  ui->on_fileselected (cb, filename.c_str ());
  hide ();
}

////////////////////////////////////////////////////////////////////////////////
// c_lane_widgets

void c_lane_widgets::on_action (nbtk::t_action_event &event) {
  if (!ui || ui->updating_from_state)
    return;

  c_neuralblender_lane_state *lane_state =
    bank_id < BANK_COUNT && lane_id < NB_NUM_MODELS ?
      &ui->state.banks [bank_id].lanes [lane_id] : NULL;

  auto handle_knob = [&] (nbtk::c_knob &knob, _widget_role role) {
    if (event.source_id != knob.id)
      return false;

    const float value = knob.value;
    const float g = db_to_gain (value);
    switch (role) {
      case ROLE_GAIN_IN:
        if (lane_state)
          lane_state->gain_in = g;
        ui->on_gain_in (&knob, g);
      break;

      case ROLE_IR_PITCH:
        if (lane_state)
          lane_state->ir_pitch_semitones = value;
        ui->on_ir_pitch (&knob, value);
      break;

      case ROLE_GAIN_OUT:
        if (lane_state)
          lane_state->gain_out = g;
        ui->on_gain_out (&knob, g);
      break;

      case ROLE_DRY_OUT:
        if (lane_state)
          lane_state->dry_out = value <= DB_SILENCE ? 0.0f : g;
        ui->on_dry_out (&knob, g);
      break;

      case ROLE_DELAY:
        if (lane_state)
          lane_state->delay_ms = value;
        ui->on_delay (&knob, value);
      break;

      default:
      break;
    }
    event.handled = true;
    return true;
  };

  if (handle_knob (knob_gain_in, ROLE_GAIN_IN) ||
      handle_knob (knob_ir_pitch, ROLE_IR_PITCH) ||
      handle_knob (knob_gain_out, ROLE_GAIN_OUT) ||
      handle_knob (knob_dry_out, ROLE_DRY_OUT) ||
      handle_knob (knob_delay, ROLE_DELAY))
    return;

  auto handle_button = [&] (nbtk::c_button &button, _widget_role role) {
    if (event.source_id != button.id)
      return false;

    event.handled = true;
    const bool value = button.value;

    switch (role) {
      case ROLE_MUTE:
        ui->on_mute (&button, value);
        if (lane_state)
          lane_state->lane_mute = value;
        ui->sync_widgets_from_state (ui->state);
      break;

      case ROLE_EXCL_USE:
        ui->on_excl_use (&button, value);
        ui->sync_widgets_from_state (ui->state);
      break;

      case ROLE_BROWSE:
        ui->on_filebrowse (&button);
        filepicker.show ();
      break;

      case ROLE_CLEAR:
        ui->on_fileclear (&button);
      break;

      case ROLE_DCFLIP:
        if (lane_state)
          lane_state->dcflip = value;
        ui->on_dcflip (&button, value);
      break;

      case ROLE_CALIBRATE:
        if (lane_state)
          lane_state->do_calib = value;
        ui->on_calibrate (&button, value);
      break;

      default:
      break;
    }

    return true;
  };

  if (handle_button (btn_mute, ROLE_MUTE) ||
      handle_button (btn_excl, ROLE_EXCL_USE) ||
      handle_button (btn_browse, ROLE_BROWSE) ||
      handle_button (btn_clear, ROLE_CLEAR) ||
      handle_button (btn_flip, ROLE_DCFLIP) ||
      handle_button (btn_calib, ROLE_CALIBRATE))
    return;

  if (event.source_id == menu_list.id) {
    event.handled = true;
    const int x = menu_list.get_selection ();
    if (x < 0 || x >= (int) menu_list.items.size ())
      return;

    std::string dir = filepicker.combo_dir.empty () ?
      filepicker.current_dir : filepicker.combo_dir;
    std::string fullpath = dir;
    if (!fullpath.empty () && fullpath.back () != '/')
      fullpath += '/';
    fullpath += menu_list.items [x];

    const _lane_bank bank = bank_id < BANK_COUNT ? (_lane_bank) bank_id : BANK_AMP;
    ui->load_model (bank, lane_id, fullpath.c_str ());
  }
}

void c_lane_widgets::create (
    c_neuralblender_ui *ui_,
    nbtk::c_widget *parent,
    nbtk::t_native_handle native_owner_,
    size_t bank_id_,
    size_t lane_id_,
    int x, int y, int w, int h) { CP
  
  move_resize (x, y, w, h);
  //knob_top = (h - knob_size) / 2;
  
  ui = ui_;
  lane_id = lane_id_;
  bank_id = bank_id_;
  native_owner = native_owner_;

  char label [64];
  const char *bank_name = "Amp";
  switch (bank_id) {
    case BANK_PEDAL: bank_name = "Pedal"; break;
    case BANK_CAB:   bank_name = "Cab/IR"; break;
    case BANK_AMP:
    default:         bank_name = "Amp"; break;
  }
  snprintf (label, 31, "%s %c", bank_name, (char) ('A' + lane_id));
  lane_root.create (parent, "", x, y, std::max (1, w), std::max (1, h));
  lane_root.bank = bank_id;
  lane_root.lane = lane_id;
  lane_frame.create (&lane_root, label, 0, 0, std::max (1, w), std::max (1, h));
  lane_frame.bank = bank_id;
  lane_frame.lane = lane_id;
  lane_frame.state = lane_state;
  created = true;
  main_widget = native_owner;
  
  knob_delay.create (&lane_frame, "Delay", 0, 0, 64, 64);
  knob_delay.role = ROLE_DELAY;
  knob_delay.bank = bank_id;
  knob_delay.lane = lane_id;
  knob_delay.set_min (0);
  knob_delay.set_max (30);
  knob_delay.set_default (0);
  knob_delay.set_value (0);
  knob_delay.set_step (0.01);

  menu_list.create (&lane_frame, "", 0, 0, 320, 32);
  menu_list.role = ROLE_LOADFILE;
  menu_list.bank = bank_id;
  menu_list.lane = lane_id;
  menu_list.wheel_selects_item = true;

  meter_out.create (&lane_root, "", 0, 0, METER_WIDTH, 120);
  meter_out.set_vudata (&vudata_out);
  meter_out.set_stereo (false);
  vudata_out.set_l (0.0, 0.0);
  
  btn_browse.create (&lane_frame, "", 0, 0, 100, 40);
  btn_browse.role = ROLE_BROWSE;
  btn_browse.bank = bank_id;
  btn_browse.lane = lane_id;
  btn_clear.create  (&lane_frame, "", 0, 0, 100, 40);
  btn_clear.role = ROLE_CLEAR;
  btn_clear.bank = bank_id;
  btn_clear.lane = lane_id;
  btn_excl.set_image (data_icon_radiobutton_on_png, nbtk::WSTATE_ON);
  btn_excl.set_image (data_icon_radiobutton_off_png, nbtk::WSTATE_OFF);
  
  btn_calib.create  (&lane_frame, "", 0, 0, 32, 32);
  btn_calib.role = ROLE_CALIBRATE;
  btn_calib.bank = bank_id;
  btn_calib.lane = lane_id;
  btn_flip.create   (&lane_frame, "", 0, 0, 32, 32);
  btn_flip.role = ROLE_DCFLIP;
  btn_flip.bank = bank_id;
  btn_flip.lane = lane_id;
  btn_flip.is_toggle = true;
  btn_calib.is_toggle = true;
  if (ui && lane_id < NB_NUM_MODELS && bank_id < BANK_COUNT)
    btn_calib.set_value (ui->state.banks [bank_id].lanes [lane_id].do_calib);
  //label_flip.create (ui, wp, "DC flip", 0, 0, 75, 32);
  //label_calib.create (ui, wp, "Calib.", 0, 0, 75, 32);
  label_frames.create (&lane_frame, "(not loaded)", 0, 0, 75, 24);
  label_frames.fontsize = 10.5f;
  label_frames.align = nbtk::TEXT_CENTER;
  label_trim.create (&lane_frame, "1.0", 0, 0, 75, 24);
  label_trim.fontsize = 10.5f;
  label_trim.align = nbtk::TEXT_CENTER;
  label_engine.create (&lane_frame, "(none)", 0, 0, 120, 24);
  label_engine.fontsize = 10.5f;
  label_engine.align = nbtk::TEXT_CENTER;
  
  btn_excl.create   (&lane_frame, "Use", 0, 0, 100, 40);
  btn_excl.role = ROLE_EXCL_USE;
  btn_excl.bank = bank_id;
  btn_excl.lane = lane_id;
  btn_mute.create   (&lane_frame, "Mute", 0, 0, 100, 40);
  btn_mute.role = ROLE_MUTE;
  btn_mute.bank = bank_id;
  btn_mute.lane = lane_id;
  btn_excl.is_toggle = true;
  btn_mute.is_toggle = true;
  switch (bank_id) { // uhh wuh duh fuh
    case BANK_PEDAL:
    break;
    case BANK_AMP:
    break;
    case BANK_CAB:
    break;
  }
  btn_mute.set_value (false);
  btn_mute.set_image (data_icon_speaker_off_big_png, nbtk::WSTATE_ON);
  btn_mute.set_image (data_icon_speaker_on_big_png, nbtk::WSTATE_OFF);

  btn_browse.set_image_default (data_icon_folder_big_png);
  btn_clear.set_image_default (data_icon_x_big_png);
  btn_calib.set_image_default (data_icon_calib_big_png);
  btn_flip.set_image_default (data_icon_phase_big_png);
  
  int knobs_right = w - 180;
  knob_gain_in.create (&lane_frame, "Input", 0, 0, 64, 64);
  knob_gain_in.role = ROLE_GAIN_IN;
  knob_gain_in.bank = bank_id;
  knob_gain_in.lane = lane_id;
  knob_ir_pitch.create (&lane_frame, "Pitch", 0, 0, 64, 64);
  knob_ir_pitch.role = ROLE_IR_PITCH;
  knob_ir_pitch.bank = bank_id;
  knob_ir_pitch.lane = lane_id;
  knob_gain_in.set_min (-40);
  knob_gain_in.set_max (40);
  knob_gain_in.set_default (0);
  knob_gain_in.set_value (0);
  knob_gain_in.set_step (0.1);

  knob_ir_pitch.set_min (-12.0);
  knob_ir_pitch.set_max (12.0);
  knob_ir_pitch.set_default (0);
  knob_ir_pitch.set_value (0);
  knob_ir_pitch.set_step (0.01);
  knob_ir_pitch.hide ();
  
  knob_dry_out.create (&lane_frame, "Dry out", 0, 0, 64, 64);
  knob_dry_out.role = ROLE_DRY_OUT;
  knob_dry_out.bank = bank_id;
  knob_dry_out.lane = lane_id;
  knob_dry_out.set_min (-120);
  knob_dry_out.set_max (12);
  knob_dry_out.set_default (-120);
  knob_dry_out.set_value (-120);
  knob_dry_out.set_step (0.1);
  
  knob_gain_out.create (&lane_frame, "Output", 0, 0, 64, 64);
  knob_gain_out.role = ROLE_GAIN_OUT;
  knob_gain_out.bank = bank_id;
  knob_gain_out.lane = lane_id;
  knob_gain_out.set_min (-40);
  knob_gain_out.set_max (40);
  knob_gain_out.set_default (0);
  knob_gain_out.set_value (0);
  knob_gain_out.set_step (0.1);
  
  if (ui && lane_id < NB_NUM_MODELS) {
    nbtk::t_native_window root =
        ui->nbtk_app.backend
          ? ui->nbtk_app.backend->root_window (ui->mainwindow.native_handle (), false)
          : 0;
    filepicker.create (
      ui, &ui->nbtk_app, root, native_owner, lane_id, bank_id,
      bank_id == BANK_CAB ? "Select IR file" : "Select model file");
    filepicker.lane = lane_id;
    filepicker.bank = bank_id;
    filepicker.clear_allowed_filters ();
    std::string wav_label = "IR / WAV";
    std::string nam_label = "NAM / JSON";
    if (bank_id == BANK_CAB) {
      filepicker.add_allowed_filter (ui_wav_filter (), wav_label);
      filepicker.add_allowed_filter (ui_model_filter (), nam_label);
    } else {
      filepicker.add_allowed_filter (ui_model_filter (), nam_label);
      filepicker.add_allowed_filter (ui_wav_filter (), wav_label);
    }
    filepicker.add_allowed_filter ("*", "All files");
  }
  
  menu_list.set_tooltip ("Currently loaded model/IR, lists others in same directory");
  knob_gain_in.set_tooltip ("Input going into this model (NAM)");
  knob_ir_pitch.set_tooltip ("Shift the pitch of this IR by 100ths of a semitone");
  knob_gain_out.set_tooltip ("Scale output from this model");
  if (bank_id == BANK_CAB)
    knob_dry_out.set_tooltip ("Pass dry signal alongside IR output");
  else
    knob_dry_out.set_tooltip ("Pass dry signal alongside model output");
  btn_browse.set_tooltip ("Load a model or IR (wav) file");
  btn_clear.set_tooltip ("Clear this model/IR");
  btn_mute.set_tooltip ("Mute this lane/channel");
  btn_excl.set_tooltip ("Use this lane/channel");
  knob_delay.set_tooltip ("Micro delay applied to this lane's output");
  btn_flip.set_tooltip ("DC flip (phase invert) this lane");
  btn_calib.set_tooltip ("Calibrate (normalize) this lane's output");  
}

void c_lane_widgets::move_resize (
    int x, int y, int w, int h) {
  
  if (!main_widget)
    return;
    
  if (x == last_x && y == last_y && w == last_w && h == last_h)
    return;
  
  last_x = x;
  last_y = y;
  last_w = w;
  last_h = h;

  const int meter_x = w + 5;
  const int host_w = std::max (w, meter_x + METER_WIDTH);
  lane_root.move_resize (x, y, host_w, h);
  lane_frame.move_resize (0, 0, w, h);

  int button_padding = 4;
  
  //const int knob_size = 64;//std::max (64, h / 2);
  int knob_size = std::max (48, w / 10);
  knob_size = std::min (knob_size, std::max (48, (h * 5) / 8));
  const int knob_top = (h - knob_size) / 2 - 16;
  const int knob_right = w - knob_size * 3 - 8;
  const int menu_x = 16 + knob_size;//delay.x () + delay.w () + 8;
  const int menu_width = std::max (64, w - menu_x - (w - knob_right) - button_padding - 10);
  menu_list.move_resize (menu_x, 12, menu_width, 32 + (h / 20));
  //int button_width = std::max (24, (menu_list.w () + button_padding) / 3 - button_padding);
  int button_left = menu_list.x;
  int button_top = menu_list.y + menu_list.h + 8;
  int button_width = std::clamp (std::min (h - 68, w / 10), 24, 96);
  
  knob_delay.move_resize (12, knob_top, knob_size, knob_size + 16);

  btn_browse.move_resize (button_left, button_top, button_width, button_width);
  btn_clear.move_resize (btn_browse.x + btn_browse.w + button_padding,
                         button_top, button_width, button_width);
  btn_calib.move_resize (btn_clear.x + btn_browse.w + button_padding,
                         button_top, button_width, button_width);
  btn_flip.move_resize (btn_calib.x + btn_browse.w + button_padding,
                         button_top, button_width, button_width);
                         
  int mute_x = btn_flip.x + btn_calib.w + button_padding;
  int mute_width = std::max (24, menu_list.x + menu_list.w - mute_x);
  btn_mute.move_resize (mute_x,
                         button_top, mute_width, button_width);
  if (btn_mute.w > 80) {
    btn_mute.label = "Mute";
    btn_mute.invalidate ();
    btn_excl.label = "Use";
    btn_excl.invalidate ();
  } else {
    btn_mute.label.clear ();
    btn_mute.invalidate ();
    btn_excl.label.clear ();
    btn_excl.invalidate ();
  }
  btn_excl.move_resize (btn_mute.x, btn_mute.y, btn_mute.w, btn_mute.h);
  btn_mute.padding = btn_mute.h / 4;
  
  int btnpadding = button_width / 5;
  btn_browse.padding = btnpadding;
  btn_clear.padding = btnpadding;
  btn_flip.padding = btnpadding;
  btn_calib.padding = btnpadding;
  
  knob_gain_in.move_resize (knob_right, knob_top, knob_size, knob_size + 16);
  knob_ir_pitch.move_resize (knob_gain_in.x, knob_gain_in.y,
                             knob_gain_in.w, knob_gain_in.h);
  knob_gain_out.move_resize (knob_right + (knob_size + 1) * 2, knob_top, knob_size, knob_size + 16);
  knob_dry_out.move_resize (knob_right + knob_size + 1, knob_top, knob_size, knob_size + 16);
  
  meter_out.move_resize (meter_x, 4, METER_WIDTH, h - 8);
  
  int adv_btn_x = 84;
  int adv_btn_y = h * 2 / 11;
  label_frames.move_resize (knob_delay.x - 10, h - 20, knob_delay.w + 20, 16);
  label_engine.move_resize (knob_gain_in.x, h - 20, knob_gain_in.w, 16);
  label_trim.move_resize (knob_dry_out.x, h - 20, knob_dry_out.w +
                          knob_dry_out.w, 16);
                          
  //move_resize (x, y, w, h);
}

void c_lane_widgets::set_state (nbtk::_widget_state state) {
  if (lane_state == state)
    return;

  lane_state = state;
  lane_frame.state = lane_state;
  lane_frame.invalidate ();
}

////////////////////////////////////////////////////////////////////////////////
// c_eqband_widgets, c_eqpage_widgets

extern float g_defaultfreqs [];
extern _eq_band_mode g_defaultmodes [];

void c_eqpage_widgets::create (
    c_neuralblender_ui       *ui_,
    nbtk::c_widget           *parent,
    nbtk::t_native_handle    native_owner,
    size_t                   bank_id,
    //size_t                   lane_id,
    int x, int y, int w, int h) { CP
  
  ui = ui_;
  std::string str;
  if (bank_id == BANK_EQPRE)
    str = "Pre-EQ";
  else if (bank_id == BANK_EQPOST)
    str = "Post-EQ";
  else
    str = "EQ";
  
  frame.create (parent, "", x, y, w, h);
  cont_bands.create (&frame, "", 0, 0, w, h);
  label.create (&frame, str.c_str (), 0, 0, 300, 30);
  label.align = nbtk::TEXT_LEFT;
  knob_gain.create (&frame, "", 0, 0, 40, 56);
  knob_gain.text_size = 0.75;
  knob_gain.label_position = nbtk::LABEL_LEFT;
  knob_gain.label_align = nbtk::TEXT_LEFT;
  knob_gain.role = ROLE_EQ_MASTER_GAIN;
  knob_gain.bank = bank_id;
  knob_gain.set_min (-36.0f);
  knob_gain.set_max (36.0f);
  knob_gain.set_default (0.0f);
  knob_gain.set_value (0.0f);
  knob_gain.set_step (0.1f);
  
  for (int i = 0; i < EQ_NUM_BANDS; i++) {
    bands [i].slider_gain.create (&cont_bands, "", 0, 0, 12, 200);
    bands [i].slider_gain.set_orientation (nbtk::SCROLLBAR_VERTICAL);
    bands [i].slider_gain.set_range (-36.0f, 36.0f);
    bands [i].slider_gain.set_step (0.1f);
    bands [i].slider_gain.set_default (0.0f);
    bands [i].slider_gain.set_value (0.0f);
    bands [i].slider_gain.set_page_size (6.0f / 72.0f);
    bands [i].slider_gain.label_position = nbtk::LABEL_BELOW;
    bands [i].slider_gain.label_fontsize = 11.0f;
    bands [i].slider_gain.label_gap = 4;
    bands [i].slider_gain.track_size = 16;
    bands [i].slider_gain.role = ROLE_EQ_GAIN;
    bands [i].slider_gain.bank = bank_id;
    bands [i].slider_gain.lane = i;
    
    bands [i].btn_on.create (&cont_bands, "", 0, 250, 32, 32);
    bands [i].btn_on.is_toggle = true;
    bands [i].btn_on.role = ROLE_EQ_ENABLED;
    bands [i].btn_on.bank = bank_id;
    bands [i].btn_on.lane = i;
    
    bands [i].menu_mode.create (&cont_bands, "", 0, 280, 92, 32);
    //bands [i].menu_mode.fontsize = 8.0f;
    bands [i].menu_mode.set_items ({
      "HP", "Low", "Bell", "High", "LP"
    });
    bands [i].menu_mode.role = ROLE_EQ_MODE;
    bands [i].menu_mode.bank = bank_id;
    bands [i].menu_mode.lane = i;
    bands [i].menu_mode.set_selected ((int) g_defaultmodes [i] - 1);
    bands [i].menu_mode.text_size = 0.75;

    bands [i].knob_freq.create (&cont_bands, "Freq", 0, 300, 36, 50);
    bands [i].knob_freq.role = ROLE_EQ_FREQ;
    bands [i].knob_freq.bank = bank_id;
    bands [i].knob_freq.lane = i;
    bands [i].knob_freq.set_min (20.0f);
    bands [i].knob_freq.set_max (20000.0f);
    bands [i].knob_freq.set_step (1.0f);
    bands [i].knob_freq.log_taper = 4.5f;
    bands [i].knob_freq.set_default (g_defaultfreqs [i]);
    bands [i].knob_freq.set_value (g_defaultfreqs [i]);
    bands [i].knob_freq.text_size = 0.75;
    bands [i].knob_q.create (&cont_bands, "Q", 0, 380, 36, 50);
    bands [i].knob_q.role = ROLE_EQ_Q;
    bands [i].knob_q.bank = bank_id;
    bands [i].knob_q.lane = i;
    bands [i].knob_q.set_min (0.01f);
    bands [i].knob_q.set_max (100.0f);
    bands [i].knob_q.set_default (1.0f);
    bands [i].knob_q.set_value (1.0f);
    bands [i].knob_q.log_taper = 3.0f;
    bands [i].knob_q.text_size = 0.75;
  }
}

void c_eqpage_widgets::move_resize (int x, int y, int w, int h) {
  const int frame_w = std::max (1, w);
  const int frame_h = std::max (1, h);
  frame.move_resize (x, y, frame_w, frame_h);
  const int padding = 16;
  const int title_h = 48;
  const int inner_x = padding;
  const int inner_y = padding + title_h + 8;
  const int inner_w = std::max (1, frame_w - padding * 2);
  const int inner_h = std::max (1, frame_h - inner_y - padding);
  const int band_w = std::max (1, inner_w / EQ_NUM_BANDS);
  const int slider_w = 16;
  const int slider_widget_w = std::max (slider_w, band_w - 4);
  const int label_h = 18;
  const int checkbox = 28;
  const int menu_h = 28;
  const int knob_w = std::min (64, std::max (42, band_w - 8));
  const int menu_w = std::min (80, std::max (44, band_w - 8));
  const int knob_h = 68;
  const int controls_h = label_h + checkbox + menu_h + knob_h * 2 + 12;
  const int slider_top = inner_y;
  const int slider_track_h = std::max (40, inner_h - controls_h - 8);
  const int slider_h = slider_track_h + label_h + 4;
  const int checkbox_y = slider_top + slider_h + 6;
  const int menu_y = checkbox_y + checkbox + 6;
  const int freq_y = menu_y + menu_h + 6;
  const int q_y = freq_y + knob_h;
  
  label.move_resize (padding, padding, std::max (1, frame_w / 2), title_h);
  knob_gain.move_resize (
    std::max (padding + 80, frame_w - padding - 138),
    padding,
    130,
    title_h);
  cont_bands.move_resize (0, 0, frame_w, frame_h);
  
  for (int i = 0; i < EQ_NUM_BANDS; i++) {
    const int band_x = inner_x + i * band_w;
    const int center_x = band_x + band_w / 2;

    bands [i].slider_gain.move_resize (
      center_x - slider_widget_w / 2, slider_top, slider_widget_w, slider_h);
    bands [i].btn_on.move_resize (
      center_x - checkbox / 2, checkbox_y, checkbox, checkbox);
    bands [i].menu_mode.move_resize (
      center_x - menu_w / 2, menu_y, menu_w, menu_h);
    bands [i].knob_freq.move_resize (
      center_x - knob_w / 2, freq_y, knob_w, knob_h);
    bands [i].knob_q.move_resize (
      center_x - knob_w / 2, q_y, knob_w, knob_h);
  }
}

void c_eqpage_widgets::sync_from_state (const c_eq_state &eq_state) {
  knob_gain.set_value (eq_state.master_gain_db);

  for (int i = 0; i < EQ_NUM_BANDS; ++i) {
    bands [i].btn_on.set_value (eq_state.enabled [i]);
    bands [i].menu_mode.set_selected (std::clamp (
      (int) eq_state.mode [i] - 1,
      0,
      (int) EQ_LOWPASS - 1));
    bands [i].slider_gain.set_value (eq_state.gain_db [i]);
    bands [i].knob_freq.set_value (eq_state.freq [i]);
    bands [i].knob_q.set_value (eq_state.q [i]);
  }
}

////////////////////////////////////////////////////////////////////////////////
// c_neuralblender_ui

c_neuralblender_ui::c_neuralblender_ui () { CP
  memset (&app, 0, sizeof (app));
  for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
    for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
      const size_t n = i * UI_STATS_PER_LANE;
      stats [bank] [n] = 0.0f;
      stats [bank] [n + 1] = 1.0f;
      stats [bank] [n + 2] = (float) ENGINE_NONE;
    }
  }
  display = NULL;
  window = 0;
  ui_ready = false;
}

c_neuralblender_ui::~c_neuralblender_ui () { CP
  destroy ();
}

void c_neuralblender_ui::update_model_cwd (std::string path) {
  CP
  debug ("path='%s'", path.c_str ());
  configfile.set_item (CONFIG_KEY_NAME_MODEL_CWD, nbtk::path_dirname (path));
}

void c_neuralblender_ui::update_ir_cwd (std::string path) {
  CP
  debug ("path='%s'", path.c_str ());
  configfile.set_item (CONFIG_KEY_NAME_IR_CWD, nbtk::path_dirname (path));
}

bool c_neuralblender_ui::create (nbtk::t_native_window parent_) { CP
  size_t i;
  destroy ();

  if (!nbtk_app.backend)
    nbtk_app.backend = nbtk::create_native_backend ();
  if (!nbtk_app.backend)
    return false;

  nbtk_app.backend->init_app (&app);
  nbtk_app.native_app = &app;
  /*app.small_font = 12 * app.hdpi;
  app.normal_font = 14 * app.hdpi;
  app.big_font = 20 * app.hdpi;*/
  display = nbtk_app.backend->display (&app);
  nbtk_app.display = display;
  
  configfile.read_file ();
  read_prefs_from_config (configfile, prefs);
  
  if (configfile.istrue (CONFIG_KEY_NAME_ADV)) {
    CP
    state.showadvanced = true;
  }

  if (prefs.calib_default) {
    calib_default = true;
    for (_lane_bank bank_id : MODEL_BANKS) {
      const size_t bank = (size_t) bank_id;
      for (i = 0; i < NB_NUM_MODELS; ++i)
        state.banks [bank].lanes [i].do_calib = true;
    }
  }

  parent = parent_;
  if (!parent)
    parent = nbtk_app.backend->default_root_window (display);

  int initial_w = DEFAULT_WINDOW_WIDTH;
  int initial_h = DEFAULT_WINDOW_HEIGHT;
  if (parent_) {
    int parent_w = 0;
    int parent_h = 0;
    if (nbtk_app.backend->window_size (
          display, parent, app.hdpi, &parent_w, &parent_h) &&
        parent_w >= MIN_WINDOW_WIDTH &&
        parent_h >= MIN_WINDOW_HEIGHT &&
        parent_w <= 3000 &&
        parent_h <= 2200) {
      initial_w = parent_w;
      initial_h = parent_h;
      debug ("initial parent window size %d,%d", initial_w, initial_h);
    }
  }
  
  if (!mainwindow.create (this, parent, "NeuralBlender", 0, 0, 
                          initial_w, initial_h)) {
    fprintf (stderr, "Cant' create main window!\n");
    return false;
  }
  
  mainwindow.set_icon_from_png (data_neuralblender_logo_512_png);

  cont_toparea.create (&mainwindow.root_widget, "", 0, 0, 640, 50);
  cont_pedals.create (&mainwindow.root_widget, "", 0, 120, 640, 480);
  cont_eqpre.create (&mainwindow.root_widget, "", 0, 120, 640, 480);
  cont_models.create (&mainwindow.root_widget, "", 0, 120, 640, 480);
  cont_eqpost.create (&mainwindow.root_widget, "", 0, 120, 640, 480);
  cont_cabs.create (&mainwindow.root_widget, "", 0, 120, 640, 480);
  cont_other.create (&mainwindow.root_widget, "", 0, 120, 640, 480);
  
  eqpage_pre.create (this, &cont_eqpre, mainwindow.native_handle (),
                     BANK_EQPRE, 0, 0, 640, 600);
  eqpage_post.create (this, &cont_eqpost, mainwindow.native_handle (),
                      BANK_EQPOST, 0, 0, 640, 600);
  
  img_logo.create (&cont_toparea, "", 0, 0, 256, 32);
  img_logo.set_image_default (data_textlogo_1024x128_png);
  img_logo.set_image_hover (data_textlogo_highlight_1024x128_png);
  img_logo.set_tooltip ("Click for tuner");
  img_logo.role = ROLE_TUNER;
  
  const int tabbutton_padding = 16;
  btn_tab_pedals.create (&cont_toparea, "PDL", 0, 0, 84, 50);
  btn_tab_pedals.role = ROLE_PAGESWITCH;
  btn_tab_pedals.bank = BANK_PEDAL;
  btn_tab_pedals.page = PAGE_PEDAL;
  btn_tab_pedals.set_image_default (data_icon_power_on_png);
  btn_tab_pedals.padding = tabbutton_padding;
  btn_tab_eqpre.create (&cont_toparea, "EQ1", 0, 0, 84, 50);
  btn_tab_eqpre.role = ROLE_PAGESWITCH;
  btn_tab_eqpre.bank = BANK_EQPRE;
  btn_tab_eqpre.page = PAGE_EQPRE;
  btn_tab_eqpre.set_image_default (data_icon_power_on_png);
  btn_tab_eqpre.padding = tabbutton_padding;
  btn_tab_models.create (&cont_toparea, "AMP", 0, 0, 84, 50);
  btn_tab_models.role = ROLE_PAGESWITCH;
  btn_tab_models.bank = BANK_AMP;
  btn_tab_models.page = PAGE_AMP;
  btn_tab_models.set_image_default (data_icon_power_on_png);
  btn_tab_models.padding = tabbutton_padding;
  btn_tab_eqpost.create (&cont_toparea, "EQ2", 0, 0, 84, 50);
  btn_tab_eqpost.role = ROLE_PAGESWITCH;
  btn_tab_eqpost.bank = BANK_EQPOST;
  btn_tab_eqpost.page = PAGE_EQPOST;
  btn_tab_eqpost.set_image_default (data_icon_power_on_png);
  btn_tab_eqpost.padding = tabbutton_padding;


  btn_tab_cabs.create (&cont_toparea, "CAB", 0, 0, 84, 50);
  btn_tab_cabs.role = ROLE_PAGESWITCH;
  btn_tab_cabs.bank = BANK_CAB;
  btn_tab_cabs.page = PAGE_CAB;
  btn_tab_cabs.set_image_default (data_icon_power_on_png);
  btn_tab_cabs.padding = tabbutton_padding;
  btn_tab_other.create (&cont_toparea, "...", 0, 0, 84, 50);
  btn_tab_other.role = ROLE_PAGESWITCH;
  btn_tab_other.bank = BANK_AMP;
  btn_tab_other.page = PAGE_OTHER;
  
  //btn_noisegate.create (&cont_toparea, "", 0, 0, 40, 40);
  //btn_noisegate.role = ROLE_NOISEGATE;
  //btn_noisegate.is_toggle = true;
  //btn_noisegate.set_image_default (data_icon_noisegate_png);
  //
  //btn_tuner.create (&cont_toparea, "", 0, 0, 40, 40);
  //btn_tuner.role = ROLE_TUNER;
  //btn_tuner.is_toggle = true;
  //btn_tuner.set_image_default (data_icon_tuner_png);
  
  btn_muteall.create (&cont_toparea, "", 500, 12, 40, 40);
  btn_muteall.role = ROLE_MUTEALL;
  btn_muteall.is_toggle = true;
  btn_muteall.set_image (data_icon_speaker_off_big_png, nbtk::WSTATE_ON);
  btn_muteall.set_image (data_icon_speaker_on_big_png, nbtk::WSTATE_OFF);
  
  btn_enable.create (&cont_toparea, "",  20, 12, 40, 40);
  btn_enable.role = ROLE_BYPASS;
  btn_enable.is_toggle = true;
  btn_enable.set_value (true);
  btn_enable.set_image (data_icon_power_on_png, nbtk::WSTATE_ON);
  btn_enable.set_image (data_icon_power_grey_png, nbtk::WSTATE_OFF);
  
  btn_enable.padding =    12;
  btn_muteall.padding =   12;
  //btn_tuner.padding =     12;
  btn_noisegate.padding = 12;
  
  aboutwindow.create (this);
  prefswindow.create (this);
  mainwindow.activate ();
  
  for (i = 0; i < NB_NUM_MODELS; i++) {
    lanes_pedals [i].create (
    this, &cont_pedals, mainwindow.native_handle (), BANK_PEDAL, i, 0, 0, 1, 1);
    lanes_models [i].create (
    this, &cont_models, mainwindow.native_handle (), BANK_AMP, i, 0, 0, 1, 1);
    lanes_cabs [i].create (
    this, &cont_cabs, mainwindow.native_handle (), BANK_CAB, i, 0, 0, 1, 1);
  }
  meter_in [PAGE_PEDAL].create (&cont_pedals, "", 6, 70, METER_WIDTH, 520);
  meter_in [PAGE_EQPRE].create (&cont_eqpre, "", 6, 70, METER_WIDTH, 520);
  meter_in [PAGE_AMP].create (&cont_models, "", 6, 70, METER_WIDTH, 520);
  meter_in [PAGE_EQPOST].create (&cont_eqpost, "", 6, 70, METER_WIDTH, 520);
  meter_in [PAGE_CAB].create (&cont_cabs, "", 6, 70, METER_WIDTH, 520);
  meter_eqout [0].create (&cont_eqpre, "", 6, 70, METER_WIDTH, 520);
  meter_eqout [1].create (&cont_eqpost, "", 6, 70, METER_WIDTH, 520);
  meter_in [PAGE_OTHER].create (&cont_other, "", 6, 70, METER_WIDTH, 520);
  meter_masterout.create (&cont_other, "", 6, 70, METER_WIDTH, 520);

  for (i = 0; i < BANK_COUNT; i++) {
    meter_in [i].set_vudata (&vudata_in [i]);
    meter_in [i].set_stereo (false);
    vudata_in [i].set_l (0.0, 0.0);
  }
  meter_in [PAGE_OTHER].set_vudata (&vudata_masterin);
  meter_in [PAGE_OTHER].set_stereo (false);
  vudata_masterin.set_l (0.0, 0.0);
  meter_eqout [0].set_vudata (&vudata_in [BANK_AMP]);
  meter_eqout [0].set_stereo (false);
  meter_eqout [1].set_vudata (&vudata_in [BANK_CAB]);
  meter_eqout [1].set_stereo (false);
  meter_masterout.set_vudata (&vudata_masterout);
  meter_masterout.set_stereo (false);
  vudata_masterout.set_l (0.0, 0.0);
  
  frame_other_volumepresence.create (&cont_other, "", 16, 16, 512, 128);
  knob_mastervolume.create (&frame_other_volumepresence, "Master out", 16, 12, 80, 96);
  knob_mastervolume.role = ROLE_MASTER;
  knob_presence.create (&frame_other_volumepresence, "Presence", 116, 12, 80, 96);
  knob_presence.role = ROLE_PRESENCE;
  knob_mastervolume.set_min (-40);
  knob_mastervolume.set_max (12);
  knob_mastervolume.set_value (gain_to_db (state.master_gain));
  knob_mastervolume.set_default (0);
  knob_mastervolume.set_step (0.1);
  knob_presence.set_min (0);
  knob_presence.set_max (1);
  knob_presence.set_value (state.presence);
  knob_presence.set_default (0);
  knob_presence.set_step (0.01);
  
  frame_other_noisegate.create (&cont_other, "", 24, 16, 512, 128);
  label_other_noisegate.create (&frame_other_noisegate, "Noise gate:", 16, 8, 200, 24);
  label_other_noisegate.align = nbtk::TEXT_LEFT;
  btn_other_noisegate.create (&frame_other_noisegate, "", 24, 44, 48, 48);
  btn_other_noisegate.role = ROLE_NOISEGATE;
  btn_other_noisegate.is_toggle = true;
  btn_other_noisegate.set_image_default (data_icon_noisegate_png);

  knob_noisethresh.create (&frame_other_noisegate,  "Thresh",   90, 36, 64, 72);
  knob_noisethresh.role = ROLE_NOISETHRESH;
  knob_noiseattack.create (&frame_other_noisegate,  "Attack",  150, 36, 64, 72);
  knob_noiseattack.role = ROLE_NOISEATTACK;
  knob_noisehold.create (&frame_other_noisegate,    "Hold",    210, 36, 64, 72);
  knob_noisehold.role = ROLE_NOISEHOLD;
  knob_noiserelease.create (&frame_other_noisegate, "Release", 270, 36, 64, 72);
  knob_noiserelease.role = ROLE_NOISERELEASE;
  knob_noisethresh.set_min (-120);
  knob_noisethresh.set_max (-6);
  knob_noisethresh.set_value (state.noisethresh);
  knob_noisethresh.set_default (-60);
  knob_noisethresh.set_step (0.1);

  knob_noiseattack.set_min (0);
  knob_noiseattack.set_max (200);
  knob_noiseattack.set_value (state.noiseattack);
  knob_noiseattack.set_default (2);
  knob_noiseattack.set_step (0.1);

  knob_noisehold.set_min (0);
  knob_noisehold.set_max (200);
  knob_noisehold.set_value (state.noisehold);
  knob_noisehold.set_default (10);
  knob_noisehold.set_step (0.1);

  knob_noiserelease.set_min (0);
  knob_noiserelease.set_max (500);
  knob_noiserelease.set_value (state.noiserelease);
  knob_noiserelease.set_default (20);
  knob_noiserelease.set_step (0.1);

  frame_other_linkexcl.create (&cont_other, "", 16, 16, 512, 128);
  const int x0 = 16, x1 = 156, x2 = 246, x3 = 336, x4 = 426, x5 = 516;
  const int y0 = 16, y1 = 56, y2 = 96, chkw = 80;
  label_other_byp.create (&frame_other_linkexcl, "Bypass: ",  x0, y0, 150, 32);
  label_other_link.create (&frame_other_linkexcl, "Link calibration: ", x0, y1, 150, 32);
  label_other_excl.create (&frame_other_linkexcl, "Exclusive mode: ",   x0, y2, 150, 32);
  label_other_byp.align = nbtk::TEXT_LEFT;
  label_other_link.align = nbtk::TEXT_LEFT;
  label_other_excl.align = nbtk::TEXT_LEFT;
  btn_other_byp_pedal.create (&frame_other_linkexcl, "Pedal",  x1, y0, 80, 32);
  btn_other_byp_pedal.role = ROLE_BANK_BYPASS;
  btn_other_byp_pedal.bank = BANK_PEDAL;
  btn_other_byp_eq1.create (&frame_other_linkexcl, "EQ1",  x2, y0, 80, 32);
  btn_other_byp_eq1.role = ROLE_BANK_BYPASS;
  btn_other_byp_eq1.bank = BANK_EQPRE;
  btn_other_byp_amp.create (&frame_other_linkexcl, "Amp",      x3, y0, 80, 32);
  btn_other_byp_amp.role = ROLE_BANK_BYPASS;
  btn_other_byp_amp.bank = BANK_AMP;
  btn_other_byp_eq2.create (&frame_other_linkexcl, "EQ2",  x4, y0, 80, 32);
  btn_other_byp_eq2.role = ROLE_BANK_BYPASS;
  btn_other_byp_eq2.bank = BANK_EQPOST;
  btn_other_byp_cab.create (&frame_other_linkexcl, "Cab",   x5, y0, 80, 32);
  btn_other_byp_cab.role = ROLE_BANK_BYPASS;
  btn_other_byp_cab.bank = BANK_CAB;
  btn_other_link_pedal.create (&frame_other_linkexcl, "Pedal", x1, y1, 80, 32);
  btn_other_link_pedal.role = ROLE_LINKED_CALIB;
  btn_other_link_pedal.bank = BANK_PEDAL;
  btn_other_link_amp.create (&frame_other_linkexcl, "Amp",     x2, y1, 80, 32);
  btn_other_link_amp.role = ROLE_LINKED_CALIB;
  btn_other_link_amp.bank = BANK_AMP;
  btn_other_link_cab.create (&frame_other_linkexcl, "Cab",  x3, y1, 80, 32);
  btn_other_link_cab.role = ROLE_LINKED_CALIB;
  btn_other_link_cab.bank = BANK_CAB;
  btn_other_excl_pedal.create (&frame_other_linkexcl, "Pedal", x1, y2, 80, 32);
  btn_other_excl_pedal.role = ROLE_EXCL_TOGGLE;
  btn_other_excl_pedal.bank = BANK_PEDAL;
  btn_other_excl_amp.create (&frame_other_linkexcl, "Amp",     x2, y2, 80, 32);
  btn_other_excl_amp.role = ROLE_EXCL_TOGGLE;
  btn_other_excl_amp.bank = BANK_AMP;
  btn_other_excl_cab.create (&frame_other_linkexcl, "Cab",  x3, y2, 80, 32);
  btn_other_excl_cab.role = ROLE_EXCL_TOGGLE;
  btn_other_excl_cab.bank = BANK_CAB;
  
  frame_other_misc.create (&cont_other, "", 16, 16, 512, 128);
  text_other_tuner.create (&frame_other_misc, "", 250, 14, 140, 40);
  text_other_tuner.role = ROLE_TUNER_BASE_FREQ;
  btn_tuner.create (&frame_other_misc, "", 410, 14, 40, 40);
  btn_tuner.role = ROLE_TUNER;
  btn_tuner.is_toggle = true;
  btn_tuner.set_image_default (data_icon_tuner_png);
  btn_other_tuner_down.create (&frame_other_misc, "", 458, 14, 40, 40);
  btn_other_tuner_down.role = ROLE_TUNER_DOWN;
  btn_other_tuner_down.set_image_default (data_icon_flat_png);
  btn_other_tuner_up.create (&frame_other_misc, "", 506, 14, 40, 40);
  btn_other_tuner_up.role = ROLE_TUNER_UP;
  btn_other_tuner_up.set_image_default (data_icon_sharp_png);
  btn_other_tuner_default.create (&frame_other_misc, "", 554, 14, 40, 40);
  btn_other_tuner_default.role = ROLE_TUNER_DEFAULT;
  btn_other_tuner_default.set_image_default (data_icon_x_big_png);
  text_other_calib.create (&frame_other_misc, "", 250, 60, 140, 40);
  text_other_calib.role = ROLE_CALIB_TARGET_DB;
  label_other_tuner.create (&frame_other_misc, "Tuner base frequency: ", 16, 20, 200, 32);
  label_other_calib.create (&frame_other_misc, "Calibration target dB: ", 16, 60, 200, 32);
  label_other_tuner.align = nbtk::TEXT_LEFT;
  label_other_calib.align = nbtk::TEXT_LEFT;
  btn_other_vu.create (&frame_other_misc, "VU meters", 16, 100, 200, 32);
  btn_other_vu.role = ROLE_VUTOGGLE;
  btn_other_bass.create (&frame_other_misc, "Calibrate for bass", 16, 140, 200, 32);
  btn_other_bass.role = ROLE_CALIBBASS;
  
  btn_other_prefs.create (&frame_other_misc, "Settings", 0, 130, 120, 40);
  btn_other_prefs.role = ROLE_PREFS;
  btn_other_prefs.set_image_default (data_icon_xputty_gear_png);
  btn_other_about.create (&frame_other_misc, "About...", 0, 130, 120, 40);
  btn_other_about.role = ROLE_ABOUT;
  btn_other_about.set_image_default (data_icon_xputty_info_png);
  
  //tuner.create (this, mainwindow.widget, "", 0, 0, 400, 24);
  if (blender)
    tuner.set_pitchtracker (&blender->pitchtracker);
  tuner.hide ();

  if (blender) {
    for (i = 0; i < NB_NUM_MODELS; i++) {
      blender->banks [BANK_PEDAL].meters_out [i] = &lanes_pedals [i].vudata_out;
      blender->banks [BANK_AMP].meters_out [i]   = &lanes_models [i].vudata_out;
      blender->banks [BANK_CAB].meters_out [i]   = &lanes_cabs [i].vudata_out;
    }
    blender->banks [BANK_PEDAL].meter_in = &vudata_in [BANK_PEDAL];
    blender->banks [BANK_EQPRE].meter_in = &vudata_in [BANK_EQPRE];
    blender->banks [BANK_AMP].meter_in   = &vudata_in [BANK_AMP];
    blender->banks [BANK_EQPOST].meter_in = &vudata_in [BANK_EQPOST];
    blender->banks [BANK_CAB].meter_in   = &vudata_in [BANK_CAB];
    blender->meter_masterin              = &vudata_masterin;
    blender->meter_masterout             = &vudata_masterout;
  }

  apply_ui_prefs (prefs);
  
  //if (state.showadvanced) {
  //  show_advanced_settings ();
  //} else {
  //  hide_advanced_settings ();
  //}
  btn_tab_pedals.set_tooltip ("Pedals bank");
  btn_tab_models.set_tooltip ("Amp model bank");
  btn_tab_eqpre.set_tooltip ("Pre EQ");
  btn_tab_eqpost.set_tooltip ("Post EQ");
  btn_tab_cabs.set_tooltip ("Cab/IR bank");
  btn_tab_other.set_tooltip ("More settings, right-click to toggle current bank's exclusive mode");
  btn_enable.set_tooltip ("Master BYPASS");
  btn_muteall.set_tooltip ("Master MUTE");
  btn_tuner.set_tooltip ("Enable tuner");
  btn_other_noisegate.set_tooltip ("Enable noisegate");
  btn_other_noisegate.set_tooltip ("Enable noisegate");
  knob_mastervolume.set_tooltip ("Master output volume from plugin");
  knob_presence.set_tooltip ("Presence: applies a builtin high-frequency IR to output");
  knob_noisethresh.set_tooltip ("Noisegate threshold");
  knob_noiseattack.set_tooltip ("Noisegate attack");
  knob_noisehold.set_tooltip ("Noisegate hold");
  knob_noiserelease.set_tooltip ("Noisegate release");
  text_other_tuner.set_tooltip ("Base frequency used by tuner for middle A");
  text_other_calib.set_tooltip ("Aim calibration to reach this target output on typical input");
  btn_other_vu.set_tooltip ("Enable/disable display of VU meters");
  btn_other_prefs.set_tooltip ("Non-host preferences");
  btn_other_about.set_tooltip ("About NeuralBlender");
  btn_other_tuner_down.set_tooltip ("Transpose tuner down a semitone");
  btn_other_tuner_up.set_tooltip ("Transpose tuner up a semitone");
  btn_other_tuner_default.set_tooltip ("Reset tuner to A=440");
  btn_other_bass.set_tooltip ("Calibrate models/IR output levels for bass guitar");
  
  ui_ready = true;
  move_resize ();
  mainwindow.show ();
  sync_tuner_visibility ();
  CP
  //do_set_min_size = true;
  return true;
}

void c_neuralblender_ui::move_resize (bool snap_to_default) {
  CP
  //state.showadvanced = b;
  
  if (!ui_resize_lock && mainwindow.is_created ()) {
    int metrics_w = 0;
    int metrics_h = 0;
    bool metrics_visible = false;
    mainwindow.get_metrics (&metrics_w, &metrics_h, &metrics_visible);
    ui_resize_lock = true;
    
    int window_width = std::max (MIN_WINDOW_WIDTH, mainwindow.w ());
    int window_height = std::max (MIN_WINDOW_HEIGHT, mainwindow.h ());

    debug ("move_resize: snap=%d visible=%d main=%d,%d metrics=%d,%d pending=%d,%d",
           (int) snap_to_default, (int) metrics_visible,
           mainwindow.w (), mainwindow.h (),
           metrics_w, metrics_h,
           pending_resize_w, pending_resize_h);
    if (!snap_to_default && metrics_visible) {
      window_width = std::max (MIN_WINDOW_WIDTH, metrics_w);
      window_height = std::max (MIN_WINDOW_HEIGHT, metrics_h);
    }
    
    //if (do_set_min_size)
    mainwindow.set_min_size (MIN_WINDOW_WIDTH, MIN_WINDOW_HEIGHT);
    
    int toparea = std::max (window_height / 5, 116);
    if (toparea < 32) toparea = 32;
    cont_toparea.move_resize (0, 0, window_width, toparea);
    const int page_y = toparea + 8;
    const int bottom_inset = 8;
    const int page_h = std::max (1, window_height - page_y - bottom_inset);
    cont_pedals.move_resize (0, page_y, window_width, page_h);
    cont_eqpre.move_resize  (0, page_y, window_width, page_h);
    cont_models.move_resize (0, page_y, window_width, page_h);
    cont_eqpost.move_resize (0, page_y, window_width, page_h);
    cont_cabs.move_resize   (0, page_y, window_width, page_h);
    cont_other.move_resize  (0, page_y, window_width, page_h);
    
    sync_page_visibility ();
    
    int lane_width = window_width - 32;
    const int lane_top = 0;
    const int lane_gap = 12;
    const int lane_count = NB_NUM_MODELS;
    const int total_gap = (lane_count > 1) ? (lane_count - 1) * lane_gap : 0;
    //const int lane_area = window_height - lane_top - bottom_reserve - total_gap;
    const int lane_area = page_h - total_gap;
    int lane_height = std::max (1, lane_area / lane_count);
    
    debug ("window w/h %d,%d", window_width, window_height);
    
    const int btnl = 16;
          int btnh = std::max (50, toparea / 2 - 12);
          int btnw = std::min (76, btnh * 2);
              btnw = std::max (btnw, window_width / 8);
    if (btnh > window_width / 12)
      btnh = window_width / 12;
    const int btnr = window_width - 12;
    const int btnt = toparea - btnh; //toparea / 2 + 8;
    
    btn_tab_pedals.move_resize  (btnl,                  btnt, btnw, btnh);
    btn_tab_eqpre.move_resize   (btnl + (btnw + 4) * 1, btnt, btnw, btnh);
    btn_tab_models.move_resize  (btnl + (btnw + 4) * 2, btnt, btnw, btnh);
    btn_tab_eqpost.move_resize  (btnl + (btnw + 4) * 3, btnt, btnw, btnh);
    btn_tab_cabs.move_resize    (btnl + (btnw + 4) * 4, btnt, btnw, btnh);
    btn_tab_other.move_resize   (btnl + (btnw + 4) * 5, btnt, btnw, btnh);
    //btn_noisegate.move_resize   (btnr - (btnh + 4) * 4, btnt, btnh, btnh);
    //btn_tuner.move_resize       (btnr - (btnh + 4) * 3, btnt, btnh, btnh);
    btn_muteall.move_resize     (btnr - (btnh + 4) * 2, btnt, btnh, btnh);
    btn_enable.move_resize      (btnr - (btnh + 4)    , btnt, btnh, btnh);
    
    int btnpad = btn_tab_pedals.h / 4;
    btn_tab_pedals.padding = btnpad;
    btn_tab_models.padding = btnpad;
    btn_tab_cabs.padding = btnpad;
    btn_tab_eqpre.padding = btnpad;
    btn_tab_eqpost.padding = btnpad;
    
    tuner_height = toparea - btnh - 8;
    img_logo.move_resize (window_width / 2 - 128, tuner_height / 2- 16, 256, 32);
    cont_toparea.invalidate ();
    if (tuner.created)
      tuner.move_resize (4, 4, window_width - 8, tuner_height);
    
    const int panelwidth = window_width - 32;
    auto lane_meter_span = [] (c_lane_widgets *lanes, int count,
                               int *top, int *height) {
      int meter_top = 0;
      int meter_bottom = 0;
      bool have_meter = false;

      for (int i = 0; i < count; ++i) {
        const int y0 = lanes [i].lane_root.y + lanes [i].meter_out.y;
        const int y1 = y0 + lanes [i].meter_out.h;
        if (!have_meter) {
          meter_top = y0;
          meter_bottom = y1;
          have_meter = true;
        } else {
          meter_top = std::min (meter_top, y0);
          meter_bottom = std::max (meter_bottom, y1);
        }
      }

      *top = meter_top;
      *height = std::max (1, meter_bottom - meter_top);
    };
    int meter_top = 0;
    int meter_h = std::max (1, page_h);
      
    const int lane_stack_h = std::max (
      1, lane_height * lane_count + total_gap);
    eqpage_pre.move_resize (16, lane_top, lane_width, lane_stack_h);
    eqpage_post.move_resize (16, lane_top, lane_width, lane_stack_h);

    if (visible_page == PAGE_EQPRE || visible_page == PAGE_EQPOST) {
      const int eq_meter = visible_page == PAGE_EQPRE ? 0 : 1;
      const _lane_bank eq_bank = bank_for_page (visible_page);
      meter_top = lane_top + 4;
      meter_h = std::max (1, lane_stack_h - 8);
      meter_in [eq_bank].move_resize (
        5, meter_top, METER_WIDTH, meter_h);
      meter_eqout [eq_meter].move_resize (
        16 + lane_width + 5, meter_top, METER_WIDTH, meter_h);
    } else if (page_has_bank (visible_page)) {
      c_lane_widgets *meter_ref_lanes = lanes_for_bank (visible_bank);
      size_t i;
      for (i = 0; i < NB_NUM_MODELS; i++) {
        meter_ref_lanes [i].move_resize (
          16, lane_top + i * (lane_height + lane_gap), lane_width, lane_height);
      }
      lane_meter_span (meter_ref_lanes, NB_NUM_MODELS, &meter_top, &meter_h);
      
      meter_in [visible_bank].move_resize (
        5, meter_top, METER_WIDTH, meter_h);
    } else {
      c_lane_widgets *meter_ref_lanes = lanes_for_bank (visible_bank);
      for (int i = 0; i < NB_NUM_MODELS; i++) {
        meter_ref_lanes [i].move_resize (
          16, lane_top + i * (lane_height + lane_gap), lane_width, lane_height);
      }
      lane_meter_span (meter_ref_lanes, NB_NUM_MODELS, &meter_top, &meter_h);

      meter_in [PAGE_OTHER].move_resize (5, meter_top, METER_WIDTH, meter_h);
      meter_masterout.move_resize (
        window_width - 5 - METER_WIDTH, meter_top, METER_WIDTH, meter_h);
      const int panelsplit = panelwidth * 2 / 5;
      frame_other_volumepresence.move_resize (16, 0, panelsplit - 12, 120);
      frame_other_noisegate.move_resize (panelsplit + 16, 0, panelwidth - panelsplit, 120);
      frame_other_linkexcl.move_resize (16, 132, panelwidth, 140);
      int frameothersize = frame_other_linkexcl.y + frame_other_linkexcl.h + 12;
      frame_other_misc.move_resize (
        16, frameothersize, panelwidth,
        std::max (1, cont_other.h - frameothersize));
      const int about_y = std::max (0, frame_other_misc.h - 60);
      btn_other_prefs.move (frame_other_misc.w - 280, about_y);
      btn_other_about.move (frame_other_misc.w - 140, about_y);
    }
    
    ui_resize_lock = false;
  }
}

void c_neuralblender_ui::destroy () { CP
  if (ui_ready)
    if (nbtk_app.backend)
      nbtk_app.backend->shutdown_app (&app);

  memset (&app, 0, sizeof (app));
  display = NULL;
  nbtk_app.native_app = NULL;
  nbtk_app.display = NULL;
  window = 0;
  mainwindow.widget = NULL;
  mainwindow.window = 0;
  ui_ready = false;
}

c_lane_widgets *c_neuralblender_ui::lanes_for_bank (_lane_bank bank) {
  switch (bank) {
    case BANK_PEDAL: return lanes_pedals;
    case BANK_CAB:   return lanes_cabs;
    case BANK_AMP:
    default:         return lanes_models;
  }
}

const c_lane_widgets *c_neuralblender_ui::lanes_for_bank (_lane_bank bank) const {
  switch (bank) {
    case BANK_PEDAL: return lanes_pedals;
    case BANK_CAB:   return lanes_cabs;
    case BANK_AMP:
    default:         return lanes_models;
  }
}

c_meterwidget &c_neuralblender_ui::input_meter_for_bank (_lane_bank bank) {
  if (bank < BANK_PEDAL || bank >= BANK_COUNT)
    bank = BANK_AMP;
  return meter_in [bank];
}

c_vudata &c_neuralblender_ui::input_vudata_for_bank (_lane_bank bank) {
  if (bank < BANK_PEDAL || bank >= BANK_COUNT)
    bank = BANK_AMP;
  return vudata_in [bank];
}

static bool meter_dirty (c_meterwidget &meter) {
  c_vudata *data = meter.get_vudata ();
  if (!meter.visible || !data)
    return false;

  return data->needs_redraw.exchange (false, std::memory_order_acq_rel);
}

void c_neuralblender_ui::redraw_visible_meters () {
  if (!state.do_vu)
    return;

  auto redraw_meter = [&] (c_meterwidget &meter) {
    if (meter_dirty (meter)) {
      mainwindow.redraw_child (meter);
      return true;
    }
    return false;
  };

  if (visible_page == PAGE_OTHER) {
    redraw_meter (meter_in [PAGE_OTHER]);
    redraw_meter (meter_masterout);
  } else if (visible_page == PAGE_EQPRE || visible_page == PAGE_EQPOST) {
    const int eq_meter = visible_page == PAGE_EQPRE ? 0 : 1;
    const _lane_bank eq_bank = bank_for_page (visible_page);
    redraw_meter (meter_in [eq_bank]);
    redraw_meter (meter_eqout [eq_meter]);
  } else {
    redraw_meter (meter_in [visible_bank]);
    c_lane_widgets *bank_lanes = lanes_for_bank (visible_bank);
    for (int i = 0; i < NB_NUM_MODELS; i++)
      redraw_meter (bank_lanes [i].meter_out);
  }
}

void c_neuralblender_ui::redraw_tuner_if_needed () {
  if (state.tuner_on && tuner.on_ui_timer ())
    mainwindow.redraw_child (tuner);
}

int c_neuralblender_ui::exclusive_lane_for_bank (_lane_bank bank) const {
  if (bank < BANK_PEDAL || bank >= BANK_COUNT)
    bank = BANK_AMP;
  return state.banks [bank].exclusive_lane;
}

void c_neuralblender_ui::set_exclusive_lane_for_bank (_lane_bank bank, int lane) {
  if (bank < BANK_PEDAL || bank >= BANK_COUNT)
    bank = BANK_AMP;
  state.banks [bank].exclusive_lane = std::clamp (lane, 0, (int) NB_NUM_MODELS);
}

bool c_neuralblender_ui::linked_calib_for_bank (_lane_bank bank) const {
  if (bank < BANK_PEDAL || bank >= BANK_COUNT)
    bank = BANK_AMP;
  return state.banks [bank].linked_calib;
}

void c_neuralblender_ui::set_linked_calib_for_bank (_lane_bank bank, bool b) {
  if (bank < BANK_PEDAL || bank >= BANK_COUNT)
    bank = BANK_AMP;
  state.banks [bank].linked_calib = b;
}

void c_neuralblender_ui::on_bank_switch (nbtk::c_widget *w, int n) { CP
  (void) w;
  if (n >= PAGE_PEDAL && n < PAGE_COUNT) {
    visible_page = (_ui_page) n;
    if (visible_page != PAGE_OTHER)
      visible_bank = bank_for_page (visible_page);
  }

  sync_widgets_from_state (state);
  move_resize ();
}

void c_neuralblender_ui::sync_page_visibility () {
  if (visible_page == PAGE_PEDAL)
    cont_pedals.show ();
  else
    cont_pedals.hide ();

  if (visible_page == PAGE_EQPRE)
    cont_eqpre.show ();
  else
    cont_eqpre.hide ();

  if (visible_page == PAGE_AMP)
    cont_models.show ();
  else
    cont_models.hide ();

  if (visible_page == PAGE_EQPOST)
    cont_eqpost.show ();
  else
    cont_eqpost.hide ();

  if (visible_page == PAGE_CAB)
    cont_cabs.show ();
  else
    cont_cabs.hide ();

  if (visible_page == PAGE_OTHER)
    cont_other.show ();
  else
    cont_other.hide ();

  btn_tab_pedals.set_value (visible_page == PAGE_PEDAL);
  btn_tab_eqpre.set_value (visible_page == PAGE_EQPRE);
  btn_tab_models.set_value (visible_page == PAGE_AMP);
  btn_tab_eqpost.set_value (visible_page == PAGE_EQPOST);
  btn_tab_cabs.set_value (visible_page == PAGE_CAB);
  btn_tab_other.set_value (visible_page == PAGE_OTHER);

  sync_bank_tab_icon (btn_tab_pedals, state, BANK_PEDAL);
  sync_bank_tab_icon (btn_tab_eqpre, state, BANK_EQPRE);
  sync_bank_tab_icon (btn_tab_models, state, BANK_AMP);
  sync_bank_tab_icon (btn_tab_eqpost, state, BANK_EQPOST);
  sync_bank_tab_icon (btn_tab_cabs, state, BANK_CAB);
}

void c_neuralblender_ui::ensure_tuner_created () {
  if (tuner.created || !mainwindow.is_created ())
    return;

  int metrics_w = 0;
  mainwindow.get_metrics (&metrics_w, NULL);
  const int w = std::max (1, metrics_w - 8);
  tuner.create (&cont_toparea, "", 4, 4, w, tuner_height);
  if (blender)
    tuner.set_pitchtracker (&blender->pitchtracker);
}

void c_neuralblender_ui::sync_tuner_visibility () {
  btn_tuner.set_value (state.tuner_on);
  if (state.tuner_on) {
    ensure_tuner_created ();
    tuner.show ();
    img_logo.hide ();
    tuner.invalidate_base ();
  } else {
    tuner.hide ();
    img_logo.show ();
    img_logo.invalidate ();
  }
}

static bool page_has_bank (_ui_page page) {
  return page == PAGE_PEDAL || page == PAGE_AMP || page == PAGE_CAB;
}

static bool page_has_bypass (_ui_page page) {
  return page == PAGE_PEDAL ||
         page == PAGE_EQPRE ||
         page == PAGE_AMP ||
         page == PAGE_EQPOST ||
         page == PAGE_CAB;
}

static _lane_bank bank_for_page (_ui_page page) {
  switch (page) {
    case PAGE_PEDAL:   return BANK_PEDAL;
    case PAGE_EQPRE:   return BANK_EQPRE;
    case PAGE_AMP:     return BANK_AMP;
    case PAGE_EQPOST:  return BANK_EQPOST;
    case PAGE_CAB:     return BANK_CAB;
    case PAGE_OTHER:
    default:           return BANK_AMP;
  }
}

static const char *cwd_config_key_for_bank_ui (_lane_bank bank) {
  return bank == BANK_CAB ? CONFIG_KEY_NAME_IR_CWD : CONFIG_KEY_NAME_MODEL_CWD;
}

static bool bank_bypass_for_state (
    const c_neuralblender_state &state,
    _lane_bank bank) {

  switch (bank) {
    case BANK_PEDAL: return state.pedal_bypass;
    case BANK_EQPRE: return state.eqpre_bypass;
    case BANK_CAB:   return state.cab_bypass;
    case BANK_EQPOST:return state.eqpost_bypass;
    case BANK_AMP:
    default:         return state.amp_bypass;
  }
}

static void set_bank_bypass_for_state (
    c_neuralblender_state &state,
    _lane_bank bank,
    bool bypass) {

  switch (bank) {
    case BANK_PEDAL:
      state.pedal_bypass = bypass;
    break;

    case BANK_EQPRE:
      state.eqpre_bypass = bypass;
    break;

    case BANK_CAB:
      state.cab_bypass = bypass;
    break;

    case BANK_EQPOST:
      state.eqpost_bypass = bypass;
    break;

    case BANK_AMP:
    default:
      state.amp_bypass = bypass;
    break;
  }
}

static void sync_bank_tab_icon (
    nbtk::c_button &button,
    const c_neuralblender_state &state,
    _lane_bank bank) {

  button.set_image_default (
    bank_bypass_for_state (state, bank)
      ? data_icon_power_grey_png
      : data_icon_power_on_png);
}

void c_neuralblender_ui::on_action (nbtk::t_action_event &event) {
  if (updating_from_state)
    return;

  for (_lane_bank bank_id : MODEL_BANKS) {
    if (event.handled)
      break;

    c_lane_widgets *bank_lanes = lanes_for_bank (bank_id);
    for (size_t i = 0; i < NB_NUM_MODELS && !event.handled; ++i)
      bank_lanes [i].on_action (event);
  }
  if (event.handled)
    return;

  auto finish = [&] () {
    event.handled = true;
    sync_widgets_from_state (state);
  };

  auto handle_bank_switch = [&] (
      nbtk::c_button &button,
      _ui_page page,
      _lane_bank bank) {
    if (event.source_id != button.id)
      return false;

    const bool right_click = event.mouse_button == Button3;
    if (page == PAGE_OTHER) {
      if (visible_page != PAGE_OTHER && right_click && page_has_bank (visible_page)) {
        const int exclusive_lane =
          exclusive_lane_for_bank (visible_bank) ? 0 : (int) choose_exclusive_lane ();
        on_excl (nullptr, exclusive_lane);
      } else if (visible_page != PAGE_OTHER) {
        on_bank_switch (&button, page);
      }
    } else if (page_has_bypass (page) &&
        ((right_click && prefs.bypass_rightclick) ||
        (visible_page == page && prefs.bypass_doubleclick))) {
      const bool bypass = !bank_bypass_for_state (state, bank);
      set_bank_bypass_for_state (state, bank, bypass);
      on_bank_bypass (&button, bank, bypass);
    } else if (page_has_bypass (page) && right_click) {
      // Right-click bank buttons are reserved for bank bypass when enabled.
    } else {
      on_bank_switch (&button, page);
    }
    finish ();
    return true;
  };

  if (handle_bank_switch (btn_tab_pedals, PAGE_PEDAL, BANK_PEDAL) ||
      handle_bank_switch (btn_tab_eqpre, PAGE_EQPRE, BANK_EQPRE) ||
      handle_bank_switch (btn_tab_models, PAGE_AMP, BANK_AMP) ||
      handle_bank_switch (btn_tab_eqpost, PAGE_EQPOST, BANK_EQPOST) ||
      handle_bank_switch (btn_tab_cabs, PAGE_CAB, BANK_CAB) ||
      handle_bank_switch (btn_tab_other, PAGE_OTHER, BANK_AMP))
    return;

  auto handle_button = [&] (nbtk::c_button &button, _widget_role role, uint64_t bank) {
    if (event.source_id != button.id)
      return false;

    const bool value = button.value;

    switch (role) {
      case ROLE_BYPASS:
        state.bypass = !value;
        on_bypass (&button, value);
      break;

      case ROLE_MUTEALL:
        state.mute_all = value;
        on_muteall (&button, value);
      break;

      case ROLE_NOISEGATE:
        state.noisegate_on = value;
        on_noisegate (&button, value);
      break;

      case ROLE_TUNER:
        state.tuner_on = &button == &img_logo ? true : value;
        on_tuner (&button, state.tuner_on);
        sync_tuner_visibility ();
      break;

      case ROLE_BANK_BYPASS:
        if (bank < BANK_COUNT) {
          const _lane_bank b = (_lane_bank) bank;
          set_bank_bypass_for_state (state, b, value);
          on_bank_bypass (&button, b, value);
        }
      break;

      case ROLE_LINKED_CALIB:
        if (bank < BANK_COUNT)
          visible_bank = (_lane_bank) bank;
        if (page_has_bank (visible_page) || bank < BANK_COUNT) {
          const _lane_bank b = bank < BANK_COUNT ? (_lane_bank) bank : visible_bank;
          set_linked_calib_for_bank (b, value);
          on_linked_calib (&button, value);
        }
      break;

      case ROLE_EXCL_TOGGLE:
        if (bank < BANK_COUNT)
          visible_bank = (_lane_bank) bank;
        if (value) {
          size_t exclusive_lane = choose_exclusive_lane ();
          on_excl (&button, exclusive_lane);
        } else {
          on_excl (&button, 0);
        }
      break;

      case ROLE_CALIBBASS:
        state.calib_source = value ? 1 : 0;
        on_calib_bass (&button, value);
      break;

      case ROLE_VUTOGGLE:
        state.do_vu = value;
        vu_on (value);
        on_vu (&button, value);
      break;

      case ROLE_PREFS:
        write_prefs_to (prefs);
        prefswindow.get_prefs_from (prefs);
        prefswindow.show ();
        on_prefs ();
      break;

      case ROLE_ABOUT:
        aboutwindow.show ();
        on_about ();
      break;

      case ROLE_TUNER_UP:
        state.tuner_base_freq *= SEMITONE_MULTIPLIER;
        on_tuner_base_freq (&button, state.tuner_base_freq);
      break;

      case ROLE_TUNER_DOWN:
        state.tuner_base_freq /= SEMITONE_MULTIPLIER;
        on_tuner_base_freq (&button, state.tuner_base_freq);
      break;

      case ROLE_TUNER_DEFAULT:
        state.tuner_base_freq = 440.0f;
        on_tuner_base_freq (&button, state.tuner_base_freq);
      break;

      default:
      break;
    }

    finish ();
    return true;
  };

  if (handle_button (btn_enable, ROLE_BYPASS, visible_bank) ||
      handle_button (btn_muteall, ROLE_MUTEALL, visible_bank) ||
      handle_button (btn_noisegate, ROLE_NOISEGATE, visible_bank) ||
      handle_button (btn_other_noisegate, ROLE_NOISEGATE, visible_bank) ||
      handle_button (btn_tuner, ROLE_TUNER, visible_bank) ||
      handle_button (img_logo, ROLE_TUNER, visible_bank) ||
      handle_button (btn_other_byp_pedal, ROLE_BANK_BYPASS, BANK_PEDAL) ||
      handle_button (btn_other_byp_eq1, ROLE_BANK_BYPASS, BANK_EQPRE) ||
      handle_button (btn_other_byp_amp, ROLE_BANK_BYPASS, BANK_AMP) ||
      handle_button (btn_other_byp_eq2, ROLE_BANK_BYPASS, BANK_EQPOST) ||
      handle_button (btn_other_byp_cab, ROLE_BANK_BYPASS, BANK_CAB) ||
      handle_button (btn_other_link_pedal, ROLE_LINKED_CALIB, BANK_PEDAL) ||
      handle_button (btn_other_link_amp, ROLE_LINKED_CALIB, BANK_AMP) ||
      handle_button (btn_other_link_cab, ROLE_LINKED_CALIB, BANK_CAB) ||
      handle_button (btn_other_excl_pedal, ROLE_EXCL_TOGGLE, BANK_PEDAL) ||
      handle_button (btn_other_excl_amp, ROLE_EXCL_TOGGLE, BANK_AMP) ||
      handle_button (btn_other_excl_cab, ROLE_EXCL_TOGGLE, BANK_CAB) ||
      handle_button (btn_other_vu, ROLE_VUTOGGLE, visible_bank) ||
      handle_button (btn_other_bass, ROLE_CALIBBASS, visible_bank) ||
      handle_button (btn_other_prefs, ROLE_PREFS, visible_bank) ||
      handle_button (btn_other_about, ROLE_ABOUT, visible_bank) ||
      handle_button (btn_other_tuner_down, ROLE_TUNER_DOWN, visible_bank) ||
      handle_button (btn_other_tuner_up, ROLE_TUNER_UP, visible_bank) ||
      handle_button (btn_other_tuner_default, ROLE_TUNER_DEFAULT, visible_bank))
    return;

  if (event.source_id == tuner.id) {
    state.tuner_on = false;
    on_tuner (&tuner, false);
    sync_tuner_visibility ();
    finish ();
    return;
  }

  auto handle_eq_widget = [&] (nbtk::c_widget *widget) {
    if (!widget || event.source_id != widget->id ||
        widget->bank >= BANK_COUNT ||
        widget->lane >= EQ_NUM_BANDS)
      return false;

    const _lane_bank bank = (_lane_bank) widget->bank;
    if (bank != BANK_EQPRE && bank != BANK_EQPOST)
      return false;

    c_eq_state &eq =
      bank == BANK_EQPRE ? state.eqpre : state.eqpost;
    const size_t band = widget->lane;
    bool changed_parameter = false;

    switch ((_widget_role) widget->role) {
      case ROLE_EQ_ENABLED:
        eq.enabled [band] = static_cast<nbtk::c_button *> (widget)->value;
      break;

      case ROLE_EQ_MODE:
        eq.mode [band] = (_eq_band_mode) std::clamp (
          static_cast<nbtk::c_combobox *> (widget)->selected + 1,
          (int) EQ_HIPASS,
          (int) EQ_LOWPASS);
        changed_parameter = true;
      break;

      case ROLE_EQ_FREQ:
        eq.freq [band] = std::clamp (
          static_cast<nbtk::c_knob *> (widget)->value,
          20.0f,
          20000.0f);
        changed_parameter = true;
      break;

      case ROLE_EQ_GAIN:
        eq.gain_db [band] = std::clamp (
          static_cast<nbtk::c_slider *> (widget)->real_value (),
          -36.0f,
          36.0f);
        changed_parameter = true;
      break;

      case ROLE_EQ_Q:
        eq.q [band] = std::clamp (
          static_cast<nbtk::c_knob *> (widget)->value,
          0.01f,
          100.0f);
        changed_parameter = true;
      break;

      default:
        return false;
    }

    if (eq_auto_enable && changed_parameter)
      eq.enabled [band] = true;

    on_eq_band (widget, bank, band);
    finish ();
    return true;
  };

  if (handle_eq_widget (event.source))
    return;

  if (event.source &&
      event.source->role == ROLE_EQ_MASTER_GAIN &&
      (event.source->bank == BANK_EQPRE || event.source->bank == BANK_EQPOST)) {
    const _lane_bank bank = (_lane_bank) event.source->bank;
    c_eq_state &eq =
      bank == BANK_EQPRE ? state.eqpre : state.eqpost;
    eq.master_gain_db = std::clamp (
      static_cast<nbtk::c_knob *> (event.source)->value,
      -36.0f,
      36.0f);
    on_eq_master_gain (event.source, bank, eq.master_gain_db);
    finish ();
    return;
  }

  auto handle_knob = [&] (nbtk::c_knob &knob, _widget_role role) {
    if (event.source_id != knob.id)
      return false;

    const float value = knob.value;
    switch (role) {
      case ROLE_MASTER:
        state.master_gain = db_to_gain (value);
        on_master_gain (&knob, value);
      break;

      case ROLE_PRESENCE:
        state.presence = value;
        on_presence (&knob, value);
      break;

      case ROLE_NOISETHRESH:
        state.noisethresh = value;
        on_noisethresh (&knob, value);
      break;

      case ROLE_NOISEATTACK:
        state.noiseattack = value;
        on_noiseattack (&knob, value);
      break;

      case ROLE_NOISEHOLD:
        state.noisehold = value;
        on_noisehold (&knob, value);
      break;

      case ROLE_NOISERELEASE:
        state.noiserelease = value;
        on_noiserelease (&knob, value);
      break;

      default:
      break;
    }
    finish ();
    return true;
  };

  if (handle_knob (knob_mastervolume, ROLE_MASTER) ||
      handle_knob (knob_presence, ROLE_PRESENCE) ||
      handle_knob (knob_noisethresh, ROLE_NOISETHRESH) ||
      handle_knob (knob_noiseattack, ROLE_NOISEATTACK) ||
      handle_knob (knob_noisehold, ROLE_NOISEHOLD) ||
      handle_knob (knob_noiserelease, ROLE_NOISERELEASE))
    return;

  auto handle_textbox = [&] (nbtk::c_textbox &textbox, _widget_role role) {
    if (event.source_id != textbox.id)
      return false;

    char *end = NULL;
    const float value = std::strtof (textbox.text ().c_str (), &end);
    if (end && end != textbox.text ().c_str ()) {
      if (role == ROLE_TUNER_BASE_FREQ)
        on_tuner_base_freq (&textbox, std::clamp (value, 400.0f, 480.0f));
      else if (role == ROLE_CALIB_TARGET_DB)
        on_calib_target_db (
          &textbox,
          std::clamp (value, CALIB_TARGET_DB_MIN, CALIB_TARGET_DB_MAX));
    }

    finish ();
    return true;
  };

  if (handle_textbox (text_other_tuner, ROLE_TUNER_BASE_FREQ) ||
      handle_textbox (text_other_calib, ROLE_CALIB_TARGET_DB))
    return;
}

void c_neuralblender_ui::set_threshgain (float f) {
  meter_in [BANK_PEDAL].set_compression_gain (f);
  for (size_t bank = BANK_PEDAL + 1; bank < BANK_COUNT; ++bank)
    meter_in [bank].set_compression_gain (1.0f);
  meter_eqout [0].set_compression_gain (1.0f);
  meter_eqout [1].set_compression_gain (1.0f);
  meter_in [PAGE_OTHER].set_compression_gain (f);
  meter_masterout.set_compression_gain (1.0f);
}

void c_neuralblender_ui::on_about () { CP }

void c_neuralblender_ui::on_prefs () { CP }

void c_neuralblender_ui::on_prefs_ok () {
}

void c_neuralblender_ui::apply_ui_prefs (t_prefs &p) { CP
  char buf [128];
  const float scale_db = p.vu_scale_db <= 0.0f ? p.vu_scale_db : DEFAULT_VU_DB;
  const float headroom_db = std::clamp (p.vu_headroom_db, 0.0f, 12.0f);

  nbtk_app.show_tooltips = p.show_tooltips;
  if (!nbtk_app.show_tooltips)
    nbtk_app.hide_tooltip ();

  for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
    meter_in [bank].set_db_scale (scale_db);
    meter_in [bank].set_headroom (headroom_db);
    vudata_in [bank].set_db_scale (scale_db);
    vudata_in [bank].set_headroom (headroom_db);
  }
  for (size_t i = 0; i < 2; ++i) {
    meter_eqout [i].set_db_scale (scale_db);
    meter_eqout [i].set_headroom (headroom_db);
  }
  meter_in [PAGE_OTHER].set_db_scale (scale_db);
  meter_in [PAGE_OTHER].set_headroom (headroom_db);
  meter_masterout.set_db_scale (scale_db);
  meter_masterout.set_headroom (headroom_db);
  vudata_masterin.set_db_scale (scale_db);
  vudata_masterin.set_headroom (headroom_db);
  vudata_masterout.set_db_scale (scale_db);
  vudata_masterout.set_headroom (headroom_db);

  for (_lane_bank bank_id : MODEL_BANKS) {
    c_lane_widgets *bank_lanes = lanes_for_bank (bank_id);
    for (size_t i = 0; i < NB_NUM_MODELS; i++) {
      bank_lanes [i].meter_out.set_db_scale (scale_db);
      bank_lanes [i].meter_out.set_headroom (headroom_db);
      bank_lanes [i].vudata_out.set_db_scale (scale_db);
      bank_lanes [i].vudata_out.set_headroom (headroom_db);
    }
  }

  btn_other_bass.set_value (state.calib_source == 1);

  vu_on (state.do_vu);

  btn_noisegate.set_value (state.noisegate_on);
  btn_other_noisegate.set_value (state.noisegate_on);
  knob_mastervolume.set_value (gain_to_db (state.master_gain));
  knob_presence.set_value (state.presence);
  knob_noisethresh.set_value (state.noisethresh);
  knob_noiseattack.set_value (state.noiseattack);
  knob_noisehold.set_value (state.noisehold);
  knob_noiserelease.set_value (state.noiserelease);

  format_freq_text (buf, sizeof (buf), state.tuner_base_freq);
  text_other_tuner.set_text (buf);
  format_db_text (buf, sizeof (buf), state.calib_target_db);
  text_other_calib.set_text (buf);

  sync_tuner_visibility ();
}

void c_neuralblender_ui::apply_prefs (t_prefs &p) { CP
  calib_default = p.calib_default;
  apply_ui_prefs (p);
}

void c_neuralblender_ui::write_prefs_to (t_prefs &p) { CP
  p.calib_default = calib_default;
}

// called from lv2_ui - runs in UI thread
void c_neuralblender_ui::update_stats () {
  char buf [128];
  
  static const char *engine_names [] = {
    "",
    "NAM A1",
    "NAM A2",
    "JSON",
    "IR",
    "(unknown)",
    NULL
  };
  
  for (_lane_bank bank_id : MODEL_BANKS) {
    const size_t bank = (size_t) bank_id;
    c_lane_widgets *bank_lanes = lanes_for_bank (bank_id);

    for (size_t i = 0; i < NB_NUM_MODELS; i++) {
      const size_t n = i * UI_STATS_PER_LANE;
      int nframes = stats [bank] [n];
      float trim = stats [bank] [n + 1];
      int eng = (int) stats [bank] [n + 2];
      if (eng < ENGINE_NONE || eng > ENGINE_UNKNOWN)
        eng = ENGINE_UNKNOWN;
      
      bank_lanes [i].label_engine.label = engine_names [eng];
      bank_lanes [i].label_engine.invalidate ();
      const bool lane_loaded =
        state.banks [bank].lanes [i].loaded;
      const bool show_ir_pitch =
        eng == ENGINE_IR || (bank == BANK_CAB && !lane_loaded);

      if (show_ir_pitch) {
        bank_lanes [i].knob_gain_in.hide ();
        bank_lanes [i].knob_ir_pitch.show ();
      } else {
        bank_lanes [i].knob_ir_pitch.hide ();
        bank_lanes [i].knob_gain_in.show ();
      }

      snprintf (buf, 127, "%d frames", nframes);
      bank_lanes [i].label_frames.label = buf;
      bank_lanes [i].label_frames.invalidate ();
      if (trim == 1.00) {
        bank_lanes [i].label_trim.label.clear ();
        bank_lanes [i].label_trim.invalidate ();
      } else {
        float db = gain_to_db (trim);
        snprintf (buf, 127, "Trim: %s%.02fdB", db > 0.0 ? "+" : "", db);
        bank_lanes [i].label_trim.label = buf;
        bank_lanes [i].label_trim.invalidate ();
      }
    }
  }
}

/*void c_neuralblender_ui::show_advanced_settings (bool b) {
  //show_advanced = b;
  state.showadvanced = b;
  move_resize (true);
}

void c_neuralblender_ui::hide_advanced_settings () {
  show_advanced_settings (false);
}*/

void c_neuralblender_ui::vu_on (bool b) { CP
  if (!b) {
    vu_off ();
    return;
  }

  state.do_vu = true;

  for (_lane_bank bank_id : MODEL_BANKS) {
    const size_t bank = (size_t) bank_id;
    meter_in [bank].show ();
    c_lane_widgets *bank_lanes = lanes_for_bank (bank_id);
    for (size_t i = 0; i < NB_NUM_MODELS; i++)
      bank_lanes [i].meter_out.show ();
  }
  meter_in [BANK_EQPRE].show ();
  meter_in [BANK_EQPOST].show ();
  meter_eqout [0].show ();
  meter_eqout [1].show ();
  meter_in [PAGE_OTHER].show ();
  meter_masterout.show ();
  //on_vu (&btn_vu, b);
}

void c_neuralblender_ui::vu_off () { CP
  state.do_vu = false;

  for (_lane_bank bank_id : MODEL_BANKS) {
    const size_t bank = (size_t) bank_id;
    meter_in [bank].hide ();
    c_lane_widgets *bank_lanes = lanes_for_bank (bank_id);
    for (size_t i = 0; i < NB_NUM_MODELS; i++)
      bank_lanes [i].meter_out.hide ();
  }
  meter_in [BANK_EQPRE].hide ();
  meter_in [BANK_EQPOST].hide ();
  meter_eqout [0].hide ();
  meter_eqout [1].hide ();
  meter_in [PAGE_OTHER].hide ();
  meter_masterout.hide ();
  //on_vu (&btn_vu, false);
}

size_t c_neuralblender_ui::choose_exclusive_lane () const {
  const c_neuralblender_bank_state &bank_state = state.banks [visible_bank];

  const int current_exclusive = exclusive_lane_for_bank (visible_bank);
  if (current_exclusive > 0 &&
      current_exclusive <= (int) NB_NUM_MODELS)
    return (size_t) current_exclusive;

  if (last_exclusive_lane [visible_bank] > 0 &&
      last_exclusive_lane [visible_bank] <= NB_NUM_MODELS)
    return last_exclusive_lane [visible_bank];

  for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
    if (!bank_state.lanes [i].filename.empty () &&
        !bank_state.lanes [i].lane_mute)
      return i + 1;
  }

  for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
    //if (!filepickers [i].selected_file.empty ())
    if (!bank_state.lanes [i].filename.empty ())
      return i + 1;
  }

  for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
    if (!bank_state.lanes [i].lane_mute)
      return i + 1;
  }

  return 1;
}

void c_neuralblender_ui::on_window_resize (int w, int h) {
  if (!ui_ready || ui_resize_lock)
    return;

  debug ("on_window_resize: %d,%d", w, h);
  pending_resize_w = w;
  pending_resize_h = h;
  ui_resize_pending = true;
}

void c_neuralblender_ui::on_window_configured () {
  if (!ui_ready || ui_resize_lock || !ui_resize_pending)
    return;

  debug ("on_window_configured: consume pending resize %d,%d",
         pending_resize_w, pending_resize_h);
  ui_resize_pending = false;
  move_resize ();
  mainwindow.show_children ();
  sync_tuner_visibility ();
  pending_resize_w = 0;
  pending_resize_h = 0;
}

bool c_neuralblender_ui::request_window_size (int w, int h) {
  return mainwindow.request_size (w, h);
}

void c_neuralblender_ui::on_excl (nbtk::c_widget *w, int n) {
  const _lane_bank bank =
    w && w->bank < BANK_COUNT ? (_lane_bank) w->bank : visible_bank;

  debug ("n=%d", n);
  set_exclusive_lane_for_bank (bank, n);
  if (n > 0 && n <= (int) NB_NUM_MODELS)
    last_exclusive_lane [bank] = (size_t) n;

  switch (bank) {
    case BANK_PEDAL:
      btn_other_excl_pedal.set_value (exclusive_lane_for_bank (BANK_PEDAL) != 0);
    break;
    case BANK_CAB:
      btn_other_excl_cab.set_value (exclusive_lane_for_bank (BANK_CAB) != 0);
    break;
    case BANK_AMP:
    default:
      btn_other_excl_amp.set_value (exclusive_lane_for_bank (BANK_AMP) != 0);
    break;
  }
  apply_effective_controls();
  //sync_widgets_from_state (state);
}

void c_neuralblender_ui::on_excl_use (nbtk::c_widget *w, bool b) {
  (void) b;
  if (!w)
    return;

  debug ("lane %d, value=%d", (int) w->lane + 1, (int) b);

  on_excl (w, (int) w->lane + 1);
}

/*void c_neuralblender_ui::on_advanced (c_widget *w, bool b) {
  (void) w;
  debug ("b=%d", (int) b);
  show_advanced_settings (b);
  configfile.set_item (CONFIG_KEY_NAME_ADV, b ? "1" : "0");
  configfile.write_file ();
}*/

int c_neuralblender_ui::idle () {
  if (!ui_ready) {
    CP
    return 0;
  }

  if (ui_resize_pending && !ui_resize_lock) {
    debug ("idle: consume pending resize %d,%d", pending_resize_w, pending_resize_h);
    ui_resize_pending = false;
    move_resize ();
    pending_resize_w = 0;
    pending_resize_h = 0;
  }

  if (nbtk_app.backend)
    nbtk_app.backend->run_events (&app);

  nbtk_app.tick ();

  redraw_visible_meters ();
  
  redraw_tuner_if_needed ();

  if (nbtk_app.backend)
    nbtk_app.backend->flush_dirty (&app);

  //static c_printfps fps ("UI idle: ");
  //fps.tick ();

  return 0;
}

void c_neuralblender_ui::draw () {
  if (!mainwindow.is_created ())
    return;

  mainwindow.force_draw ();
}

bool c_neuralblender_ui::load_model (size_t which, const char *filename) {
  return load_model (BANK_AMP, which, filename);
}

void c_neuralblender_ui::clear_lane_model_ui (_lane_bank bank, size_t which) {
  if (which >= NB_NUM_MODELS)
    return;

  //filepickers [which].selected_file.clear ();
  if (bank < BANK_PEDAL || bank >= BANK_COUNT)
    bank = BANK_AMP;

  state.banks [bank].lanes [which].filename.clear ();
  lanes_for_bank (bank) [which].menu_list.clear ();
}

void c_neuralblender_ui::clear_lane_model_ui (size_t which) {
  clear_lane_model_ui (BANK_AMP, which);
}

void c_neuralblender_ui::set_lane_mute (_lane_bank bank, size_t which, bool b) {
  debug ("which=%d, b=%d", (int) which, (int) b);
  if (which >= NB_NUM_MODELS)
    return;

  if (bank < BANK_PEDAL || bank >= BANK_COUNT)
    bank = BANK_AMP;

  const bool old_updating = updating_from_state;
  updating_from_state = true;

  c_lane_widgets *lanes = lanes_for_bank (bank);
  lanes [which].btn_mute.value = b;
  lanes [which].btn_mute.set_value (b);

  updating_from_state = old_updating;
}

void c_neuralblender_ui::set_lane_mute (size_t which, bool b) {
  set_lane_mute (BANK_AMP, which, b);
}

void c_neuralblender_ui::apply_effective_controls () {
}

// calibration default is written to config ONLY if all
// calib check boxes are on/off
void c_neuralblender_ui::write_calib_state_if_consistent () {
  bool all_on = true;
  bool all_off = true;
  const c_neuralblender_bank_state &bank_state = state.banks [visible_bank];

  for (size_t i = 0; i < NB_NUM_MODELS && i < NB_NUM_MODELS; ++i) {
    all_on  &= bank_state.lanes [i].do_calib;
    all_off &= !bank_state.lanes [i].do_calib;
  }

  if (!all_on && !all_off)
    return;

  configfile.set_item (CONFIG_KEY_NAME_CALIB, all_on ? "1" : "0");
}

void c_neuralblender_ui::sync_widgets_from_state (const c_neuralblender_state &state_,
                                                  bool scan_dirs) {
  if (!ui_ready)
    return;
  const bool showadvanced = state.showadvanced;
  this->state = state_;
  this->state.showadvanced = showadvanced;
  for (_lane_bank bank_id : MODEL_BANKS) {
    const size_t bank = (size_t) bank_id;
    const int n = this->state.banks [bank].exclusive_lane;
    if (n > 0 && n <= (int) NB_NUM_MODELS)
      last_exclusive_lane [bank] = (size_t) n;
  }

  updating_from_state = true;
  
  sync_page_visibility ();
  
  btn_noisegate.set_value (state.noisegate_on);
  btn_other_noisegate.set_value (state.noisegate_on);
  knob_mastervolume.set_value (gain_to_db (state.master_gain));
  knob_presence.set_value (state.presence);
  knob_noisethresh.set_value (state.noisethresh);
  knob_noiseattack.set_value (state.noiseattack);
  knob_noisehold.set_value (state.noisehold);
  knob_noiserelease.set_value (state.noiserelease);
  char buf [128];
  format_freq_text (buf, sizeof (buf), state.tuner_base_freq);
  text_other_tuner.set_text (buf);
  format_db_text (buf, sizeof (buf), state.calib_target_db);
  text_other_calib.set_text (buf);
  /*if (state.noisegate_on)
    knob_noisethresh.show ();
  else
    knob_noisethresh.hide ();*/

  for (_lane_bank bank_id : MODEL_BANKS) {
    const size_t bank = (size_t) bank_id;
    c_lane_widgets *bank_lanes = lanes_for_bank (bank_id);
    const c_neuralblender_bank_state &bank_state = state.banks [bank];

    for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
      const c_neuralblender_lane_state &lane = bank_state.lanes [i];

      bank_lanes [i].knob_gain_in.set_value (gain_to_db (lane.gain_in));
      bank_lanes [i].knob_ir_pitch.set_value (lane.ir_pitch_semitones);
      bank_lanes [i].knob_gain_out.set_value (gain_to_db (lane.gain_out));
      bank_lanes [i].knob_dry_out.set_value (
        lane.dry_out > 0.0f ? gain_to_db (lane.dry_out) : DB_SILENCE);
      bank_lanes [i].knob_delay.set_value (lane.delay_ms);
      bank_lanes [i].btn_flip.set_value (lane.dcflip);
      bank_lanes [i].btn_calib.set_value (lane.do_calib);

      //filepickers [i].selected_file = lane.filename;
      if (scan_dirs) {
        if (lane.filename.empty ()) {
          bank_lanes [i].menu_list.clear ();
        } else {
          c_neuralblender_filepicker &fp = bank_lanes [i].filepicker;
          if (fp.current_dir.empty ()) {
            const _lane_bank lane_bank = (_lane_bank) bank;
            fp.current_dir =
              configfile.get_item (cwd_config_key_for_bank_ui (lane_bank));
            if (fp.current_dir.empty ())
              fp.current_dir = CONFIG_DEFAULT_DIR;
          }
          fp.scan_current_dir ();
          fp.add_files_from_dir (&bank_lanes [i].menu_list);
        }
      }
    }
  }
  update_stats ();
  
  const bool enabled = !state.bypass;
  btn_enable.set_value (enabled);
  
  sync_tuner_visibility ();
  //btn_enable.set_label (enabled ? "Enabled" : " Bypass ");

  /*btn_advanced.set_value (state.showadvanced);
  show_advanced_settings (state.showadvanced);*/

  btn_other_bass.set_value (state.calib_source == 1);
  btn_other_byp_pedal.set_value (state.pedal_bypass);
  btn_other_byp_eq1.set_value (state.eqpre_bypass);
  btn_other_byp_amp.set_value (state.amp_bypass);
  btn_other_byp_eq2.set_value (state.eqpost_bypass);
  btn_other_byp_cab.set_value (state.cab_bypass);
  btn_other_link_pedal.set_value (linked_calib_for_bank (BANK_PEDAL));
  btn_other_link_amp.set_value (linked_calib_for_bank (BANK_AMP));
  btn_other_link_cab.set_value (linked_calib_for_bank (BANK_CAB));
  btn_other_vu.set_value (state.do_vu);
  eqpage_pre.sync_from_state (state.eqpre);
  eqpage_post.sync_from_state (state.eqpost);
  if (state.do_vu) {
    for (_lane_bank bank_id : MODEL_BANKS) {
      const size_t bank = (size_t) bank_id;
      meter_in [bank].show ();
      c_lane_widgets *bank_lanes = lanes_for_bank (bank_id);
      for (size_t i = 0; i < NB_NUM_MODELS; ++i)
        bank_lanes [i].meter_out.show ();
    }
    meter_in [BANK_EQPRE].show ();
    meter_in [BANK_EQPOST].show ();
    meter_eqout [0].show ();
    meter_eqout [1].show ();
    meter_in [PAGE_OTHER].show ();
    meter_masterout.show ();
  } else {
    for (_lane_bank bank_id : MODEL_BANKS) {
      const size_t bank = (size_t) bank_id;
      meter_in [bank].hide ();
      c_lane_widgets *bank_lanes = lanes_for_bank (bank_id);
      for (size_t i = 0; i < NB_NUM_MODELS; ++i)
        bank_lanes [i].meter_out.hide ();
    }
    meter_in [BANK_EQPRE].hide ();
    meter_in [BANK_EQPOST].hide ();
    meter_eqout [0].hide ();
    meter_eqout [1].hide ();
    meter_in [PAGE_OTHER].hide ();
    meter_masterout.hide ();
  }

  const int visible_exclusive_lane = exclusive_lane_for_bank (visible_bank);
  btn_other_excl_pedal.set_value (exclusive_lane_for_bank (BANK_PEDAL) > 0);
  btn_other_excl_amp.set_value (exclusive_lane_for_bank (BANK_AMP) > 0);
  btn_other_excl_cab.set_value (exclusive_lane_for_bank (BANK_CAB) > 0);

  if (!page_has_bank (visible_page)) {
    updating_from_state = false;
    return;
  }

  c_lane_widgets *visible_lanes = lanes_for_bank (visible_bank);
  const c_neuralblender_bank_state &visible_bank_state = state.banks [visible_bank];
  const bool exclusive_on = visible_exclusive_lane > 0;
  const bool visible_bank_bypassed =
    bank_bypass_for_state (state, visible_bank);
  for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
    const c_neuralblender_lane_state &lane = visible_bank_state.lanes [i];
    const bool selected =
      exclusive_on && visible_exclusive_lane == (int) i + 1;

    visible_lanes [i].btn_mute.set_value (lane.lane_mute);
    visible_lanes [i].btn_excl.set_value (selected);

    if (lane.lane_mute || state.mute_all || !enabled ||
        visible_bank_bypassed) { CP
      visible_lanes [i].set_state (nbtk::WSTATE_DISABLED);
    } else {
      visible_lanes [i].set_state (nbtk::WSTATE_NORMAL);
    }

    if (exclusive_on) { CP
      visible_lanes [i].set_state (nbtk::WSTATE_DISABLED);
      visible_lanes [i].btn_mute.hide ();
      visible_lanes [i].btn_excl.show ();
    } else { CP
      visible_lanes [i].btn_mute.show ();
      visible_lanes [i].btn_excl.hide ();
    }
  }
  if (exclusive_on && !state.mute_all && enabled && !visible_bank_bypassed) {
    visible_lanes [visible_exclusive_lane - 1].set_state (nbtk::WSTATE_SELECTED);
  }

  updating_from_state = false;
}
