
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

#include <X11/cursorfont.h>

#include "neuralblender.h"
#include "ui.h"

#include "data/data.h"

#define CMDLINE_DEBUG_COLOR ANSI_MAGENTA
#include "cmdline_debug.h"

#define MIN_WINDOW_HEIGHT (100 + (130 * NB_NUM_MODELS))
//#define DEFAULT_WINDOW_HEIGHT (12 + std::min (640, (52 + (180 * NB_NUM_MODELS))))
#define DEFAULT_WINDOW_HEIGHT MIN_WINDOW_HEIGHT
#define MIN_WINDOW_WIDTH 620
#define DEFAULT_WINDOW_WIDTH MIN_WINDOW_WIDTH

#define METER_WIDTH 5

extern const char *g_build_timestamp;

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

static bool get_parent_window_size (
    Display *display, Window parent,
    double hdpi,
    int *w, int *h) {

  if (!display || !parent || !w || !h)
    return false;

  XWindowAttributes attr;
  if (!XGetWindowAttributes (display, parent, &attr))
    return false;

  if (attr.width <= 0 || attr.height <= 0)
    return false;

  const double scale = hdpi > 0.0 ? hdpi : 1.0;
  const int logical_w = (int) (attr.width / scale);
  const int logical_h = (int) (attr.height / scale);

  if (logical_w < MIN_WINDOW_WIDTH || logical_h < MIN_WINDOW_HEIGHT)
    return false;

  // Avoid using the root window size as a startup hint in standalone mode.
  if (logical_w > 3000 || logical_h > 2200)
    return false;

  *w = logical_w;
  *h = logical_h;
  return true;
}

std::string path_dirname (const std::string &path) {
  const size_t pos = path.find_last_of ('/');
  if (pos == std::string::npos)
    return "";

  if (pos == 0)
    return "/";

  return path.substr (0, pos);
}

std::string path_basename (const std::string &path) {
  const size_t pos = path.find_last_of ('/');
  if (pos == std::string::npos)
    return path;

  return path.substr (pos + 1);
}

bool is_supported_model_filename (const std::string &path) {
  std::string lower = path;
  std::transform (lower.begin (), lower.end (), lower.begin (),
                  [] (unsigned char c) { return (char) std::tolower (c); });

  return (lower.size () >= 4 && lower.rfind (".nam") == lower.size () - 4) ||
         (lower.size () >= 5 && lower.rfind (".json") == lower.size () - 5) ||
         (lower.size () >= 6 && lower.rfind (".aidax") == lower.size () - 6) ||
         (lower.size () >= 4 && lower.rfind (".wav") == lower.size () - 4);
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
  
  if (!create_tk (
      ui,
      &ui->tk_app,
      os_get_root_window (&ui->app, IS_WINDOW),
      "NeuralBlender settings",
      0, 0, default_w, default_h,
      ui->mainwindow.native_handle ()))
    return;
  set_min_size_to_current ();

  frame1.create (&tk_root, "", 12, 12, w () - 24, h () - 80);
  btn_ok.create (&tk_root, "OK", 0, 0, 128, 40);
  btn_ok.set_image_default (data_xputty_approved_png);
  btn_cancel.create (&tk_root, "Cancel", 0, 0, 128, 40);
  btn_cancel.set_image_default (data_xputty_cancel_png);

  label_vuscale.create (&frame1, "VU meter scale dB:", 16, 32, 180, 32);
  label_vuscale.align = TEXT_LEFT;
  label_vuheadroom.create (&frame1, "VU meter headroom dB:", 16, 72, 180, 32);
  label_vuheadroom.align = TEXT_LEFT;
  label_spacer1.create (&frame1, "", 16, 112, 12, 12);

  btn_bypass_doubleclick.create (
    &frame1, "Toggle bypass on click again", 16, 152, 320, 32);
  btn_bypass_doubleclick.align = TEXT_LEFT;
  btn_bypass_rightclick.create (
    &frame1, "Toggle bypass on right click", 16, 192, 320, 32);
  btn_bypass_rightclick.align = TEXT_LEFT;
  btn_defaults.create (&frame1, "Reset to defaults", 12, 0, 400, 40);

  text_vuscale.create (&frame1, "", 220, 28, 120, 36);
  text_vuheadroom.create (&frame1, "", 220, 68, 120, 36);
  
  on_resize ();
}

void c_prefswindow::on_resize () {
  c_tktoplevelwindow::on_resize ();
  frame1.move_resize (12, 12, w () - 24, h () - 80);
  
  // bottom about/ok/cancel buttons
  btn_ok.move_resize (w () - 140, h () - 56, 128, 40);
  btn_cancel.move_resize (w () - 280, h () - 56, 128, 40);
  btn_defaults.move_resize (12, frame1.h - 50, frame1.w - 24, 40);
}

void c_prefswindow::on_tk_action (nbtk::t_action_event &event) {
  if (event.mouse_button != Button1)
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
  
  c_toplevelwindow::show ();
}

void c_prefswindow::hide () { CP
  c_toplevelwindow::hide ();
}

void c_prefswindow::load_defaults () {
  text_vuscale.set_text ("-48");
  text_vuheadroom.set_text ("6");
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
}

////////////////////////////////////////////////////////////////////////////////
// c_tkaboutwindow

static const char *g_about_text [] = {
#ifdef LV2
  "An amp modeling plugin based on",
#else
  "An amp modeling app based on",
#endif
  "RTNeural and NeuralAmpModeler",
  "",
  "by Deimos Laboratories",
  "", // web link, see below
  NULL
};

void c_tkaboutwindow::create (c_neuralblender_ui *ui_) { CP
  ui = ui_;
  if (!ui || !ui->ui_ready || widget)
    return;

  if (!create_tk (
      ui,
      &ui->tk_app,
      os_get_root_window (&ui->app, IS_WINDOW),
      "About NeuralBlender (tk)",
      470, 0, 450, 480,
      ui->mainwindow.native_handle ()))
    return;

  set_min_size_to_current ();
  nbtk::c_widget *root = &tk_root;
  const int panel_x = 12;
  const int panel_y = 12;
  const int panel_w = 426;

  tk_frame.create (root, "", panel_x, panel_y, panel_w, 388);

  tk_toplogo.create (&tk_frame, "", 85, 12, 256, 32);
  tk_toplogo.set_png (data_textlogo_1024x128_png);

  tk_logo.create (&tk_frame, "", 133, 64, 160, 160);
  tk_logo.set_png (data_neuralblender_logo_160_png);

  for (int i = 0; g_about_text [i]; i++) {
    tk_labels [i].create (
      &tk_frame, g_about_text [i], 0, 240 + i * 24, panel_w, 24);
    tk_labels [i].size = 13.0f;
  }

  tk_link.create (
    &tk_frame, "http://deimos.ca/neuralblender", 0, 336, panel_w, 24);
  tk_link.size = 13.0f;
  tk_link.link = true;

  char buf [64];
  snprintf (buf, sizeof (buf), "Build timestamp: %s", g_build_timestamp);
  tk_build.create (&tk_frame, buf, 0, 360, panel_w, 20);
  tk_build.size = 10.0f;

  tk_ok.create (root, "OK", 310, 424, 128, 40);
  tk_ok.set_image_default (data_xputty_approved_png);
  
  test_knob.create (root, "test knob", 16, 400, 64, 64);
  test_listbox.create (root, "test listbox", 100, 400, 200, 80);
  test_scrollbar.create (root, "", 300, 400, UI_SCROLLBAR_WIDTH, 80);
  test_listbox.set_scrollbar (&test_scrollbar);
  for (int i = 0; i < 10; i++) {
    char buf [32];
    snprintf (buf, 31, "Item %d", i + 1);
    test_listbox.add (buf);
  }
}

void c_tkaboutwindow::show () { CP
  if (!widget)
    create (ui);
  if (!widget)
    return;

  widget_show_all (widget);
  expose_widget (widget);
}

void c_tkaboutwindow::hide () { CP
  if (!widget)
    return;

  widget_hide (widget);
}

void c_tkaboutwindow::on_tk_action (nbtk::t_action_event &event) {
  if (event.source_id == tk_ok.id && event.mouse_button == Button1) {
    hide ();
    event.handled = true;
  }
}

////////////////////////////////////////////////////////////////////////////////
// c_lane_widgets

void c_lane_widgets::on_tk_action (nbtk::t_action_event &event) {
  if (!ui || ui->updating_from_state)
    return;

  proxy.ui = ui;
  proxy.lane = lane_id;
  proxy.bank = bank_id;

  c_neuralblender_lane_state *lane_state =
    bank_id < BANK_COUNT && lane_id < NB_NUM_MODELS ?
      &ui->state.banks [bank_id].lanes [lane_id] : NULL;

  auto handle_knob = [&] (nbtk::c_knob &knob, _widget_role role) {
    if (event.source_id != knob.id)
      return false;

    proxy.role = role;
    const float value = knob.value;
    const float g = db_to_gain (value);
    switch (role) {
      case ROLE_GAIN_IN:
        if (lane_state)
          lane_state->gain_in = g;
        ui->on_gain_in (&proxy, g);
      break;

      case ROLE_IR_PITCH:
        if (lane_state)
          lane_state->ir_pitch_semitones = value;
        ui->on_ir_pitch (&proxy, value);
      break;

      case ROLE_GAIN_OUT:
        if (lane_state)
          lane_state->gain_out = g;
        ui->on_gain_out (&proxy, g);
      break;

      case ROLE_DRY_OUT:
        if (lane_state)
          lane_state->dry_out = value <= DB_SILENCE ? 0.0f : g;
        ui->on_dry_out (&proxy, g);
      break;

      case ROLE_DELAY:
        if (lane_state)
          lane_state->delay_ms = value;
        ui->on_delay (&proxy, value);
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
    proxy.role = role;
    const bool value = button.value;

    switch (role) {
      case ROLE_MUTE:
        ui->on_mute (&proxy, value);
        if (lane_state)
          lane_state->lane_mute = value;
      break;

      case ROLE_EXCL_USE:
        ui->on_excl_use (&proxy, value);
        ui->sync_widgets_from_state (ui->state);
      break;

      case ROLE_BROWSE:
        ui->on_filebrowse (&proxy);
        filepicker.show ();
      break;

      case ROLE_CLEAR:
        ui->on_fileclear (&proxy);
      break;

      case ROLE_DCFLIP:
        if (lane_state)
          lane_state->dcflip = value;
        ui->on_dcflip (&proxy, value);
      break;

      case ROLE_CALIBRATE:
        if (lane_state)
          lane_state->do_calib = value;
        ui->on_calibrate (&proxy, value);
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
  lane_frame.create (&lane_root, label, 0, 0, std::max (1, w), std::max (1, h));
  lane_frame.state = lane_state;
  created = true;
  main_widget = native_owner;
  
  // regular controls
  menu_list.create (&lane_frame, label, 0, 0, 320, 32);

  int knobs_right = w - 180;
  knob_gain_in.create (&lane_frame, "Input", 0, 0, 64, 64);
  knob_ir_pitch.create (&lane_frame, "Pitch", 0, 0, 64, 64);
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
  
  knob_gain_out.create (&lane_frame, "Output", 0, 0, 64, 64);
  knob_gain_out.set_min (-40);
  knob_gain_out.set_max (40);
  knob_gain_out.set_default (0);
  knob_gain_out.set_value (0);
  knob_gain_out.set_step (0.1);
  
  knob_dry_out.create (&lane_frame, "Dry out", 0, 0, 64, 64);
  knob_dry_out.set_min (-120);
  knob_dry_out.set_max (12);
  knob_dry_out.set_default (-120);
  knob_dry_out.set_value (-120);
  knob_dry_out.set_step (0.1);
  
  meter_out.create (&lane_root, "", 0, 0, METER_WIDTH, 120);
  meter_out.set_vudata (&vudata_out);
  meter_out.set_stereo (false);
  vudata_out.set_l (0.0, 0.0);
  
  btn_browse.create (&lane_frame, "", 0, 0, 100, 40);
  btn_clear.create  (&lane_frame, "", 0, 0, 100, 40);
  btn_excl.create   (&lane_frame, "Use", 0, 0, 100, 40);
  btn_mute.create   (&lane_frame, "Mute", 0, 0, 100, 40);
  btn_excl.is_toggle = true;
  btn_mute.is_toggle = true;
  switch (bank_id) {
    case BANK_PEDAL:
    break;
    case BANK_AMP:
    break;
    case BANK_CAB:
    break;
  }
  btn_mute.set_value (false);
  btn_mute.set_image (data_icon_speaker_off_big_png, WSTATE_ON);
  btn_mute.set_image (data_icon_speaker_on_big_png, WSTATE_OFF);
  btn_excl.set_image (data_icon_radiobutton_on_png, WSTATE_ON);
  btn_excl.set_image (data_icon_radiobutton_off_png, WSTATE_OFF);
  
  // advanced controls
  knob_delay.create (&lane_frame, "Delay", 0, 0, 64, 64);
  knob_delay.set_min (0);
  knob_delay.set_max (30);
  knob_delay.set_default (0);
  knob_delay.set_value (0);
  knob_delay.set_step (0.01);
  //delay.set_tooltip ("Micro delay applied to this amp's output");

  btn_flip.create   (&lane_frame, "", 0, 0, 32, 32);
  btn_calib.create  (&lane_frame, "", 0, 0, 32, 32);
  btn_flip.is_toggle = true;
  btn_calib.is_toggle = true;
  if (ui && lane_id < NB_NUM_MODELS && bank_id < BANK_COUNT)
    btn_calib.set_value (ui->state.banks [bank_id].lanes [lane_id].do_calib);
  //label_flip.create (ui, wp, "DC flip", 0, 0, 75, 32);
  //label_calib.create (ui, wp, "Calib.", 0, 0, 75, 32);
  label_frames.create (&lane_frame, "(not loaded)", 0, 0, 75, 24);
  label_frames.size = 10.5f;
  label_frames.align = TEXT_CENTER;
  label_trim.create (&lane_frame, "1.0", 0, 0, 75, 24);
  label_trim.size = 10.5f;
  label_trim.align = TEXT_CENTER;
  label_engine.create (&lane_frame, "(none)", 0, 0, 120, 24);
  label_engine.size = 10.5f;
  label_engine.align = TEXT_CENTER;
  
  btn_browse.set_image_default (data_icon_folder_big_png);
  btn_clear.set_image_default (data_icon_x_big_png);
  btn_calib.set_image_default (data_icon_calib_big_png);
  btn_flip.set_image_default (data_icon_phase_big_png);
  
  if (ui && lane_id < NB_NUM_MODELS) {
    Window root = os_get_root_window (&ui->app, IS_WINDOW);
    filepicker.create (
      ui, &ui->tk_app, root, native_owner, lane_id, bank_id, "Select file");
    filepicker.lane = lane_id;
    filepicker.bank = bank_id;
  }
  
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

  const int meter_x = w + 6;
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
  menu_list.move_resize (menu_x, 16, menu_width, 32);
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
  label_frames.move_resize (knob_delay.x, h - 20, knob_delay.w, 16);
  label_engine.move_resize (knob_gain_in.x, h - 20, knob_gain_in.w, 16);
  label_trim.move_resize (knob_dry_out.x, h - 20, knob_dry_out.w +
                          knob_dry_out.w, 16);
                          
  //move_resize (x, y, w, h);
}

void c_lane_widgets::set_state (_widget_state state) {
  if (lane_state == state)
    return;

  lane_state = state;
  lane_frame.state = lane_state;
  lane_frame.invalidate ();
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
  configfile.set_item (CONFIG_KEY_NAME_MODEL_CWD, path_dirname (path));
}

void c_neuralblender_ui::update_ir_cwd (std::string path) {
  CP
  debug ("path='%s'", path.c_str ());
  configfile.set_item (CONFIG_KEY_NAME_IR_CWD, path_dirname (path));
}

bool c_neuralblender_ui::create (Window parent_) { CP
  size_t i;
  destroy ();
  
  main_init (&app);
  app.small_font = 12 * app.hdpi;
  app.normal_font = 14 * app.hdpi;
  app.big_font = 20 * app.hdpi;
  display = app.dpy;
  
  configfile.read_file ();
  read_prefs_from_config (configfile, prefs);
  
  if (configfile.istrue (CONFIG_KEY_NAME_ADV)) {
    CP
    state.showadvanced = true;
  }

  if (configfile.istrue (CONFIG_KEY_NAME_CALIB)) {
    calib_default = true;
    for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank)
      for (i = 0; i < NB_NUM_MODELS; ++i)
        state.banks [bank].lanes [i].do_calib = true;
  }

  parent = parent_;
  if (!parent)
    parent = DefaultRootWindow (display);

  int initial_w = DEFAULT_WINDOW_WIDTH;
  int initial_h = DEFAULT_WINDOW_HEIGHT;
  if (parent_ &&
      get_parent_window_size (
        display, parent, app.hdpi, &initial_w, &initial_h)) {
    debug ("initial parent window size %d,%d", initial_w, initial_h);
  }
    
  if (!mainwindow.create (this, parent, "NeuralBlender", 0, 0, 
                          initial_w, initial_h)) {
    fprintf (stderr, "Cant' create main window!\n");
    return false;
  }
  
  cont_toparea.create (&mainwindow.tk_root, "", 0, 0, 640, 50);
  cont_pedals.create (&mainwindow.tk_root, "", 0, 120, 640, 480);
  cont_models.create (&mainwindow.tk_root, "", 0, 120, 640, 480);
  cont_cabs.create (&mainwindow.tk_root, "", 0, 120, 640, 480);
  cont_other.create (&mainwindow.tk_root, "", 0, 120, 640, 480);
  
  mainwindow.set_icon_from_png (data_neuralblender_logo_512_png);

  //label_big.create (this, mainwindow.widget, "NeuralBlender", 120, 24, 400, 40);
  //label_big.align = TEXT_CENTER;
  //label_big.textsize = 1.5;
  img_logo.create (&cont_toparea, "", 0, 0, 256, 32);
  img_logo.set_png (data_textlogo_1024x128_png);
  
  const int tabbutton_padding = 14;
  btn_tab_pedals.create (&cont_toparea, "PDL", 0, 0, 84, 50);
  btn_tab_pedals.set_image_default (data_icon_power_on_png);
  btn_tab_pedals.padding = tabbutton_padding;
  btn_tab_models.create (&cont_toparea, "AMP", 0, 0, 84, 50);
  btn_tab_models.set_image_default (data_icon_power_on_png);
  btn_tab_models.padding = tabbutton_padding;
  btn_tab_cabs.create (&cont_toparea, "CAB", 0, 0, 84, 50);
  btn_tab_cabs.set_image_default (data_icon_power_on_png);
  btn_tab_cabs.padding = tabbutton_padding;
  btn_tab_other.create (&cont_toparea, "...", 0, 0, 84, 50);
  
  btn_enable.create (&cont_toparea, "",  20, 12, 40, 40);
  btn_enable.is_toggle = true;
  btn_enable.set_value (true);
  btn_enable.set_image (data_icon_power_on_png, WSTATE_ON);
  btn_enable.set_image (data_icon_power_grey_png, WSTATE_OFF);
  
  //btn_muteall.create (this, mainwindow.widget, "Mute all", 500, 12, 120, 40, WSTYLE_IMAGE_TOGGLE);
  btn_muteall.create (&cont_toparea, "", 500, 12, 40, 40);
  btn_muteall.is_toggle = true;
  btn_muteall.set_image (data_icon_speaker_off_big_png, WSTATE_ON);
  btn_muteall.set_image (data_icon_speaker_on_big_png, WSTATE_OFF);
  
  btn_noisegate.create (&cont_toparea, "", 0, 0, 40, 40);
  btn_noisegate.is_toggle = true;
  btn_noisegate.set_image_default (data_icon_noisegate_png);
  btn_tuner.create (&cont_toparea, "", 0, 0, 40, 40);
  btn_tuner.is_toggle = true;
  btn_tuner.set_image_default (data_icon_tuner_png);
  
  btn_enable.padding =    12;
  btn_muteall.padding =   12;
  btn_tuner.padding =     12;
  btn_noisegate.padding = 12;
  
  tkaboutwindow.create (this);
  prefswindow.create (this);
  mainwindow.activate_tk ();
  
  for (i = 0; i < NB_NUM_MODELS; i++) {
    lanes_pedals [i].create (
    this, &cont_pedals, mainwindow.native_handle (), BANK_PEDAL, i, 0, 0, 1, 1);
    lanes_models [i].create (
    this, &cont_models, mainwindow.native_handle (), BANK_AMP, i, 0, 0, 1, 1);
    lanes_cabs [i].create (
    this, &cont_cabs, mainwindow.native_handle (), BANK_CAB, i, 0, 0, 1, 1);
  }
  meter_in [PAGE_PEDAL].create (&cont_pedals, "", 6, 70, 5, 520);
  meter_in [PAGE_AMP].create (&cont_models, "", 6, 70, 5, 520);
  meter_in [PAGE_CAB].create (&cont_cabs, "", 6, 70, 5, 520);
  meter_in [PAGE_OTHER].create (&cont_other, "", 6, 70, 5, 520);
  meter_masterout.create (&cont_other, "", 6, 70, 5, 520);

  for (i = 0; i < BANK_COUNT; i++) {
    meter_in [i].set_vudata (&vudata_in [i]);
    meter_in [i].set_stereo (false);
    vudata_in [i].set_l (0.0, 0.0);
  }
  meter_in [PAGE_OTHER].set_vudata (&vudata_masterin);
  meter_in [PAGE_OTHER].set_stereo (false);
  vudata_masterin.set_l (0.0, 0.0);
  meter_masterout.set_vudata (&vudata_masterout);
  meter_masterout.set_stereo (false);
  vudata_masterout.set_l (0.0, 0.0);
  
  frame_other_volumepresence.create (&cont_other, "", 16, 16, 512, 128);
  knob_mastervolume.create (&frame_other_volumepresence, "Master out", 16, 12, 80, 96);
  knob_presence.create (&frame_other_volumepresence, "Presence", 116, 12, 80, 96);
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
  
  frame_other_noisegate.create (&cont_other, "", 16, 16, 512, 128);
  label_other_noisegate.create (&frame_other_noisegate, "Noise gate:", 16, 8, 200, 24);
  label_other_noisegate.align = TEXT_LEFT;
  knob_noisethresh.create (&frame_other_noisegate,  "Thresh",   16, 36, 64, 72);
  knob_noiseattack.create (&frame_other_noisegate,  "Attack",   76, 36, 64, 72);
  knob_noisehold.create (&frame_other_noisegate,    "Hold",    136, 36, 64, 72);
  knob_noiserelease.create (&frame_other_noisegate, "Release", 196, 36, 64, 72);
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
  const int x0 = 16, x1 = 186, x2 = 296, x3 = 406;
  const int y0 = 16, y1 = 56, y2 = 96;
  label_other_byp.create (&frame_other_linkexcl, "Bypass: ",  x0, y0, 150, 32);
  label_other_link.create (&frame_other_linkexcl, "Link calibration: ", x0, y1, 150, 32);
  label_other_excl.create (&frame_other_linkexcl, "Exclusive mode: ",   x0, y2, 150, 32);
  label_other_byp.align = TEXT_LEFT;
  label_other_link.align = TEXT_LEFT;
  label_other_excl.align = TEXT_LEFT;
  btn_other_byp_pedal.create (&frame_other_linkexcl, "Pedal",  x1, y0, 150, 32);
  btn_other_byp_amp.create (&frame_other_linkexcl, "Amp",      x2, y0, 150, 32);
  btn_other_byp_cab.create (&frame_other_linkexcl, "Cab/IR",   x3, y0, 150, 32);
  btn_other_link_pedal.create (&frame_other_linkexcl, "Pedal", x1, y1, 150, 32);
  btn_other_link_amp.create (&frame_other_linkexcl, "Amp",     x2, y1, 150, 32);
  btn_other_link_cab.create (&frame_other_linkexcl, "Cab/IR",  x3, y1, 150, 32);
  btn_other_excl_pedal.create (&frame_other_linkexcl, "Pedal", x1, y2, 150, 32);
  btn_other_excl_amp.create (&frame_other_linkexcl, "Amp",     x2, y2, 150, 32);
  btn_other_excl_cab.create (&frame_other_linkexcl, "Cab/IR",  x3, y2, 150, 32);
  
  frame_other_misc.create (&cont_other, "", 16, 16, 512, 128);
  label_other_tuner.create (&frame_other_misc, "Tuner base frequency: ", 16, 20, 200, 32);
  label_other_calib.create (&frame_other_misc, "Calibration target dB: ", 16, 60, 200, 32);
  label_other_tuner.align = TEXT_LEFT;
  label_other_calib.align = TEXT_LEFT;
  btn_other_vu.create (&frame_other_misc, "VU meters", 16, 100, 200, 32);
  btn_other_bass.create (&frame_other_misc, "Calibrate for bass", 16, 140, 200, 32);
  text_other_tuner.create (&frame_other_misc, "", 250, 14, 150, 40);
  text_other_calib.create (&frame_other_misc, "", 250, 60, 150, 40);
  btn_other_prefs.create (&frame_other_misc, "Settings", 0, 130, 120, 40);
  btn_other_prefs.set_image_default (data_xputty_gear_png);
  btn_other_about.create (&frame_other_misc, "About...", 0, 130, 120, 40);
  btn_other_about.set_image_default (data_xputty_info_png);
  
  btn_other_tuner_down.create (&frame_other_misc, "", 420, 14, 40, 40);
  btn_other_tuner_down.set_image_default (data_icon_flat_png);
  btn_other_tuner_up.create (&frame_other_misc, "", 468, 14, 40, 40);
  btn_other_tuner_up.set_image_default (data_icon_sharp_png);
  btn_other_tuner_default.create (&frame_other_misc, "", 516, 14, 40, 40);
  btn_other_tuner_default.set_image_default (data_icon_tuner_png);
  
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
    blender->banks [BANK_AMP].meter_in   = &vudata_in [BANK_AMP];
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
  
  ui_ready = true;
  move_resize ();
  mainwindow.show ();
  sync_tuner_visibility ();
  CP
  //XFlush (display);
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
    cont_models.move_resize (0, page_y, window_width, page_h);
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
          int btnw = std::min (88, btnh * 2);
              btnw = std::max (btnw, window_width / 8);
    if (btnh > window_width / 12)
      btnh = window_width / 12;
    const int btnr = window_width - 12;
    const int btnt = toparea - btnh; //toparea / 2 + 8;
    
    btn_tab_pedals.move_resize  (btnl,                  btnt, btnw, btnh);
    btn_tab_models.move_resize  (btnl + (btnw + 4),     btnt, btnw, btnh);
    btn_tab_cabs.move_resize    (btnl + (btnw + 4) * 2, btnt, btnw, btnh);
    btn_tab_other.move_resize   (btnl + (btnw + 4) * 3, btnt, btnw, btnh);
    btn_enable.move_resize      (btnr - (btnh + 4) * 4, btnt, btnh, btnh);
    btn_muteall.move_resize     (btnr - (btnh + 4) * 3, btnt, btnh, btnh);
    btn_tuner.move_resize       (btnr - (btnh + 4) * 2, btnt, btnh, btnh);
    btn_noisegate.move_resize   (btnr - (btnh + 4)    , btnt, btnh, btnh);
    
    int btnpad = btn_tab_pedals.h / 4;
    btn_tab_pedals.padding = btnpad;
    btn_tab_models.padding = btnpad;
    btn_tab_cabs.padding = btnpad;
    
    tuner_height = toparea - btnh - 8;
    img_logo.move_resize (window_width / 2 - 128, tuner_height / 2- 16, 256, 32);
    cont_toparea.invalidate ();
    if (tuner.created)
      tuner.move_resize (4, 4, window_width - 8, tuner_height);
    
    const int lane_bottom = lane_top + lane_count * lane_height + total_gap;
    const int panelwidth = window_width - 32;
    const int meter_h = std::max (1, page_h - 10);
    const int meter_top = lane_top + 4;
      
    if (page_has_bank (visible_page)) {
      size_t i;
      c_lane_widgets *bank_lanes = lanes_for_bank (visible_bank);
      for (i = 0; i < NB_NUM_MODELS; i++) {
        bank_lanes [i].move_resize (
          16, lane_top + i * (lane_height + lane_gap), lane_width, lane_height);
      }
      
      meter_in [visible_bank].move_resize (
        5, meter_top, 5, meter_h /*std::max (1, lane_bottom - lane_top - 8)*/);
    } else {
      meter_in [PAGE_OTHER].move_resize (5, meter_top, METER_WIDTH, meter_h);
      meter_masterout.move_resize (
        window_width - 5 - METER_WIDTH, meter_top, METER_WIDTH, meter_h);
      frame_other_volumepresence.move_resize (16, 0, panelwidth / 2 - 12, 120);
      frame_other_noisegate.move_resize (panelwidth / 2 + 16, 0, panelwidth / 2, 120);
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
    main_quit (&app);

  memset (&app, 0, sizeof (app));
  display = NULL;
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
  } else {
    redraw_meter (meter_in [visible_bank]);
    c_lane_widgets *bank_lanes = lanes_for_bank (visible_bank);
    for (int i = 0; i < NB_NUM_MODELS; i++)
      redraw_meter (bank_lanes [i].meter_out);
  }
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

void c_neuralblender_ui::on_bank_switch (c_widget *w, int n) { CP
  (void) w;
  if (n >= PAGE_PEDAL && n < PAGE_COUNT) {
    visible_page = (_ui_page) n;
    if (page_has_bank (visible_page))
      visible_bank = bank_for_page (visible_page);
  }

  sync_widgets_from_state (state);
  move_resize ();
}

void c_neuralblender_ui::sync_page_visibility () {
  switch (visible_page) {
    case PAGE_PEDAL:
      cont_pedals.show ();
      cont_models.hide ();
      cont_cabs.hide ();
      cont_other.hide ();
      btn_tab_pedals.set_value (true);
      btn_tab_models.set_value (false);
      btn_tab_cabs.set_value (false);
      btn_tab_other.set_value (false);
    break;
    
    case PAGE_CAB:
      cont_pedals.hide ();
      cont_models.hide ();
      cont_cabs.show ();
      cont_other.hide ();
      btn_tab_pedals.set_value (false);
      btn_tab_models.set_value (false);
      btn_tab_cabs.set_value (true);
      btn_tab_other.set_value (false);
    break;

    case PAGE_OTHER:
      cont_pedals.hide ();
      cont_models.hide ();
      cont_cabs.hide ();
      cont_other.show ();
      btn_tab_pedals.set_value (false);
      btn_tab_models.set_value (false);
      btn_tab_cabs.set_value (false);
      btn_tab_other.set_value (true);
    break;
    
    case PAGE_AMP:
    default:
      cont_pedals.hide ();
      cont_models.show ();
      cont_cabs.hide ();
      cont_other.hide ();
      btn_tab_pedals.set_value (false);
      btn_tab_models.set_value (true);
      btn_tab_cabs.set_value (false);
      btn_tab_other.set_value (false);
    break;
  }

  sync_bank_tab_icon (btn_tab_pedals, state, BANK_PEDAL);
  sync_bank_tab_icon (btn_tab_models, state, BANK_AMP);
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
  return page >= PAGE_PEDAL && page <= PAGE_CAB;
}

static _lane_bank bank_for_page (_ui_page page) {
  switch (page) {
    case PAGE_PEDAL:   return BANK_PEDAL;
    case PAGE_AMP:     return BANK_AMP;
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
    case BANK_CAB:   return state.cab_bypass;
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

    case BANK_CAB:
      state.cab_bypass = bypass;
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

void c_neuralblender_ui::on_tk_action (nbtk::t_action_event &event) {
  if (updating_from_state)
    return;

  for (size_t bank = BANK_PEDAL; bank < BANK_COUNT && !event.handled; ++bank) {
    c_lane_widgets *bank_lanes = lanes_for_bank ((_lane_bank) bank);
    for (size_t i = 0; i < NB_NUM_MODELS && !event.handled; ++i)
      bank_lanes [i].on_tk_action (event);
  }
  if (event.handled)
    return;

  c_widget proxy;
  proxy.ui = this;
  proxy.bank = visible_bank;
  proxy.lane = 0;

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

    proxy.role = ROLE_BANKSWITCH;
    proxy.bank = bank;
    proxy.page = page;
    const bool right_click = event.mouse_button == Button3;
    if (page == PAGE_OTHER) {
      if (visible_page != PAGE_OTHER && right_click) {
        proxy.bank = visible_bank;
        const int exclusive_lane =
          exclusive_lane_for_bank (visible_bank) ? 0 : (int) choose_exclusive_lane ();
        on_excl (&proxy, exclusive_lane);
      } else if (visible_page != PAGE_OTHER) {
        on_bank_switch (&proxy, page);
      }
    } else if (page_has_bank (page) &&
        ((right_click && prefs.bypass_rightclick) ||
        (visible_page == page && prefs.bypass_doubleclick))) {
      const bool bypass = !bank_bypass_for_state (state, bank);
      set_bank_bypass_for_state (state, bank, bypass);
      on_bank_bypass (&proxy, bank, bypass);
    } else if (page_has_bank (page) && right_click) {
      // Right-click bank buttons are reserved for bank bypass when enabled.
    } else {
      on_bank_switch (&proxy, page);
    }
    finish ();
    return true;
  };

  if (handle_bank_switch (btn_tab_pedals, PAGE_PEDAL, BANK_PEDAL) ||
      handle_bank_switch (btn_tab_models, PAGE_AMP, BANK_AMP) ||
      handle_bank_switch (btn_tab_cabs, PAGE_CAB, BANK_CAB) ||
      handle_bank_switch (btn_tab_other, PAGE_OTHER, BANK_AMP))
    return;

  auto handle_button = [&] (nbtk::c_button &button, _widget_role role, uint64_t bank) {
    if (event.source_id != button.id)
      return false;

    proxy.role = role;
    proxy.bank = bank;
    const bool value = button.value;

    switch (role) {
      case ROLE_BYPASS:
        state.bypass = !value;
        on_bypass (&proxy, value);
      break;

      case ROLE_MUTEALL:
        state.mute_all = value;
        on_muteall (&proxy, value);
      break;

      case ROLE_NOISEGATE:
        state.noisegate_on = value;
        on_noisegate (&proxy, value);
      break;

      case ROLE_TUNER:
        state.tuner_on = value;
        on_tuner (&proxy, value);
        sync_tuner_visibility ();
      break;

      case ROLE_BANK_BYPASS:
        if (bank < BANK_COUNT) {
          const _lane_bank b = (_lane_bank) bank;
          set_bank_bypass_for_state (state, b, value);
          on_bank_bypass (&proxy, b, value);
        }
      break;

      case ROLE_LINKED_CALIB:
        if (bank < BANK_COUNT)
          visible_bank = (_lane_bank) bank;
        if (page_has_bank (visible_page) || bank < BANK_COUNT) {
          const _lane_bank b = bank < BANK_COUNT ? (_lane_bank) bank : visible_bank;
          set_linked_calib_for_bank (b, value);
          on_linked_calib (&proxy, value);
        }
      break;

      case ROLE_EXCL_TOGGLE:
        if (bank < BANK_COUNT)
          visible_bank = (_lane_bank) bank;
        if (value) {
          size_t exclusive_lane = choose_exclusive_lane ();
          on_excl (&proxy, exclusive_lane);
        } else {
          on_excl (&proxy, 0);
        }
      break;

      case ROLE_CALIBBASS:
        state.calib_source = value ? 1 : 0;
        on_calib_bass (&proxy, value);
      break;

      case ROLE_VUTOGGLE:
        state.do_vu = value;
        vu_on (value);
        on_vu (&proxy, value);
      break;

      case ROLE_PREFS:
        write_prefs_to (prefs);
        prefswindow.get_prefs_from (prefs);
        prefswindow.show ();
        on_prefs ();
      break;

      case ROLE_ABOUT:
        tkaboutwindow.show ();
        on_about ();
      break;

      case ROLE_TUNER_UP:
        state.tuner_base_freq *= SEMITONE_MULTIPLIER;
        on_tuner_base_freq (&proxy, state.tuner_base_freq);
      break;

      case ROLE_TUNER_DOWN:
        state.tuner_base_freq /= SEMITONE_MULTIPLIER;
        on_tuner_base_freq (&proxy, state.tuner_base_freq);
      break;

      case ROLE_TUNER_DEFAULT:
        state.tuner_base_freq = 440.0f;
        on_tuner_base_freq (&proxy, state.tuner_base_freq);
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
      handle_button (btn_tuner, ROLE_TUNER, visible_bank) ||
      handle_button (btn_other_byp_pedal, ROLE_BANK_BYPASS, BANK_PEDAL) ||
      handle_button (btn_other_byp_amp, ROLE_BANK_BYPASS, BANK_AMP) ||
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

  auto handle_knob = [&] (nbtk::c_knob &knob, _widget_role role) {
    if (event.source_id != knob.id)
      return false;

    proxy.role = role;
    const float value = knob.value;
    switch (role) {
      case ROLE_MASTER:
        state.master_gain = db_to_gain (value);
        on_master_gain (&proxy, state.master_gain);
      break;

      case ROLE_PRESENCE:
        state.presence = value;
        on_presence (&proxy, value);
      break;

      case ROLE_NOISETHRESH:
        state.noisethresh = value;
        on_noisethresh (&proxy, value);
      break;

      case ROLE_NOISEATTACK:
        state.noiseattack = value;
        on_noiseattack (&proxy, value);
      break;

      case ROLE_NOISEHOLD:
        state.noisehold = value;
        on_noisehold (&proxy, value);
      break;

      case ROLE_NOISERELEASE:
        state.noiserelease = value;
        on_noiserelease (&proxy, value);
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
      proxy.role = role;
      if (role == ROLE_TUNER_BASE_FREQ)
        on_tuner_base_freq (&proxy, std::clamp (value, 400.0f, 480.0f));
      else if (role == ROLE_CALIB_TARGET_DB)
        on_calib_target_db (
          &proxy,
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

  for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
    meter_in [bank].set_db_scale (scale_db);
    meter_in [bank].set_headroom (headroom_db);
    vudata_in [bank].set_db_scale (scale_db);
    vudata_in [bank].set_headroom (headroom_db);
  }
  meter_in [PAGE_OTHER].set_db_scale (scale_db);
  meter_in [PAGE_OTHER].set_headroom (headroom_db);
  meter_masterout.set_db_scale (scale_db);
  meter_masterout.set_headroom (headroom_db);
  vudata_masterin.set_db_scale (scale_db);
  vudata_masterin.set_headroom (headroom_db);
  vudata_masterout.set_db_scale (scale_db);
  vudata_masterout.set_headroom (headroom_db);

  for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
    c_lane_widgets *bank_lanes = lanes_for_bank ((_lane_bank) bank);
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
  apply_ui_prefs (p);
}

void c_neuralblender_ui::write_prefs_to (t_prefs &p) { CP
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
  
  for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
    c_lane_widgets *bank_lanes = lanes_for_bank ((_lane_bank) bank);

    for (size_t i = 0; i < NB_NUM_MODELS; i++) {
      const size_t n = i * UI_STATS_PER_LANE;
      int nframes = stats [bank] [n];
      float trim = stats [bank] [n + 1];
      int eng = (int) stats [bank] [n + 2];
      if (eng < ENGINE_NONE || eng > ENGINE_UNKNOWN)
        eng = ENGINE_UNKNOWN;
      
      bank_lanes [i].label_engine.label = engine_names [eng];
      bank_lanes [i].label_engine.invalidate ();
      if (eng == ENGINE_IR) {
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

  for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
    meter_in [bank].show ();
    c_lane_widgets *bank_lanes = lanes_for_bank ((_lane_bank) bank);
    for (size_t i = 0; i < NB_NUM_MODELS; i++)
      bank_lanes [i].meter_out.show ();
  }
  meter_in [PAGE_OTHER].show ();
  meter_masterout.show ();
  //on_vu (&btn_vu, b);
}

void c_neuralblender_ui::vu_off () { CP
  state.do_vu = false;

  for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
    meter_in [bank].hide ();
    c_lane_widgets *bank_lanes = lanes_for_bank ((_lane_bank) bank);
    for (size_t i = 0; i < NB_NUM_MODELS; i++)
      bank_lanes [i].meter_out.hide ();
  }
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

void c_neuralblender_ui::on_excl (c_widget *w, int n) {
  const _lane_bank bank =
    w && w->bank < BANK_COUNT ? (_lane_bank) w->bank : visible_bank;

  debug ("n=%d", n);
  set_exclusive_lane_for_bank (bank, n);
  if (n > 0 && n <= (int) NB_NUM_MODELS)
    last_exclusive_lane [bank] = (size_t) n;
  if (!w)
    return;

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

void c_neuralblender_ui::on_excl_use (c_widget *w, bool b) {
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

  run_embedded (&app);

  redraw_visible_meters ();
  
  if (state.tuner_on) {
    tuner.on_ui_timer ();
  }

  int pending = XPending(app.dpy);
  
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
  for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
    const int n = this->state.banks [bank].exclusive_lane;
    if (n > 0 && n <= (int) NB_NUM_MODELS)
      last_exclusive_lane [bank] = (size_t) n;
  }

  updating_from_state = true;
  
  sync_page_visibility ();
  
  btn_noisegate.set_value (state.noisegate_on);
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

  for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
    c_lane_widgets *bank_lanes = lanes_for_bank ((_lane_bank) bank);
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
          nbtk::c_filepicker &fp = bank_lanes [i].filepicker;
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
  btn_other_byp_amp.set_value (state.amp_bypass);
  btn_other_byp_cab.set_value (state.cab_bypass);
  btn_other_link_pedal.set_value (linked_calib_for_bank (BANK_PEDAL));
  btn_other_link_amp.set_value (linked_calib_for_bank (BANK_AMP));
  btn_other_link_cab.set_value (linked_calib_for_bank (BANK_CAB));
  btn_other_vu.set_value (state.do_vu);
  if (state.do_vu) {
    for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
      meter_in [bank].show ();
      c_lane_widgets *bank_lanes = lanes_for_bank ((_lane_bank) bank);
      for (size_t i = 0; i < NB_NUM_MODELS; ++i)
        bank_lanes [i].meter_out.show ();
    }
    meter_in [PAGE_OTHER].show ();
    meter_masterout.show ();
  } else {
    for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
      meter_in [bank].hide ();
      c_lane_widgets *bank_lanes = lanes_for_bank ((_lane_bank) bank);
      for (size_t i = 0; i < NB_NUM_MODELS; ++i)
        bank_lanes [i].meter_out.hide ();
    }
    meter_in [PAGE_OTHER].hide ();
    meter_masterout.hide ();
  }

  const int visible_exclusive_lane = exclusive_lane_for_bank (visible_bank);
  btn_other_excl_pedal.set_value (exclusive_lane_for_bank (BANK_PEDAL) > 0);
  btn_other_excl_amp.set_value (exclusive_lane_for_bank (BANK_AMP) > 0);
  btn_other_excl_cab.set_value (exclusive_lane_for_bank (BANK_CAB) > 0);

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
      visible_lanes [i].set_state (WSTATE_DISABLED);
    } else {
      visible_lanes [i].set_state (WSTATE_NORMAL);
    }

    if (exclusive_on) { CP
      visible_lanes [i].set_state (WSTATE_DISABLED);
      visible_lanes [i].btn_mute.hide ();
      visible_lanes [i].btn_excl.show ();
    } else { CP
      visible_lanes [i].btn_mute.show ();
      visible_lanes [i].btn_excl.hide ();
    }
  }
  if (exclusive_on && !state.mute_all && enabled && !visible_bank_bypassed) {
    visible_lanes [visible_exclusive_lane - 1].set_state (WSTATE_SELECTED);
  }

  updating_from_state = false;
}
