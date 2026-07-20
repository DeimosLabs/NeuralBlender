
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

#include <X11/cursorfont.h>

#include "neuralblender.h"
#include "ui.h"

#include "data/data.h"

#define CMDLINE_DEBUG_COLOR ANSI_MAGENTA
#include "cmdline_debug.h"

#define MIN_WINDOW_HEIGHT (80 + (130 * NB_NUM_MODELS))
//#define DEFAULT_WINDOW_HEIGHT (12 + std::min (640, (52 + (180 * NB_NUM_MODELS))))
#define DEFAULT_WINDOW_HEIGHT MIN_WINDOW_HEIGHT
#define MIN_WINDOW_WIDTH 620
#define DEFAULT_WINDOW_WIDTH MIN_WINDOW_WIDTH

#define METER_WIDTH 5

extern const char *g_build_timestamp;

void combobox_selected_callback (void *w_, void *user_data);
static bool page_has_bank (_ui_page page);
static _lane_bank bank_for_page (_ui_page page);
static bool bank_bypass_for_state (
    const c_neuralblender_state &state,
    _lane_bank bank);
static void sync_bank_tab_icon (
    c_button &button,
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
  float calib_target_db = prefs.calib_target_db;
  if (parse_config_float (
      configfile.get_item (CONFIG_KEY_NAME_CALIB_TARGET),
      calib_target_db))
    prefs.calib_target_db = calib_target_db;

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

  std::string str;
  
  str = configfile.get_item (CONFIG_KEY_NAME_VU);
  if (!str.empty ())
    prefs.vu_on = c_configfile::istrue (str);
  
  str = configfile.get_item (CONFIG_KEY_NAME_TUNER);
  if (!str.empty ())
    prefs.tuner_on = c_configfile::istrue (str);

  float tuner_base_freq = prefs.tuner_base_freq;
  if (parse_config_float (
      configfile.get_item (CONFIG_KEY_NAME_TUNER_BASE),
      tuner_base_freq))
    prefs.tuner_base_freq = std::clamp (tuner_base_freq, 400.0f, 480.0f);

  str = configfile.get_item (CONFIG_KEY_NAME_NOISEGATE);
  if (!str.empty ())
    prefs.noisegate_on = c_configfile::istrue (str);

  float noisethresh = prefs.noisethresh;
  if (parse_config_float (
      configfile.get_item (CONFIG_KEY_NAME_NOISETHRESH),
      noisethresh))
    prefs.noisethresh =
      std::clamp (noisethresh, NOISEGATE_THRESH_MIN, NOISEGATE_THRESH_MAX);

  float noiseattack = prefs.noiseattack;
  if (parse_config_float (
      configfile.get_item (CONFIG_KEY_NAME_NOISEATTACK),
      noiseattack))
    prefs.noiseattack = std::max (0.0f, noiseattack);

  float noisehold = prefs.noisehold;
  if (parse_config_float (
      configfile.get_item (CONFIG_KEY_NAME_NOISEHOLD),
      noisehold))
    prefs.noisehold = std::max (0.0f, noisehold);

  float noiserelease = prefs.noiserelease;
  if (parse_config_float (
      configfile.get_item (CONFIG_KEY_NAME_NOISERELEASE),
      noiserelease))
    prefs.noiserelease = std::max (0.0f, noiserelease);

  return true;
}

bool write_prefs_to_config (c_configfile &configfile, const t_prefs &prefs) {
  char buf [128];
  snprintf (buf, sizeof (buf), "%.6g", prefs.calib_target_db);
  configfile.set_item (CONFIG_KEY_NAME_CALIB_TARGET, buf);

  snprintf (buf, sizeof (buf), "%.6g", prefs.vu_scale_db);
  configfile.set_item (CONFIG_KEY_NAME_VU_SCALE, buf);

  snprintf (buf, sizeof (buf), "%.6g", prefs.vu_headroom_db);
  configfile.set_item (CONFIG_KEY_NAME_VU_HEADROOM, buf);

  configfile.set_item (CONFIG_KEY_NAME_VU, prefs.vu_on ? "1" : "0");
  configfile.set_item (CONFIG_KEY_NAME_TUNER, prefs.tuner_on ? "1" : "0");
  snprintf (buf, sizeof (buf), "%.6g", prefs.tuner_base_freq);
  configfile.set_item (CONFIG_KEY_NAME_TUNER_BASE, buf);

  configfile.set_item (
    CONFIG_KEY_NAME_NOISEGATE,
    prefs.noisegate_on ? "1" : "0");

  snprintf (buf, sizeof (buf), "%.6g", prefs.noisethresh);
  configfile.set_item (CONFIG_KEY_NAME_NOISETHRESH, buf);

  snprintf (buf, sizeof (buf), "%.6g", prefs.noiseattack);
  configfile.set_item (CONFIG_KEY_NAME_NOISEATTACK, buf);

  snprintf (buf, sizeof (buf), "%.6g", prefs.noisehold);
  configfile.set_item (CONFIG_KEY_NAME_NOISEHOLD, buf);

  snprintf (buf, sizeof (buf), "%.6g", prefs.noiserelease);
  configfile.set_item (CONFIG_KEY_NAME_NOISERELEASE, buf);

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
  
  if (!c_toplevelwindow::create (
      ui,
      os_get_root_window (&ui->app, IS_WINDOW),
      "NeuralBlender settings",
      0, 0, default_w, default_h,
      ui->mainwindow.widget))
    return;
  set_min_size_to_current ();
  
  frame1.create (ui, widget, "", 12, 12, w () - 24, h () - 80);
  btn_ok.create (ui, widget, "OK", 0, 0, 128, 40);
  btn_ok.role = ROLE_PREFSOK;
  btn_ok.set_image (data_xputty_approved_png);
  btn_cancel.create (ui, widget, "Cancel", 0, 0, 128, 40);
  btn_cancel.role = ROLE_PREFSCANCEL;
  btn_cancel.set_image (data_xputty_cancel_png);
  label_calibdb.create (ui, frame1.widget, "Calibration target dB:", 0, 0, 120, 32);
  label_vuscale.create (ui, frame1.widget, "VU meter scale dB:", 0, 0, 120, 32);
  label_vuheadroom.create (ui, frame1.widget, "VU meter headroom dB:", 0, 0, 120, 32);
  label_spacer1.create (ui, frame1.widget, "", 0, 0, 12, 12);

  btn_defaults.create (ui, frame1.widget, "Reset to defaults", 0, 0, 400, 32);
  btn_defaults.role = ROLE_PREFSDEFAULTS;
  
  text_calibdb.create (ui, frame1.widget, "", 0, 0, 128, 32);
  text_vuscale.create (ui, frame1.widget, "", 0, 0, 128, 32);
  text_vuheadroom.create (ui, frame1.widget, "", 0, 0, 128, 32);
  
  on_resize ();
}

void c_prefswindow::on_resize () {
  frame1.move ((w () - frame1.w ()) / 2, (h () - frame1.h ()) / 2 - 24);
  
  // bottom about/ok/cancel buttons
  btn_ok.move_resize (w () - 140, h () - 56, 128, 40);
  btn_cancel.move_resize (w () - 280, h () - 56, 128, 40);
  btn_defaults.move_resize (12, frame1.h () - 50, frame1.w () - 24, 40);
  
  int labels_x = 16;
  int controls_x = 0;
  
  std::vector<c_widget *> leftcontrols = {
    &label_calibdb,
    &label_vuscale,
    &label_vuheadroom,
    &label_spacer1
  };
  for (int i = 0; i < leftcontrols.size (); i++) {
    int w = 0;
    leftcontrols [i]->shrinkwrap ();
    leftcontrols [i]->get_label_size (&w, NULL);
    debug ("w=%d", w);
    if (w > controls_x)
      controls_x = w;
    
    leftcontrols [i]->move (labels_x, 32 + i * 40);
  }
  controls_x += 36;
  
  debug ("controls_x=%d, label_calibdb: %d", controls_x, label_calibdb.y ());
  text_calibdb.move_resize    (controls_x, label_calibdb.y () - 4, 120, 36);
  text_vuscale.move_resize    (controls_x, label_vuscale.y () - 4, 120, 36);
  text_vuheadroom.move_resize (controls_x, label_vuheadroom.y () - 4, 120, 36);
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
  text_calibdb.set_text ("-18");
  text_vuscale.set_text ("-48");
  text_vuheadroom.set_text ("6");
}

void c_prefswindow::get_prefs_from (t_prefs &prefs) { CP
  if (!widget)
    create (ui);

  char buf [128];
  format_db_text (buf, sizeof (buf), prefs.calib_target_db);
  text_calibdb.set_text (buf);

  format_db_text (buf, sizeof (buf), prefs.vu_scale_db);
  text_vuscale.set_text (buf);

  format_db_text (buf, sizeof (buf), prefs.vu_headroom_db);
  text_vuheadroom.set_text (buf);

}

void c_prefswindow::set_prefs_to (t_prefs &prefs) {
  prefs.calib_target_db = std::strtof (text_calibdb.value.c_str (), NULL);

  float vu_scale_db = 0.0f;
  if (parse_config_float (text_vuscale.value, vu_scale_db) &&
      vu_scale_db <= 0.0f)
    prefs.vu_scale_db = vu_scale_db;

  float vu_headroom_db = 0.0f;
  if (parse_config_float (text_vuheadroom.value, vu_headroom_db) &&
      vu_headroom_db >= 0.0f &&
      vu_headroom_db <= 12.0f)
    prefs.vu_headroom_db = vu_headroom_db;

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
  "", // web link, see below
  NULL
};
  
void c_aboutwindow::create (c_neuralblender_ui *ui_) { CP
  ui = ui_;
  if (!ui || !ui->ui_ready || widget)
    return;
  
  if (!c_toplevelwindow::create (
      ui,
      os_get_root_window (&ui->app, IS_WINDOW),
      "About NeuralBlender",
      0, 0, 450, 480,
      ui->mainwindow.widget))
    return;

  //widget = widget;
  set_min_size_to_current ();
  
  frame.create (ui, widget, "", 12, 12, w () - 24, h () - 80);
  
  img_toplogo.create (ui, frame.widget, "", 0, 8, 256, 32);
  img_toplogo.set_png (data_textlogo_1024x128_png);
  
  //btn_ok.create (ui, frame.widget, "OK", 160, 395, 80, 40);
  btn_ok.create (ui, widget, "OK", 0, 0, 120, 40);
  btn_ok.role = ROLE_ABOUTOK;
  btn_ok.set_image (data_xputty_approved_png);
  
  int i;
  for (i = 0; g_about_text [i]; i++) {
    int h = 240 + (i * 24);
    labels [i].create (ui, frame.widget, g_about_text [i], 0, h, 400, 24);
    labels [i].align = TEXT_CENTER;
  }
  
  linklabel.create (ui, frame.widget, "http://deimos.ca/neuralblender",
      0, labels [4].y (), 400, 24);
  linklabel.align = TEXT_CENTER;

  char buf [64];
  snprintf (buf, 63, "Build timestamp: %s", g_build_timestamp);

  //labels [0].textsize = 1.5;
  //labels [1].textsize = 1.5;
  labels [i].create (ui, frame.widget, buf, 0, 360, 400, 20);
  labels [i].textsize = 0.75;
  labels [i].align = TEXT_CENTER;

  img_logo.create (ui, frame.widget, "", (400-160)/2, 64, 160, 160);
  img_logo.set_png (data_neuralblender_logo_160_png);
  
  on_resize ();
}

void c_aboutwindow::show () { CP
  if (!widget)
    create (ui);
  if (!widget)
    return;

  widget_show_all (widget);
  expose_widget (widget);
}

void c_aboutwindow::hide () { CP
  if (!widget)
    return;

  widget_hide (widget);
}

void c_aboutwindow::on_resize () { CP
  frame.move ((w () - frame.w ()) / 2, (h () - frame.h ()) / 2 - 24);
  btn_ok.move_resize (w () - 140, h () - 56, 128, 40);
  int center_h = frame.w () / 2;
  img_toplogo.move (center_h - img_toplogo.w () / 2, 12);
  img_logo.move (center_h - img_logo.w () / 2, img_logo.y ());
  for (int i = 0; i < 16 && labels [i].widget; i++) {
    labels [i].move_resize (0, labels [i].y (), frame.w (), labels [i].h ());
  }
  linklabel.move_resize (0, linklabel.y (), frame.w (), labels [4].h ());
}

////////////////////////////////////////////////////////////////////////////////
// c_lane_widgets

void c_lane_widgets::create (
    c_neuralblender_ui *ui_,
    Widget_t *parent,
    size_t bank_id_,
    size_t lane_id_,
    int x, int y, int w, int h) { CP
  
  move_resize (x, y, w, h);
  //knob_top = (h - knob_size) / 2;
  
  ui = ui_;
  lane_id = lane_id_;
  bank_id = bank_id_;

  char label [64];
  const char *bank_name = "Amp";
  switch (bank_id) {
    case BANK_PEDAL: bank_name = "Pedal"; break;
    case BANK_CAB:   bank_name = "Cab/IR"; break;
    case BANK_AMP:
    default:         bank_name = "Amp"; break;
  }
  snprintf (label, 31, "%s %c", bank_name, (char) ('A' + lane_id));
  lane_widget.create (ui, parent, label, x, y, w, h);
  lane_widget.lane = lane_id;
  lane_widget.bank = bank_id;
  lane_widget.widget->scale.gravity = NONE;
  
  Widget_t *wp = lane_widget.widget;
  main_widget = wp;
  cont_regcontrols.create (ui, wp, "", 0, 0, 600, 64);
  //cont_advcontrols.create (ui, wp, "", 0, 0, 300, 64);
  wp = cont_regcontrols.widget;
  //wadv = cont_advcontrols.widget;
  wp->scale.gravity = NONE;
  //wadv->scale.gravity = NONE;
  
  // regular controls
  menu_list.create (ui, wp, label, 0, 0, 320, 32);
  menu_list.widget->func.value_changed_callback = combobox_selected_callback;
  menu_list.lane = lane_id;
  menu_list.bank = bank_id;

  int knobs_right = w - 180;
  knob_gain_in.create (ui, wp, "Input", 0, 0, 64, 64);
  knob_ir_pitch.create (ui, wp, "Pitch", 0, 0, 64, 64);
  knob_gain_in.lane = knob_ir_pitch.lane =
    knob_gain_out.lane = knob_dry_out.lane = knob_delay.lane = lane_id;
  knob_gain_in.bank = knob_ir_pitch.bank =
    knob_gain_out.bank = knob_dry_out.bank = knob_delay.bank = bank_id;
  knob_gain_in.set_min (-40);
  knob_gain_in.set_max (40);
  knob_gain_in.set_default (0);
  knob_gain_in.set_value (0);
  knob_gain_in.set_step (0.1);
  knob_gain_in.role = ROLE_GAIN_IN;

  knob_ir_pitch.set_min (-12.0);
  knob_ir_pitch.set_max (12.0);
  knob_ir_pitch.set_default (0);
  knob_ir_pitch.set_value (0);
  knob_ir_pitch.set_step (0.01);
  knob_ir_pitch.role = ROLE_IR_PITCH;
  knob_ir_pitch.hide ();
  
  knob_gain_out.create (ui, wp, "Output", 0, 0, 64, 64);
  knob_gain_out.set_min (-40);
  knob_gain_out.set_max (40);
  knob_gain_out.set_default (0);
  knob_gain_out.set_value (0);
  knob_gain_out.set_step (0.1);
  knob_gain_out.role = ROLE_GAIN_OUT;
  
  knob_dry_out.create (ui, wp, "Dry out", 0, 0, 64, 64);
  knob_dry_out.set_min (-120);
  knob_dry_out.set_max (12);
  knob_dry_out.set_default (-120);
  knob_dry_out.set_value (-120);
  knob_dry_out.set_step (0.1);
  knob_dry_out.role = ROLE_DRY_OUT;
  
  meter_out.create (ui, parent, "", 0, 0, METER_WIDTH, 120);
  meter_out.set_vudata (&vudata_out);
  meter_out.set_stereo (false);
  vudata_out.set_l (0.0, 0.0);
  
  btn_browse.create (ui, wp, "", 0, 0, 100, 40, WSTYLE_IMAGE_BUTTON);
  btn_clear.create  (ui, wp, "",     0, 0, 100, 40, WSTYLE_IMAGE_BUTTON);
  btn_excl.create   (ui, wp, "Use",       0, 0, 100, 40, WSTYLE_IMAGE_TOGGLE);
  btn_mute.create   (ui, wp, "Mute",      0, 0, 100, 40, WSTYLE_IMAGE_TOGGLE);
  switch (bank_id) {
    case BANK_PEDAL:
      btn_browse.set_tooltip ("Load a pedal capture file");
      btn_clear.set_tooltip ("Clear this pedal model");
    break;
    case BANK_AMP:
      btn_browse.set_tooltip ("Load an amp capture file");
      btn_clear.set_tooltip ("Clear this amp model");
    break;
    case BANK_CAB:
      btn_browse.set_tooltip ("Load an IR");
      btn_clear.set_tooltip ("Clear this IR");
    break;
  }
  btn_mute.set_value (false);
  btn_browse.lane = lane_id;
  btn_clear.lane = lane_id;
  btn_mute.lane = lane_id;
  btn_browse.bank = bank_id;
  btn_clear.bank = bank_id;
  btn_mute.bank = bank_id;
  btn_browse.role = ROLE_BROWSE;
  btn_clear.role = ROLE_CLEAR;
  btn_mute.role = ROLE_MUTE;
  btn_excl.role = ROLE_EXCL_USE;
  btn_excl.lane = lane_id;
  btn_excl.bank = bank_id;
  btn_mute.set_image (data_icon_speaker_off_big_png, WSTATE_ON);
  btn_mute.set_image (data_icon_speaker_on_big_png, WSTATE_OFF);
  btn_mute.padding = 16;
  btn_excl.set_image (data_icon_radiobutton_on_png, WSTATE_ON);
  btn_excl.set_image (data_icon_radiobutton_off_png, WSTATE_OFF);
  btn_excl.padding = 0;
  
  // advanced controls
  knob_delay.role = ROLE_DELAY;
  knob_delay.create (ui, wp, "Delay", 0, 0, 64, 64);
  knob_delay.set_min (0);
  knob_delay.set_max (30);
  knob_delay.set_default (0);
  knob_delay.set_value (0);
  knob_delay.set_step (0.01);
  knob_delay.role = ROLE_DELAY;
  //delay.set_tooltip ("Micro delay applied to this amp's output");

  btn_flip.create   (ui, wp, "", 0, 0, 32, 32, WSTYLE_IMAGE_TOGGLE);
  btn_calib.create   (ui, wp, "", 0, 0, 32, 32, WSTYLE_IMAGE_TOGGLE);
  btn_flip.set_tooltip ("DC flip (phase invert)");
  btn_calib.set_tooltip ("Calibrate (normalize) output level");
  btn_flip.role = ROLE_DCFLIP;
  btn_calib.role = ROLE_CALIBRATE;
  btn_flip.lane = lane_id;
  btn_calib.lane = lane_id;
  btn_flip.bank = bank_id;
  btn_calib.bank = bank_id;
  if (ui && lane_id < NB_NUM_MODELS && bank_id < BANK_COUNT)
    btn_calib.set_value (ui->state.banks [bank_id].lanes [lane_id].do_calib);
  //label_flip.create (ui, wp, "DC flip", 0, 0, 75, 32);
  //label_calib.create (ui, wp, "Calib.", 0, 0, 75, 32);
  label_frames.create (ui, wp, "(not loaded)", 0, 0, 75, 24);
  label_frames.textsize = 0.75;
  label_frames.align = TEXT_CENTER;
  label_trim.create (ui, wp, "1.0", 0, 0, 75, 24);
  label_trim.textsize = 0.75;
  label_trim.align = TEXT_CENTER;
  label_engine.create (ui,wp, "(none)", 0, 0, 120, 24);
  label_engine.textsize = 0.75;
  label_engine.align = TEXT_CENTER;
  
  btn_browse.set_image_default (data_icon_folder_big_png);
  btn_clear.set_image_default (data_icon_x_big_png);
  btn_calib.set_image_default (data_icon_calib_big_png);
  btn_flip.set_image_default (data_icon_phase_big_png);
  
  if (ui && lane_id < NB_NUM_MODELS) {
    filepicker.create (ui, btn_browse.widget, lane_id, "Select file");
    btn_browse.filepicker = &filepicker;
    btn_browse.lane = lane_id;
    btn_browse.bank = bank_id;
    filepicker.lane = lane_id;
    filepicker.bank = bank_id;
  }
  
}

void c_lane_widgets::move_resize (
    int x, int y, int w, int h) {
  
  if (!main_widget)
    return;

  lane_widget.move_resize (x, y, w, h);

  cont_regcontrols.move_resize (0, 0, w, h);
  
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
  int button_left = menu_list.x ();
  int button_top = menu_list.y () + menu_list.h () + 8;
  int button_width = std::clamp (std::min (h - 68, w / 10), 24, 96);
  
  knob_delay.move_resize (12, knob_top, knob_size, knob_size + 16);

  btn_browse.move_resize (button_left, button_top, button_width, button_width);
  btn_clear.move_resize (btn_browse.x () + btn_browse.w () + button_padding,
                         button_top, button_width, button_width);
  btn_calib.move_resize (btn_clear.x () + btn_browse.w () + button_padding,
                         button_top, button_width, button_width);
  btn_flip.move_resize (btn_calib.x () + btn_browse.w () + button_padding,
                         button_top, button_width, button_width);
                         
  int mute_x = btn_flip.x () + btn_calib.w () + button_padding;
  int mute_width = std::max (24, menu_list.x () + menu_list.w () - mute_x);
  btn_mute.move_resize (mute_x,
                         button_top, mute_width, button_width);
  if (btn_mute.w () > 80) {
    btn_mute.set_label ("Mute");
    btn_excl.set_label ("Use");
  } else {
    btn_mute.set_label ("");
    btn_excl.set_label ("");
  }
  btn_excl.move_resize (btn_mute.x (), btn_mute.y (), btn_mute.w (), btn_mute.h ());
  btn_mute.padding = btn_mute.h () / 4;
  
  int btnpadding = button_width / 5;
  btn_browse.padding = btnpadding;
  btn_clear.padding = btnpadding;
  btn_flip.padding = btnpadding;
  btn_calib.padding = btnpadding;
  
  knob_gain_in.move_resize (knob_right, knob_top, knob_size, knob_size + 16);
  knob_ir_pitch.move_resize (knob_gain_in.x (), knob_gain_in.y (),
                             knob_gain_in.w (), knob_gain_in.h ());
  knob_gain_out.move_resize (knob_right + (knob_size + 1) * 2, knob_top, knob_size, knob_size + 16);
  knob_dry_out.move_resize (knob_right + knob_size + 1, knob_top, knob_size, knob_size + 16);
  
  // these are outside ->relative to PARENT
  //meter_out.move_resize (w - 12, 16, 5, h - 28);
  meter_out.move_resize (w + 22, y + 4, METER_WIDTH, h - 8);
  
  int adv_btn_x = 84;
  int adv_btn_y = h * 2 / 11;
  label_frames.move_resize (knob_delay.x (), h - 20, knob_delay.w (), 16);
  label_engine.move_resize (knob_gain_in.x (), h - 20, knob_gain_in.w (), 16);
  label_trim.move_resize (knob_dry_out.x (), h - 20, knob_dry_out.w () + 
                          knob_dry_out.w (), 16);
                          
  //move_resize (x, y, w, h);
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
  state.do_vu = prefs.vu_on;
  state.tuner_on = prefs.tuner_on;
  state.tuner_base_freq = prefs.tuner_base_freq;
  state.noisegate_on = prefs.noisegate_on;
  state.noisethresh = prefs.noisethresh;
  state.noiseattack = prefs.noiseattack;
  state.noisehold = prefs.noisehold;
  state.noiserelease = prefs.noiserelease;
  
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
  
  cont_toparea.create (this, mainwindow.widget, "", 0, 0, 640, 50);
  cont_toparea.draw_background = true;
  cont_pedals.create (this, mainwindow.widget, "", 0, 120, 640, 480);
  cont_models.create (this, mainwindow.widget, "", 0, 120, 640, 480);
  cont_cabs.create (this, mainwindow.widget, "", 0, 120, 640, 480);
  cont_other.create (this, mainwindow.widget, "", 0, 120, 640, 480);
  
  mainwindow.set_icon_from_png (data_neuralblender_logo_512_png);

  //label_big.create (this, mainwindow.widget, "NeuralBlender", 120, 24, 400, 40);
  //label_big.align = TEXT_CENTER;
  //label_big.textsize = 1.5;
  img_logo.create (this, cont_toparea.widget, "", 0, 0, 256, 32);
  img_logo.set_png (data_textlogo_1024x128_png);
  
  const int tabbutton_padding = 14;
  btn_tab_pedals.create (this, cont_toparea.widget, "PDL", 0, 0, 84, 50);
  btn_tab_pedals.role = ROLE_BANKSWITCH;
  btn_tab_pedals.bank = BANK_PEDAL;
  btn_tab_pedals.page = PAGE_PEDAL;
  btn_tab_pedals.set_image_default (data_icon_power_on_png);
  btn_tab_pedals.padding = tabbutton_padding;
  btn_tab_models.create (this, cont_toparea.widget, "AMP", 0, 0, 84, 50);
  btn_tab_models.role = ROLE_BANKSWITCH;
  btn_tab_models.bank = BANK_AMP;
  btn_tab_models.page = PAGE_AMP;
  btn_tab_models.set_image_default (data_icon_power_on_png);
  btn_tab_models.padding = tabbutton_padding;
  btn_tab_cabs.create (this, cont_toparea.widget, "CAB", 0, 0, 84, 50);
  btn_tab_cabs.role = ROLE_BANKSWITCH;
  btn_tab_cabs.bank = BANK_CAB;
  btn_tab_cabs.page = PAGE_CAB;
  btn_tab_cabs.set_image_default (data_icon_power_on_png);
  btn_tab_cabs.padding = tabbutton_padding;
  btn_tab_other.create (this, cont_toparea.widget, "...", 0, 0, 84, 50);
  btn_tab_other.role = ROLE_BANKSWITCH;
  btn_tab_other.bank = BANK_AMP; // no DSP bank, keep cached bank unchanged
  btn_tab_other.page = PAGE_OTHER;
  
  btn_enable.create (this, cont_toparea.widget, "",  20, 12, 40, 40, WSTYLE_IMAGE_TOGGLE);
  btn_enable.set_value (true);
  btn_enable.role = ROLE_BYPASS;
  btn_enable.set_image (data_icon_power_on_png, WSTATE_ON);
  btn_enable.set_image (data_icon_power_grey_png, WSTATE_OFF);
  
  //btn_muteall.create (this, mainwindow.widget, "Mute all", 500, 12, 120, 40, WSTYLE_IMAGE_TOGGLE);
  btn_muteall.create (this, cont_toparea.widget, "", 500, 12, 40, 40, WSTYLE_IMAGE_TOGGLE);
  btn_muteall.role = ROLE_MUTEALL;
  btn_muteall.set_image (data_icon_speaker_off_big_png, WSTATE_ON);
  btn_muteall.set_image (data_icon_speaker_on_big_png, WSTATE_OFF);
  
  btn_noisegate.create (this, cont_toparea.widget, "", 0, 0, 40, 40, WSTYLE_IMAGE_TOGGLE);
  btn_noisegate.role = ROLE_NOISEGATE;
  btn_noisegate.set_image_default (data_icon_noisegate_png);
  btn_noisegate.set_tooltip ("noise gate");
  btn_tuner.create (this, cont_toparea.widget, "", 0, 0, 40, 40, WSTYLE_IMAGE_TOGGLE);
  btn_tuner.set_image_default (data_icon_tuner_png);
  btn_tuner.set_tooltip ("Tuner");
  btn_tuner.role = ROLE_TUNER;
  
  btn_enable.padding =    12;
  btn_muteall.padding =   12;
  btn_tuner.padding =     12;
  btn_noisegate.padding = 12;
  
  aboutwindow.create (this);
  prefswindow.create (this);
  
  for (i = 0; i < NB_NUM_MODELS; i++) {
    lanes_pedals [i].create (this, cont_pedals.widget, BANK_PEDAL, i, 0, 0, 1, 1);
    lanes_models [i].create (this, cont_models.widget, BANK_AMP,   i, 0, 0, 1, 1);
    lanes_cabs   [i].create (this, cont_cabs.widget,   BANK_CAB,   i, 0, 0, 1, 1);
    
    //lanes_pedals [i].lane_widget.active_bg_colors = &frame_pedal_active_bg;
    //lanes_pedals [i].lane_widget.active_fg_colors = &frame_pedal_active_fg;
    //lanes_models [i].lane_widget.active_bg_colors = &frame_amp_active_bg;
    //lanes_models [i].lane_widget.active_fg_colors = &frame_amp_active_fg;
    //lanes_cabs   [i].lane_widget.active_bg_colors = &frame_cab_active_bg;
    //lanes_cabs   [i].lane_widget.active_fg_colors = &frame_cab_active_fg;
  }
  meter_in [PAGE_PEDAL].create (this, cont_pedals.widget, "", 6, 70, 5, 520);
  meter_in [PAGE_AMP].create (this, cont_models.widget, "", 6, 70, 5, 520);
  meter_in [PAGE_CAB].create (this, cont_cabs.widget, "", 6, 70, 5, 520);
  meter_in [PAGE_OTHER].create (this, cont_other.widget, "", 6, 70, 5, 520);
  meter_masterout.create (this, cont_other.widget, "", 6, 70, 5, 520);

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
  
  frame_other_volumepresence.create (this, cont_other.widget, "", 16, 16, 512, 128);
  knob_mastervolume.create (this, frame_other_volumepresence.widget, "Master out", 16, 12, 80, 96);
  knob_presence.create (this, frame_other_volumepresence.widget, "Presence", 116, 12, 80, 96);
  knob_mastervolume.set_min (-40);
  knob_mastervolume.set_max (12);
  knob_mastervolume.set_value (gain_to_db (state.master_gain));
  knob_mastervolume.set_default (0);
  knob_mastervolume.set_step (0.1);
  knob_mastervolume.role = ROLE_MASTER;
  knob_presence.set_min (0);
  knob_presence.set_max (1);
  knob_presence.set_value (state.presence);
  knob_presence.set_default (0);
  knob_presence.set_step (0.01);
  knob_presence.role = ROLE_PRESENCE;
  
  frame_other_noisegate.create (this, cont_other.widget, "", 16, 16, 512, 128);
  label_other_noisegate.create (this, frame_other_noisegate.widget, "Noise gate:", 16, 8, 200, 24);
  knob_noisethresh.create (this, frame_other_noisegate.widget,  "Thresh",   16, 36, 64, 72);
  knob_noiseattack.create (this, frame_other_noisegate.widget,  "Attack",   76, 36, 64, 72);
  knob_noisehold.create (this, frame_other_noisegate.widget,    "Hold",    136, 36, 64, 72);
  knob_noiserelease.create (this, frame_other_noisegate.widget, "Release", 196, 36, 64, 72);
  knob_noisethresh.set_min (-120);
  knob_noisethresh.set_max (-6);
  knob_noisethresh.set_value (state.noisethresh);
  knob_noisethresh.set_default (-60);
  knob_noisethresh.set_step (0.1);
  knob_noisethresh.role = ROLE_NOISETHRESH;

  knob_noiseattack.set_min (0);
  knob_noiseattack.set_max (200);
  knob_noiseattack.set_value (state.noiseattack);
  knob_noiseattack.set_default (2);
  knob_noiseattack.set_step (0.1);
  knob_noiseattack.role = ROLE_NOISEATTACK;

  knob_noisehold.set_min (0);
  knob_noisehold.set_max (200);
  knob_noisehold.set_value (state.noisehold);
  knob_noisehold.set_default (10);
  knob_noisehold.set_step (0.1);
  knob_noisehold.role = ROLE_NOISEHOLD;

  knob_noiserelease.set_min (0);
  knob_noiserelease.set_max (500);
  knob_noiserelease.set_value (state.noiserelease);
  knob_noiserelease.set_default (20);
  knob_noiserelease.set_step (0.1);
  knob_noiserelease.role = ROLE_NOISERELEASE;

  frame_other_linkexcl.create (this, cont_other.widget, "", 16, 16, 512, 128);
  const int x0 = 16, x1 = 186, x2 = 296, x3 = 406;
  const int y0 = 20, y1 = 68;
  label_other_link.create (this, frame_other_linkexcl.widget, "Link calibration: ", x0, y0, 150, 32);
  label_other_excl.create (this, frame_other_linkexcl.widget, "Exclusive mode: ", x0, y1, 150, 32);
  btn_other_link_pedal.create (this, frame_other_linkexcl.widget, "Pedal", x1, y0, 150, 32, WSTYLE_CHECKBOX);
  btn_other_link_amp.create (this, frame_other_linkexcl.widget, "Amp", x2, y0, 150, 32, WSTYLE_CHECKBOX);
  btn_other_link_cab.create (this, frame_other_linkexcl.widget, "Cab/IR", x3, y0, 150, 32, WSTYLE_CHECKBOX);
  btn_other_excl_pedal.create (this, frame_other_linkexcl.widget, "Pedal", x1, y1, 150, 32, WSTYLE_CHECKBOX);
  btn_other_excl_amp.create (this, frame_other_linkexcl.widget, "Amp", x2, y1, 150, 32, WSTYLE_CHECKBOX);
  btn_other_excl_cab.create (this, frame_other_linkexcl.widget, "Cab/IR", x3, y1, 150, 32, WSTYLE_CHECKBOX);
  btn_other_link_pedal.role = ROLE_LINKED_CALIB;
  btn_other_link_amp.role = ROLE_LINKED_CALIB;
  btn_other_link_cab.role = ROLE_LINKED_CALIB;
  btn_other_link_pedal.bank = BANK_PEDAL;
  btn_other_link_amp.bank = BANK_AMP;
  btn_other_link_cab.bank = BANK_CAB;
  btn_other_excl_pedal.role = ROLE_EXCL_TOGGLE;
  btn_other_excl_amp.role = ROLE_EXCL_TOGGLE;
  btn_other_excl_cab.role = ROLE_EXCL_TOGGLE;
  btn_other_excl_pedal.bank = BANK_PEDAL;
  btn_other_excl_amp.bank = BANK_AMP;
  btn_other_excl_cab.bank = BANK_CAB;
  
  frame_other_misc.create (this, cont_other.widget, "", 16, 16, 512, 128);
  label_other_tuner.create (this, frame_other_misc.widget, "Tuner base frequency: ", 16, 20, 200, 32);
  label_other_calib.create (this, frame_other_misc.widget, "Calibration target dB: ", 16, 60, 200, 32);
  btn_other_vu.create (this, frame_other_misc.widget, "VU meters", 16, 100, 200, 32, WSTYLE_CHECKBOX);
  btn_other_bass.create (this, frame_other_misc.widget, "Calibrate for bass", 16, 140, 200, 32, WSTYLE_CHECKBOX);
  text_other_tuner.create (this, frame_other_misc.widget, "", 250, 14, 150, 40);
  text_other_calib.create (this, frame_other_misc.widget, "", 250, 60, 150, 40);
  btn_other_prefs.create (this, frame_other_misc.widget, "Settings", 0, 130, 120, 40, WSTYLE_IMAGE_BUTTON);
  btn_other_prefs.set_image_default (data_xputty_gear_png);
  btn_other_about.create (this, frame_other_misc.widget, "About...", 0, 130, 120, 40, WSTYLE_IMAGE_BUTTON);
  btn_other_about.set_image_default (data_xputty_info_png);
  btn_other_vu.role = ROLE_VUTOGGLE;
  btn_other_bass.role = ROLE_CALIBBASS;
  btn_other_prefs.role = ROLE_PREFS;
  btn_other_about.role = ROLE_ABOUT;
  text_other_tuner.role = ROLE_TUNER_BASE_FREQ;
  text_other_calib.role = ROLE_CALIB_TARGET_DB;
  
  btn_other_tuner_down.create (this, frame_other_misc.widget, "", 420, 14, 40, 40, WSTYLE_IMAGE_BUTTON);
  btn_other_tuner_down.set_image_default (data_icon_flat_png);
  btn_other_tuner_down.role = ROLE_TUNER_DOWN;
  btn_other_tuner_up.create (this, frame_other_misc.widget, "", 468, 14, 40, 40, WSTYLE_IMAGE_BUTTON);
  btn_other_tuner_up.set_image_default (data_icon_sharp_png);
  btn_other_tuner_up.role = ROLE_TUNER_UP;
  btn_other_tuner_default.create (this, frame_other_misc.widget, "", 516, 14, 40, 40, WSTYLE_IMAGE_BUTTON);
  btn_other_tuner_default.set_image_default (data_icon_tuner_png);
  btn_other_tuner_default.role = ROLE_TUNER_DEFAULT;
  
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
  
  if (!ui_resize_lock && mainwindow.widget) {
    Widget_t *mw = mainwindow.widget;
    Metrics_t metrics;
    os_get_window_metrics (mw, &metrics);
    ui_resize_lock = true;
    
    int window_width = std::max (MIN_WINDOW_WIDTH, mainwindow.w ());
    int window_height = std::max (MIN_WINDOW_HEIGHT, mainwindow.h ());
    debug ("move_resize: snap=%d visible=%d main=%d,%d metrics=%d,%d pending=%d,%d",
           (int) snap_to_default, (int) metrics.visible,
           mainwindow.w (), mainwindow.h (),
           (int) metrics.width, (int) metrics.height,
           pending_resize_w, pending_resize_h);
    if (!snap_to_default && metrics.visible) {
      window_width = std::max (MIN_WINDOW_WIDTH, (int) (metrics.width / mw->app->hdpi));
      window_height = std::max (MIN_WINDOW_HEIGHT, (int) (metrics.height / mw->app->hdpi));
    }
    
    img_logo.move_resize (window_width / 2 - 128, 12, 256, 32);
    
    //if (do_set_min_size)
    mainwindow.set_min_size (MIN_WINDOW_WIDTH, MIN_WINDOW_HEIGHT);
    
    cont_toparea.move_resize (0, 0, window_width, 116);
    const int page_y = 128;
    const int page_h = window_height - 138;
    cont_pedals.move_resize (0, page_y, window_width, page_h);
    cont_models.move_resize (0, page_y, window_width, page_h);
    cont_cabs.move_resize (0, page_y, window_width, page_h);
    cont_other.move_resize (0, page_y, window_width, page_h);
    cont_toparea.expose ();
    
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
    const int btnr = window_width - 54 * 4 - 66;
    const int btnt = 66;
    const int btnw = 88;
    btn_tab_pedals.move       (btnl + 0,   btnt);
    btn_tab_models.move       (btnl + btnw,     btnt);
    btn_tab_cabs.move         (btnl + btnw * 2, btnt);
    btn_tab_other.move        (btnl + btnw * 3, btnt);
    btn_enable.move_resize    (btnr + 54,     btnt, 50, 50);
    btn_muteall.move_resize   (btnr + 54 * 2, btnt, 50, 50);
    btn_tuner.move_resize     (btnr + 54 * 3, btnt, 50, 50);
    btn_noisegate.move_resize (btnr + 54 * 4, btnt, 50, 50);
    
    if (tuner.created)
      tuner.move_resize (4, 4, window_width - 8, 56);
    
    if (page_has_bank (visible_page)) {
      size_t i;
      c_lane_widgets *bank_lanes = lanes_for_bank (visible_bank);
      for (i = 0; i < NB_NUM_MODELS; i++) {
        bank_lanes [i].move_resize (
          16, lane_top + i * (lane_height + lane_gap), lane_width, lane_height);
      }
      
      const int lane_bottom = lane_top + lane_count * lane_height + total_gap;
      meter_in [visible_bank].move_resize (
        5, lane_top + 4, 5, std::max (1, lane_bottom - lane_top - 8));
    } else {
      const int panelwidth = window_width - 32;
      const int meter_h = std::max (1, page_h - 8);
      meter_in [PAGE_OTHER].move_resize (5, 4, METER_WIDTH, meter_h);
      meter_masterout.move_resize (
        window_width - 5 - METER_WIDTH, 4, METER_WIDTH, meter_h);
      frame_other_volumepresence.move_resize (16, 0, panelwidth / 2 - 8, 120);
      frame_other_noisegate.move_resize (panelwidth / 2 + 24, 0, panelwidth / 2 - 8, 120);
      frame_other_linkexcl.move_resize (16, 136, panelwidth, 120);
      frame_other_misc.move_resize (16, 272, panelwidth, window_height - 412);
      const int about_y = frame_other_misc.h () - 60;
      btn_other_prefs.move (frame_other_misc.w () - 280, about_y);
      btn_other_about.move (frame_other_misc.w () - 140, about_y);
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

c_meter &c_neuralblender_ui::input_meter_for_bank (_lane_bank bank) {
  if (bank < BANK_PEDAL || bank >= BANK_COUNT)
    bank = BANK_AMP;
  return meter_in [bank];
}

c_vudata &c_neuralblender_ui::input_vudata_for_bank (_lane_bank bank) {
  if (bank < BANK_PEDAL || bank >= BANK_COUNT)
    bank = BANK_AMP;
  return vudata_in [bank];
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
  if (tuner.created || !mainwindow.widget)
    return;

  Metrics_t metrics;
  os_get_window_metrics (mainwindow.widget, &metrics);
  const int w = std::max (1, metrics.width - 8);
  tuner.create (this, cont_toparea.widget, "", 4, 4, w, 56);
  if (blender)
    tuner.set_pitchtracker (&blender->pitchtracker);
}

void c_neuralblender_ui::sync_tuner_visibility () {
  btn_tuner.set_value (state.tuner_on);
  if (state.tuner_on) {
    ensure_tuner_created ();
    tuner.show ();
    img_logo.hide ();
    if (tuner.widget)
      tuner.expose ();
    if (tuner.tuner.widget)
      expose_widget (tuner.tuner.widget);
  } else {
    tuner.hide ();
    img_logo.show ();
    if (img_logo.widget)
      img_logo.expose ();
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
    c_button &button,
    const c_neuralblender_state &state,
    _lane_bank bank) {

  button.set_image_default (
    bank_bypass_for_state (state, bank)
      ? data_icon_power_grey_png
      : data_icon_power_on_png);
}

void c_neuralblender_ui::on_button (c_button *btn, bool value) {
  if (!btn || updating_from_state)
    return;
    
  size_t lane = btn->lane;

  switch (btn->role) {
    case ROLE_BYPASS: CP
      state.bypass = !value;
      on_bypass (btn, value);
    break;

    case ROLE_MUTE: CP
      on_mute (btn, value);
      if (lane < NB_NUM_MODELS && btn->bank < BANK_COUNT) {
        state.banks [btn->bank].lanes [lane].lane_mute = value;
      }
    break;

    case ROLE_DCFLIP: CP
      on_dcflip (btn, value);
      if (lane < NB_NUM_MODELS && btn->bank < BANK_COUNT) {
        state.banks [btn->bank].lanes [lane].dcflip = value;
      }
    break;

    case ROLE_BANKSWITCH: CP
      debug ("visible page: %d", btn->page);
      if (btn->page >= PAGE_PEDAL && btn->page < PAGE_COUNT) {
        const _ui_page page = (_ui_page) btn->page;
        if (page_has_bank (page) &&
            (btn->last_mouse_button == Button3 || visible_page == page)) {
          const _lane_bank bank = bank_for_page (page);
          const bool bypass = !bank_bypass_for_state (state, bank);
          set_bank_bypass_for_state (state, bank, bypass);
          on_bank_bypass (btn, bank, bypass);
          sync_widgets_from_state (state);
        } else {
          on_bank_switch (btn, btn->page);
        }
      }
    break;

    case ROLE_CALIBRATE: CP
      if (lane < NB_NUM_MODELS && btn->bank < BANK_COUNT) {
        state.banks [btn->bank].lanes [lane].do_calib = value;
      }
      on_calibrate (btn, value);
    break;

    case ROLE_MUTEALL: CP
      state.mute_all = value;
      on_muteall (btn, value);
    break;

    case ROLE_BROWSE: CP
      on_filebrowse (btn);
      if (btn->filepicker)
        btn->filepicker->show ();
      else
        debug ("!filepicker");
    break;

    case ROLE_CLEAR: CP
      on_fileclear (btn);
    break;

    case ROLE_ABOUT: CP
      aboutwindow.show ();
      //aboutwindow.show ();
      on_about ();
    break;

    case ROLE_ABOUTOK: CP
      aboutwindow.hide ();
    break;

    case ROLE_PREFS: CP
      write_prefs_to (prefs);
      prefswindow.get_prefs_from (prefs);
      prefswindow.show ();
      on_prefs ();
    break;

    case ROLE_PREFSDEFAULTS: CP
      prefswindow.load_defaults ();
    break;

    case ROLE_PREFSOK: CP
      prefswindow.set_prefs_to (prefs);
      apply_prefs (prefs);
      on_prefs_ok ();
      prefswindow.hide ();
    break;
    
    case ROLE_PREFSCANCEL: CP
      prefswindow.hide ();
    break;

    case ROLE_VUTOGGLE: CP
      prefs.vu_on = value;
      vu_on (value);
      on_vu (btn, value);
    break;

    case ROLE_LINKED_CALIB: CP
      if (btn->bank < BANK_COUNT)
        visible_bank = (_lane_bank) btn->bank;
      if (page_has_bank (visible_page) || btn->bank < BANK_COUNT) {
        const _lane_bank bank = btn->bank < BANK_COUNT ?
          (_lane_bank) btn->bank : visible_bank;
        set_linked_calib_for_bank (bank, value);
        on_linked_calib (btn, value);
      }
    break;

    case ROLE_CALIBBASS: CP
      prefs.calib_source = value ? 1 : 0;
      on_calib_bass (btn, value);
    break;

    case ROLE_NOISEGATE: CP
      state.noisegate_on = value;
      prefs.noisegate_on = value;
      on_noisegate (btn, value);
    break;
    
    case ROLE_TUNER: CP
      state.tuner_on = value;
      prefs.tuner_on = value;
      on_tuner (btn, value);
      sync_tuner_visibility ();
        
    break;
    
    case ROLE_TUNER_UP:
      state.tuner_base_freq *= SEMITONE_MULTIPLIER;
      on_tuner_base_freq (btn, state.tuner_base_freq);
    break;
    
    case ROLE_TUNER_DOWN:
      state.tuner_base_freq /= SEMITONE_MULTIPLIER;
      on_tuner_base_freq (btn, state.tuner_base_freq);
    break;

    case ROLE_TUNER_DEFAULT:
      state.tuner_base_freq = 440.0f;
      on_tuner_base_freq (btn, state.tuner_base_freq);
    break;

    case ROLE_EXCL_TOGGLE: CP
      if (btn->bank < BANK_COUNT)
        visible_bank = (_lane_bank) btn->bank;
      if (value) {
        size_t exclusive_lane = choose_exclusive_lane ();
        on_excl (btn, exclusive_lane); // this is 1-BASED, 0 = normal mode
      } else {
        on_excl (btn, 0);
      }
    break;

    case ROLE_EXCL_USE:
      on_excl_use (btn, value);
    break;

    //case ROLE_ADV_TOGGLE:
    //  on_advanced (this, value);
    //break;

    default: CP
    break;
  }
  sync_widgets_from_state (state);
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

  btn_other_bass.set_value (p.calib_source == 1);

  vu_on (p.vu_on);

  state.noisegate_on = p.noisegate_on;
  state.noisethresh = p.noisethresh;
  state.noiseattack = p.noiseattack;
  state.noisehold = p.noisehold;
  state.noiserelease = p.noiserelease;
  state.tuner_base_freq = p.tuner_base_freq;
  btn_noisegate.set_value (p.noisegate_on);
  knob_mastervolume.set_value (gain_to_db (state.master_gain));
  knob_presence.set_value (state.presence);
  knob_noisethresh.set_value (p.noisethresh);
  knob_noiseattack.set_value (p.noiseattack);
  knob_noisehold.set_value (p.noisehold);
  knob_noiserelease.set_value (p.noiserelease);

  format_freq_text (buf, sizeof (buf), p.tuner_base_freq);
  text_other_tuner.set_text (buf);
  format_db_text (buf, sizeof (buf), p.calib_target_db);
  text_other_calib.set_text (buf);

  /*if (p.noisegate_on)
    knob_noisethresh.show ();
  else
    knob_noisethresh.hide ();*/

  state.tuner_on = p.tuner_on;
  sync_tuner_visibility ();
}

void c_neuralblender_ui::apply_prefs (t_prefs &p) { CP
  apply_ui_prefs (p);
  for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank)
    state.banks [bank].linked_calib = p.linked_calib;
}

void c_neuralblender_ui::write_prefs_to (t_prefs &p) { CP
  p.vu_on = state.do_vu;
  p.tuner_on = state.tuner_on;
  p.noisegate_on = state.noisegate_on;
  p.noisethresh = state.noisethresh;
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
      
      bank_lanes [i].label_engine.set_label (engine_names [eng]);
      if (eng == ENGINE_IR) {
        bank_lanes [i].knob_gain_in.hide ();
        bank_lanes [i].knob_ir_pitch.show ();
      } else {
        bank_lanes [i].knob_ir_pitch.hide ();
        bank_lanes [i].knob_gain_in.show ();
      }

      snprintf (buf, 127, "%d frames", nframes);
      bank_lanes [i].label_frames.set_label (buf);
      if (trim == 1.00) {
        bank_lanes [i].label_trim.set_label ("");
      } else {
        float db = gain_to_db (trim);
        snprintf (buf, 127, "Trim: %s%.02fdB", db > 0.0 ? "+" : "", db);
        bank_lanes [i].label_trim.set_label (buf);
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
  debug ("n=%d", n);
  set_exclusive_lane_for_bank (visible_bank, n);
  if (n > 0 && n <= (int) NB_NUM_MODELS)
    last_exclusive_lane [visible_bank] = (size_t) n;
  if (!w)
    return;

  switch (visible_bank) {
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

  if (state.do_vu) {
    if (visible_page == PAGE_OTHER) {
      meter_in [PAGE_OTHER].on_ui_timer ();
      meter_masterout.on_ui_timer ();
    } else {
      meter_in [visible_bank].on_ui_timer ();
      c_lane_widgets *bank_lanes = lanes_for_bank (visible_bank);
      for (int i = 0; i < NB_NUM_MODELS; i++)
        bank_lanes [i].meter_out.on_ui_timer ();
    }
  }
  
  if (state.tuner_on) {
    tuner.on_ui_timer ();
  }
  
  run_embedded (&app);
  return 0;
}

void c_neuralblender_ui::draw () {
  if (!mainwindow.widget)
    return;

  widget_draw (mainwindow.widget, NULL);
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
  format_db_text (buf, sizeof (buf), prefs.calib_target_db);
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
          c_filepicker &fp = bank_lanes [i].filepicker;
          fp.current_dir = path_dirname (lane.filename);
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

  btn_other_bass.set_value (prefs.calib_source == 0 ? false : true);
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
      visible_lanes [i].lane_widget.set_state (WSTATE_DISABLED);
    } else {
      visible_lanes [i].lane_widget.set_state (WSTATE_NORMAL);
    }

    if (exclusive_on) { CP
      visible_lanes [i].lane_widget.set_state (WSTATE_DISABLED);
      widget_hide (visible_lanes [i].btn_mute.widget);
      widget_show (visible_lanes [i].btn_excl.widget);
    } else { CP
      widget_show (visible_lanes [i].btn_mute.widget);
      widget_hide (visible_lanes [i].btn_excl.widget);
    }
  }
  if (exclusive_on && !state.mute_all && enabled && !visible_bank_bypassed) {
    visible_lanes [visible_exclusive_lane - 1].lane_widget.set_state (WSTATE_SELECTED);
  }

  updating_from_state = false;
}
