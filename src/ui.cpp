
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

#include "neuralblender.h"
#include "ui.h"

#include "data/data.h"

#define CMDLINE_DEBUG_COLOR ANSI_MAGENTA
#include "cmdline_debug.h"

#define MIN_WINDOW_HEIGHT (64 + (150 * NB_NUM_MODELS))
//#define DEFAULT_WINDOW_HEIGHT (12 + std::min (640, (52 + (180 * NB_NUM_MODELS))))
#define DEFAULT_WINDOW_HEIGHT MIN_WINDOW_HEIGHT
#define MIN_WINDOW_WIDTH 640
#define DEFAULT_WINDOW_WIDTH 640

extern const char *g_build_timestamp;

void combobox_selected_callback (void *w_, void *user_data);

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
         (lower.size () >= 6 && lower.rfind (".aidax") == lower.size () - 6);
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

  const std::string vu = configfile.get_item (CONFIG_KEY_NAME_VU);
  if (!vu.empty ())
    prefs.vu_on = configfile.istrue (CONFIG_KEY_NAME_VU);

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
  btn_about.create (ui, widget, "About...", 0, 0, 128, 40);
  btn_about.role = ROLE_ABOUT;
  btn_about.set_image (data_xputty_info_png);
  
  label_calibdb.create (ui, frame1.widget, "Calibration target dB:", 0, 0, 120, 32);
  label_vuscale.create (ui, frame1.widget, "VU meter scale dB:", 0, 0, 120, 32);
  label_vuheadroom.create (ui, frame1.widget, "VU meter headroom dB:", 0, 0, 120, 32);
  label_spacer1.create (ui, frame1.widget, "", 0, 0, 12, 12);
  label_linkexplain.create (ui, frame1.widget, "(trim follows loudest model)", 0, 0, 320, 36);

  btn_vu.create (ui, frame1.widget, "VU meters", 0, 0, 300, 32, WSTYLE_CHECKBOX);
  btn_linkcalib.create (ui, frame1.widget, "Linked calibration",
                        0, 0, 300, 32, WSTYLE_CHECKBOX);
  btn_bass.create (ui, frame1.widget, "Calibrate for bass", 0, 0, 300, 32, WSTYLE_CHECKBOX);
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
  btn_about.move_resize (12, h () - 56, 128, 40);
  btn_defaults.move_resize (12, frame1.h () - 50, frame1.w () - 24, 40);
  
  int labels_x = 16;
  int controls_x = 0;
  
  std::vector<c_widget *> leftcontrols = {
    &label_calibdb,
    &label_vuscale,
    &label_vuheadroom,
    &label_spacer1,
    &btn_vu,
    &btn_linkcalib,
    &btn_bass
  };
  for (int i = 0; i < leftcontrols.size (); i++) {
    int w = 0;
    leftcontrols [i]->resize_to_label ();
    leftcontrols [i]->get_label_size (&w, NULL);
    debug ("w=%d", w);
    if (w > controls_x)
      controls_x = w;
    
    leftcontrols [i]->move (labels_x, 32 + i * 40);
  }
  controls_x += 36;
  
  btn_vu.resize (btn_vu.w () + 30, btn_vu.h ());
  btn_linkcalib.resize (btn_linkcalib.w () + 30, btn_linkcalib.h ());
  btn_bass.resize (btn_bass.w () + 30, btn_bass.h ());
  debug ("controls_x=%d, label_calibdb: %d", controls_x, label_calibdb.y ());
  text_calibdb.move_resize    (controls_x, label_calibdb.y () - 4, 120, 36);
  text_vuscale.move_resize    (controls_x, label_vuscale.y () - 4, 120, 36);
  text_vuheadroom.move_resize (controls_x, label_vuheadroom.y () - 4, 120, 36);
  label_linkexplain.move (btn_linkcalib.x () + btn_linkcalib.w (), btn_linkcalib.y ());
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
  btn_vu.set_value (true);
  btn_linkcalib.set_value (false);
  btn_bass.set_value (false);
}

void c_prefswindow::get_prefs_from (t_prefs &prefs) { CP
  if (!widget)
    create (ui);

  char buf [128];
  snprintf (buf, 127, "%.6g", prefs.calib_target_db);
  text_calibdb.set_text (buf);

  snprintf (buf, 127, "%.6g", prefs.vu_scale_db);
  text_vuscale.set_text (buf);

  snprintf (buf, 127, "%.6g", prefs.vu_headroom_db);
  text_vuheadroom.set_text (buf);

  btn_vu.set_value (prefs.vu_on);
  btn_linkcalib.set_value (prefs.linked_calib);
  btn_bass.set_value (prefs.calib_source == 1);
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

  prefs.vu_on = btn_vu.value;
  prefs.linked_calib = btn_linkcalib.value;
  prefs.calib_source = btn_bass.value ? 1 : 0;
}

////////////////////////////////////////////////////////////////////////////////
// c_aboutwindow

void c_aboutwindow::create (c_neuralblender_ui *ui_) { CP
  ui = ui_;
  if (!ui || !ui->ui_ready || widget)
    return;
  
  if (!c_toplevelwindow::create (
      ui,
      os_get_root_window (&ui->app, IS_WINDOW),
      "About NeuralBlender",
      0, 0, 420, 480,
      ui->mainwindow.widget))
    return;

  //widget = widget;
  set_min_size_to_current ();
  
  frame.create (ui, widget, "", 12, 12, w () - 24, h () - 80);
  
  //btn_ok.create (ui, frame.widget, "OK", 160, 395, 80, 40);
  btn_ok.create (ui, widget, "OK", 0, 0, 120, 40);
  btn_ok.role = ROLE_ABOUTOK;
  btn_ok.set_image (data_xputty_approved_png);
  
  const char *text [] = {
    "NeuralBlender",
    "",
#ifdef LV2
    "An amp modeling plugin based on",
#else
    "An amp modeling app based on",
#endif
    "RTNeural and NeuralAmpModeler",
    "",
    "by Deimos Laboratories",
    "github.com/DeimosLabs/NeuralBlender",
    NULL
  };
  
  int i;
  for (i = 0; text [i]; i++) {
    int h = (i == 0 ? 20 : (180 + i * 24));
    labels [i].create (ui, frame.widget, text [i], 0, h, 400, 24);
    labels [i].align = TEXT_CENTER;
  }

  char buf [64];
  snprintf (buf, 63, "Build timestamp: %s", g_build_timestamp);

  labels [0].textsize = 1.5;
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
}

////////////////////////////////////////////////////////////////////////////////
// c_lane_widgets

void c_lane_widgets::create (
    c_neuralblender_ui *ui_,
    Widget_t *parent,
    size_t which,
    int x, int y, int w, int h) { CP
  
  move_resize (x, y, w, h);
  //knob_top = (h - knob_size) / 2;
  
  char label [64];
  ui = ui_;
  snprintf (label, 31, "Amp %c", (char) ('A' + which));
  lane_id = which;
  lane_widget.create (ui, parent, label, x, y, w, h);
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
  menu_list.lane = which;

  int knobs_right = w - 180;
  gain_in.create (ui, wp, "Input", 0, 0, 64, 64);
  gain_in.lane = gain_out.lane = delay.lane = which;
  gain_in.set_min (-40);
  gain_in.set_max (40);
  gain_in.set_defaultvalue (0);
  gain_in.set_value (0);
  gain_in.set_step (1);
  gain_in.role = ROLE_GAIN_IN;
  
  gain_out.create (ui, wp, "Output", 0, 0, 64, 64);
  gain_out.role = ROLE_GAIN_OUT;
  gain_out.set_min (-40);
  gain_out.set_max (40);
  gain_out.set_defaultvalue (0);
  gain_out.set_value (0);
  gain_out.set_step (1);
  gain_out.role = ROLE_GAIN_OUT;
  
  meter_out.create (ui, wp, "", 0, 0, 5, 120);
  meter_out.set_vudata (&vudata_out);
  meter_out.set_stereo (false);
  vudata_out.set_l (0.0, 0.0);
  
  btn_browse.create (ui, wp, "", 0, 0, 100, 40, WSTYLE_IMAGE_BUTTON);
  btn_clear.create  (ui, wp, "",     0, 0, 100, 40, WSTYLE_IMAGE_BUTTON);
  btn_excl.create   (ui, wp, "Use",       0, 0, 100, 40, WSTYLE_TOGGLE);
  btn_mute.create   (ui, wp, "Mute",      0, 0, 100, 40, WSTYLE_IMAGE_TOGGLE);
  btn_browse.set_tooltip ("Load a model capture file");
  btn_clear.set_tooltip ("Clear this model");
  btn_mute.set_value (false);
  btn_browse.lane = which;
  btn_clear.lane = which;
  btn_mute.lane = which;
  btn_browse.role = ROLE_BROWSE;
  btn_clear.role = ROLE_CLEAR;
  btn_mute.role = ROLE_MUTE;
  btn_excl.role = ROLE_EXCL_USE;
  btn_excl.lane = which;
  btn_mute.set_image (data_icon_speaker_off_big_png, WSTATE_ON);
  btn_mute.set_image (data_icon_speaker_on_big_png, WSTATE_OFF);
  btn_mute.padding = 16;
  
  // advanced controls
  delay.role = ROLE_DELAY;
  delay.create (ui, wp, "Delay", 0, 0, 64, 64);
  delay.set_min (0);
  delay.set_max (30);
  delay.set_defaultvalue (0);
  delay.set_value (0);
  delay.set_step (0.01);
  delay.role = ROLE_DELAY;
  //delay.set_tooltip ("Micro delay applied to this amp's output");

  btn_flip.create   (ui, wp, "", 0, 0, 32, 32, WSTYLE_IMAGE_TOGGLE);
  btn_calib.create   (ui, wp, "", 0, 0, 32, 32, WSTYLE_IMAGE_TOGGLE);
  btn_flip.set_tooltip ("DC flip (phase invert)");
  btn_calib.set_tooltip ("Calibrate (normalize) output level");
  btn_flip.role = ROLE_DCFLIP;
  btn_calib.role = ROLE_CALIBRATE;
  btn_flip.lane = which;
  btn_calib.lane = which;
  if (ui && which < NB_NUM_MODELS)
    btn_calib.set_value (ui->state.lanes [which].do_calib);
  //label_flip.create (ui, wp, "DC flip", 0, 0, 75, 32);
  //label_calib.create (ui, wp, "Calib.", 0, 0, 75, 32);
  label_frames.create (ui, wp, "(not loaded)", 0, 0, 75, 24);
  label_frames.textsize = 0.75;
  label_frames.align = TEXT_CENTER;
  label_trim.create (ui, wp, "1.0", 0, 0, 75, 24);
  label_trim.textsize = 0.75;
  label_trim.align = TEXT_CENTER;
  
  btn_browse.set_image_default (data_icon_folder_big_png);
  btn_clear.set_image_default (data_icon_x_big_png);
  btn_calib.set_image_default (data_icon_calib_big_png);
  btn_flip.set_image_default (data_icon_phase_big_png);
  
  if (ui && which < NB_NUM_MODELS) {
    ui->filepickers [which].create (ui, btn_browse.widget, which, "Select file");
    btn_browse.filepicker = &ui->filepickers [which];
    btn_browse.lane = which;
    ui->filepickers [which].lane = which;
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
  int knob_size = std::max (64, w / 10);
  knob_size = std::min (knob_size, (h * 2) / 3);
  const int knob_top = (h - knob_size) / 2 - 16;
  const int knob_right = w - knob_size * 2 - 12;
  const int menu_x = 16 + knob_size;//delay.x () + delay.w () + 8;
  const int menu_width = std::max (64, w - menu_x - (w - knob_right) - button_padding - 10);
  menu_list.move_resize (menu_x, 16, menu_width, 32);
  //int button_width = std::max (24, (menu_list.w () + button_padding) / 3 - button_padding);
  int button_left = menu_list.x ();
  int button_top = menu_list.y () + menu_list.h () + 8;
  int button_width = std::min (h - 68, w / 10);
  
  delay.move_resize (12, knob_top, knob_size, knob_size + 16);

  btn_browse.move_resize (button_left, button_top, button_width, button_width);
  btn_clear.move_resize (btn_browse.x () + btn_browse.w () + button_padding,
                         button_top, button_width, button_width);
  btn_calib.move_resize (btn_clear.x () + btn_browse.w () + button_padding,
                         button_top, button_width, button_width);
  btn_flip.move_resize (btn_calib.x () + btn_browse.w () + button_padding,
                         button_top, button_width, button_width);
                         
  int mute_x = btn_flip.x () + btn_calib.w () + button_padding;
  int mute_width = menu_list.x () + menu_list.w () - mute_x;
  btn_mute.move_resize (mute_x,
                         button_top, mute_width, button_width);
  btn_excl.move_resize (btn_mute.x (), btn_mute.y (), btn_mute.w (), btn_mute.h ());
  btn_mute.padding = btn_mute.h () / 4;
  
  int btnpadding = button_width / 5;
  btn_browse.padding = btnpadding;
  btn_clear.padding = btnpadding;
  btn_flip.padding = btnpadding;
  btn_calib.padding = btnpadding;
  
  gain_in.move_resize (knob_right, knob_top, knob_size, knob_size + 16);
  gain_out.move_resize (knob_right + knob_size + 1, knob_top, knob_size, knob_size + 16);
  meter_out.move_resize (w - 12, 16, 5, h - 28);
  
  int adv_btn_x = 84;
  int adv_btn_y = h * 2 / 11;
  label_frames.move_resize (delay.x (), h - 24, delay.w (), 16);
  label_trim.move_resize (gain_in.x (), h - 24, gain_in.w () + gain_out.w (), 16);
  //move_resize (x, y, w, h);
}

////////////////////////////////////////////////////////////////////////////////
// c_neuralblender_ui

c_neuralblender_ui::c_neuralblender_ui () { CP
  memset (&app, 0, sizeof (app));
  for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
    stats [i * 2] = 0.0f;
    stats [i * 2 + 1] = 1.0f;
  }
  display = NULL;
  window = 0;
  ui_ready = false;
}

c_neuralblender_ui::~c_neuralblender_ui () { CP
  destroy ();
}

void c_neuralblender_ui::update_cwd (std::string path) {
  CP
  debug ("path='%s'", path.c_str ());
  configfile.set_item (CONFIG_KEY_NAME_CWD, path_dirname (path));
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
  
  if (configfile.istrue (CONFIG_KEY_NAME_ADV)) {
    CP
    state.showadvanced = true;
  }

  if (configfile.istrue (CONFIG_KEY_NAME_CALIB)) {
    calib_default = true;
    for (i = 0; i < NB_NUM_MODELS && i < NB_NUM_MODELS; ++i)
      state.lanes [i].do_calib = true;
  }

  parent = parent_;
  if (!parent)
    parent = DefaultRootWindow (display);
    
  if (!mainwindow.create (this, parent, "NeuralBlender", 0, 0, 
                          DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT)) {
    fprintf (stderr, "Cant' create main window!\n");
    return false;
  }
  
  mainwindow.set_icon_from_png (data_neuralblender_logo_512_png);

  label_big.create (this, mainwindow.widget, "NeuralBlender", 120, 24, 400, 40);
  label_big.align = TEXT_CENTER;
  label_big.textsize = 1.5;
  
  btn_enable.create (this, mainwindow.widget, "ON/OFF",  20, 12, 120, 40, WSTYLE_IMAGE_TOGGLE);
  btn_enable.set_value (true);
  btn_enable.role = ROLE_BYPASS;
  btn_enable.set_image (data_icon_power_on_png, WSTATE_ON);
  btn_enable.set_image (data_icon_power_grey_png, WSTATE_OFF);
  //btn_enable.set_tooltip ("BYPASS SWITCH");
  btn_prefs.create (this, mainwindow.widget, "Settings", 520, 600, 100, 40);
  btn_prefs.role = ROLE_PREFS;
  btn_prefs.set_image (data_xputty_gear_png);
  btn_muteall.create (this, mainwindow.widget, "Mute all", 500, 12, 120, 40, WSTYLE_IMAGE_TOGGLE);
  btn_muteall.role = ROLE_MUTEALL;
  btn_muteall.set_image (data_icon_speaker_off_big_png, WSTATE_ON);
  btn_muteall.set_image (data_icon_speaker_on_big_png, WSTATE_OFF);
  
  cont_checkboxes.create (this, mainwindow.widget, "", 8, 600, 550, 40);
  cont_checkboxes.widget->scale.gravity = NONE;
  
  btn_linkcalib.create (this, cont_checkboxes.widget, "Linked calib.", 0, 0, 180, 32, WSTYLE_CHECKBOX);
  btn_linkcalib.role = ROLE_LINKED_CALIB;
  btn_linkcalib.set_value (prefs.linked_calib);
  btn_linkcalib.set_tooltip ("Calibrate all models by same amount (loudest model / lowest trim)");

  btn_exclmode.create (this, cont_checkboxes.widget, "Exclusive mode", 160, 0, 180, 32, WSTYLE_CHECKBOX);
  btn_exclmode.set_value (state.do_excl);
  btn_exclmode.role = ROLE_EXCL_TOGGLE;
  btn_exclmode.set_tooltip ("Allow only one model active, seamlessly switch between them");

  btn_bass.create (this, cont_checkboxes.widget, "Bass", 350, 0, 180, 32, WSTYLE_CHECKBOX);
  btn_bass.role = ROLE_CALIBBASS;
  btn_bass.set_tooltip ("Calibrate trim levels for bass guitar");
  
  aboutwindow.create (this);
  prefswindow.create (this);
  
  for (i = 0; i < NB_NUM_MODELS; i++) {
    lanes [i].create (this, mainwindow.widget, i, 0, 0, DEFAULT_WINDOW_WIDTH, 130);
  }
  meter_in.create (this, mainwindow.widget, "", 6, 70, 5, 520);
  meter_in.set_vudata (&vudata_in);
  meter_in.set_stereo (false);
  vudata_in.set_l (0.0, 0.0);

  if (blender) {
    for (i = 0; i < NB_NUM_MODELS; i++) {
      blender->meters_out [i] = &lanes [i].vudata_out;
    }
    blender->meter_in = &vudata_in;
  }

  apply_ui_prefs (prefs);
  
  //if (state.showadvanced) {
  //  show_advanced_settings ();
  //} else {
  //  hide_advanced_settings ();
  //}
  
  mainwindow.show ();
  ui_ready = true;
  move_resize ();
  CP
  XFlush (display);
  CP
  //do_set_min_size = true;
  return true;
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
      if (lane >= 0 && lane < NB_NUM_MODELS) {
        state.lanes [lane].lane_mute = value;
      }
    break;

    case ROLE_DCFLIP: CP
      on_dcflip (btn, value);
      if (lane >= 0 && lane < NB_NUM_MODELS) {
        state.lanes [lane].dcflip = value;
      }
    break;

    case ROLE_CALIBRATE: CP
      if (lane >= 0 && lane < NB_NUM_MODELS) {
        state.lanes [lane].do_calib = value;
      }
      write_calib_state_if_consistent ();
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

    case ROLE_PREFSOK: CP
      prefswindow.set_prefs_to (prefs);
      apply_prefs (prefs);
      on_prefs_ok ();
      prefswindow.hide ();
    break;
    
    case ROLE_PREFSDEFAULTS: CP
      prefswindow.load_defaults ();
    break;

    case ROLE_PREFSCANCEL: CP
      prefswindow.hide ();
    break;

    case ROLE_VUTOGGLE: CP
      vu_on (value);
    break;

    case ROLE_LINKED_CALIB: CP
      prefs.linked_calib = value;
      on_linked_calib (btn, value);
    break;

    case ROLE_CALIBBASS: CP
      prefs.calib_source = value ? 1 : 0;
      on_calib_bass (btn, value);
    break;

    case ROLE_EXCL_TOGGLE: CP
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

void c_neuralblender_ui::on_about () { CP }

void c_neuralblender_ui::on_prefs () {
}

void c_neuralblender_ui::on_prefs_ok () {
}

void c_neuralblender_ui::apply_ui_prefs (t_prefs &p) { CP
  const float scale_db = p.vu_scale_db <= 0.0f ? p.vu_scale_db : DEFAULT_VU_DB;
  const float headroom_db = std::clamp (p.vu_headroom_db, 0.0f, 12.0f);

  meter_in.set_db_scale (scale_db);
  meter_in.set_headroom (headroom_db);
  vudata_in.set_db_scale (scale_db);
  vudata_in.set_headroom (headroom_db);

  for (size_t i = 0; i < NB_NUM_MODELS; i++) {
    lanes [i].meter_out.set_db_scale (scale_db);
    lanes [i].meter_out.set_headroom (headroom_db);
    lanes [i].vudata_out.set_db_scale (scale_db);
    lanes [i].vudata_out.set_headroom (headroom_db);
  }

  btn_bass.set_value (p.calib_source == 1);
  if (prefswindow.widget)
    prefswindow.btn_bass.set_value (p.calib_source == 1);

  vu_on (p.vu_on);
}

void c_neuralblender_ui::apply_prefs (t_prefs &p) { CP
  apply_ui_prefs (p);
  write_prefs_to_config (configfile, p);
}

void c_neuralblender_ui::write_prefs_to (t_prefs &p) { CP
  p.vu_on = state.do_vu;
}

void c_neuralblender_ui::move_resize (bool snap_to_default) {
  CP
  //state.showadvanced = b;
  
  if (!ui_resize_lock && mainwindow.widget) {
    Widget_t *mw = mainwindow.widget;
    Metrics_t metrics;
    os_get_window_metrics (mw, &metrics);
    ui_resize_lock = true;
    
    int window_width = DEFAULT_WINDOW_WIDTH;
    int window_height = DEFAULT_WINDOW_HEIGHT;
    if (!snap_to_default && metrics.visible) {
      window_width = std::max (MIN_WINDOW_WIDTH, (int) (metrics.width / mw->app->hdpi));
      window_height = std::max (MIN_WINDOW_HEIGHT, (int) (metrics.height / mw->app->hdpi));
    }
    
    //if (do_set_min_size)
    mainwindow.set_min_size (MIN_WINDOW_WIDTH, MIN_WINDOW_HEIGHT);
    
    int lane_width = window_width - 32;
    const int lane_top = 60;
    const int lane_gap = 12;
    const int bottom_reserve = 56;
    const int lane_count = NB_NUM_MODELS;
    const int total_gap = (lane_count > 1) ? (lane_count - 1) * lane_gap : 0;
    const int lane_area = window_height - lane_top - bottom_reserve - total_gap;
    int lane_height = std::max (1, lane_area / lane_count);
    
    debug ("window w/h %d,%d", window_width, window_height);
    
    cont_checkboxes.move_resize (16, window_height - 44, 450, 40);
    
    btn_enable.move_resize (16, 12, 120, 40);
    btn_muteall.move_resize (window_width - 136, 12, 120, 40);
    btn_prefs.move_resize (btn_muteall.x (), window_height - 48, 120, 40);
    //label_exclmode.set_label ("Exclusive mode");
    label_big.move_resize (150, 8, window_width - 300, 48);
    
    size_t i;
    for (i = 0; i < NB_NUM_MODELS; i++) {
      lanes [i].move_resize (16, lane_top + i * (lane_height + lane_gap), lane_width, lane_height);
    }
    
    const int lane_bottom = lane_top + lane_count * lane_height + total_gap;
    meter_in.move_resize (5, lane_top + 4, 5, std::max (1, lane_bottom - lane_top - 8));
    
    ui_resize_lock = false;
  }
}

// called from lv2_ui - runs in UI thread
void c_neuralblender_ui::update_stats () {
  char buf [128];
  
  for (size_t i = 0; i < NB_NUM_MODELS; i++) {
    int nframes = stats [i * 2];
    float trim = stats [i * 2 + 1];
    
    /*if (trim != 1.0f) {
      snprintf (buf, 127, "%d frames, trim=%.02f", nframes, trim);
    } else {
      snprintf (buf, 127, "%d frames", nframes);
    } */
    
    snprintf (buf, 127, "%d frames", nframes);
    lanes [i].label_frames.set_label (buf);
    if (trim == 1.00) {
      lanes [i].label_trim.set_label ("");
    } else {
      float db = gain_to_db (trim);
      snprintf (buf, 127, "Trim: %s%.02fdB", db > 0.0 ? "+" : "", db);
      lanes [i].label_trim.set_label (buf);
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

  meter_in.show ();
  for (size_t i = 0; i < NB_NUM_MODELS; i++) {
    lanes [i].meter_out.show ();
  }
  //on_vu (&btn_vu, b);
}

void c_neuralblender_ui::vu_off () { CP
  state.do_vu = false;

  meter_in.hide ();
  for (size_t i = 0; i < NB_NUM_MODELS; i++) {
    lanes [i].meter_out.hide ();
  }
  //on_vu (&btn_vu, false);
}

size_t c_neuralblender_ui::choose_exclusive_lane () const {
  if (state.exclusive_lane > 0 &&
      state.exclusive_lane <= (int) NB_NUM_MODELS)
    return (size_t) state.exclusive_lane;

  if (last_exclusive_lane > 0 && last_exclusive_lane <= NB_NUM_MODELS)
    return last_exclusive_lane;

  for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
    if (!state.lanes [i].filename.empty () &&
        !state.lanes [i].lane_mute)
      return i + 1;
  }

  for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
    //if (!filepickers [i].selected_file.empty ())
    if (!state.lanes [i].filename.empty ())
      return i + 1;
  }

  for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
    if (!state.lanes [i].lane_mute)
      return i + 1;
  }

  return 1;
}

void c_neuralblender_ui::on_window_resize (int w, int h) {
  if (ui_ready && !ui_resize_lock)
    move_resize ();
}

bool c_neuralblender_ui::request_window_size (int w, int h) {
  return mainwindow.request_size (w, h);
}

void c_neuralblender_ui::on_excl (c_widget *w, int n) {
  debug ("n=%d", n);
  state.exclusive_lane = n;
  if (n > 0 && n <= (int) NB_NUM_MODELS)
    last_exclusive_lane = (size_t) n;
  if (!w)
    return;

  btn_exclmode.set_value (state.exclusive_lane != 0);
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

  if (state.do_vu) {
    meter_in.on_ui_timer ();
    for (int i = 0; i < NB_NUM_MODELS; i++) {
      lanes [i].meter_out.on_ui_timer ();
    }
  }
  run_embedded (&app);
  return 0;
}

void c_neuralblender_ui::draw () {
  if (!mainwindow.widget)
    return;

  widget_draw (mainwindow.widget, NULL);
}

void c_neuralblender_ui::clear_lane_model_ui (size_t which) {
  if (which >= NB_NUM_MODELS)
    return;

  //filepickers [which].selected_file.clear ();
  state.lanes [which].filename.clear ();
  lanes [which].menu_list.clear ();
}

void c_neuralblender_ui::set_lane_mute (size_t which, bool b) {
  debug ("which=%d, b=%d", (int) which, (int) b);
  if (which >= NB_NUM_MODELS)
    return;

  const bool old_updating = updating_from_state;
  updating_from_state = true;

  lanes [which].btn_mute.value = b;
  lanes [which].btn_mute.set_value (b);

  updating_from_state = old_updating;
}

void c_neuralblender_ui::apply_effective_controls () {
}

// calibration default is written to config ONLY if all
// calib check boxes are on/off
void c_neuralblender_ui::write_calib_state_if_consistent () {
  bool all_on = true;
  bool all_off = true;

  for (size_t i = 0; i < NB_NUM_MODELS && i < NB_NUM_MODELS; ++i) {
    all_on  &= state.lanes [i].do_calib;
    all_off &= !state.lanes [i].do_calib;
  }

  if (!all_on && !all_off)
    return;

  configfile.set_item (CONFIG_KEY_NAME_CALIB, all_on ? "1" : "0");
  configfile.write_file();
}

void c_neuralblender_ui::sync_widgets_from_state (const c_neuralblender_state &state_,
                                                  bool scan_dirs) {
  if (!ui_ready)
    return;
  const bool showadvanced = state.showadvanced;
  this->state = state_;
  this->state.showadvanced = showadvanced;
  if (state.exclusive_lane > 0 &&
      state.exclusive_lane <= (int) NB_NUM_MODELS)
    last_exclusive_lane = (size_t) state.exclusive_lane;

  updating_from_state = true;

  const size_t nlanes = NB_NUM_MODELS < NB_NUM_MODELS ? NB_NUM_MODELS : NB_NUM_MODELS;
  for (size_t i = 0; i < nlanes; ++i) {
    const c_neuralblender_lane_state &lane = state.lanes [i];

    lanes [i].gain_in.set_value (gain_to_db (lane.gain_in));
    lanes [i].gain_out.set_value (gain_to_db (lane.gain_out));
    lanes [i].delay.set_value (lane.delay_ms);
    lanes [i].btn_flip.set_value (lane.dcflip);
    lanes [i].btn_calib.set_value (lane.do_calib);

    //filepickers [i].selected_file = lane.filename;
    if (scan_dirs) {
      if (state.lanes [i].filename.empty ()) {
        lanes [i].menu_list.clear ();
      } else {
        filepickers [i].current_dir = path_dirname (state.lanes [i].filename);
        filepickers [i].scan_current_dir ();
        filepickers [i].add_files_from_dir (&lanes [i].menu_list);
      }
    }
    update_stats ();
  }

  const bool enabled = !state.bypass;
  btn_enable.set_value (enabled);
  //btn_enable.set_label (enabled ? "Enabled" : " Bypass ");

  /*btn_advanced.set_value (state.showadvanced);
  show_advanced_settings (state.showadvanced);*/

  btn_bass.set_value (prefs.calib_source == 0 ? false : true);
  btn_linkcalib.set_value (prefs.linked_calib);
  if (state.do_vu) {
    meter_in.show ();
    for (size_t i = 0; i < NB_NUM_MODELS; ++i)
      lanes [i].meter_out.show ();
  } else {
    meter_in.hide ();
    for (size_t i = 0; i < NB_NUM_MODELS; ++i)
      lanes [i].meter_out.hide ();
  }

  btn_exclmode.set_value (state.exclusive_lane > 0);

  const bool exclusive_on = state.exclusive_lane > 0;
  for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
    const bool selected =
      exclusive_on && state.exclusive_lane == i + 1;

    lanes [i].btn_mute.set_value (state.lanes [i].lane_mute);
    lanes [i].btn_excl.set_value (selected);

    if (state.lanes [i].lane_mute || state.mute_all || !enabled) { CP
      lanes [i].lane_widget.set_state (WSTATE_DISABLED);
    } else {
      lanes [i].lane_widget.set_state (WSTATE_NORMAL);
    }

    if (exclusive_on) { CP
      lanes [i].lane_widget.set_state (WSTATE_DISABLED);
      widget_hide (lanes [i].btn_mute.widget);
      widget_show (lanes [i].btn_excl.widget);
    } else { CP
      widget_show (lanes [i].btn_mute.widget);
      widget_hide (lanes [i].btn_excl.widget);
    }
  }
  if (exclusive_on && !state.mute_all && enabled) {
    //lanes [state.exclusive_lane - 1].lane_widget.set_fg_color (0.1, 0.4, 0.4);
    lanes [state.exclusive_lane - 1].lane_widget.set_state (WSTATE_SELECTED);
  }

  updating_from_state = false;
}
