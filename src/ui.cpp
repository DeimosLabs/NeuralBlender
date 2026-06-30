
/* NeuralBlender - RTNeural / NAM based amp modeler
 *
 * Shared UI code
 */

#include <string.h>
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>

#include "neuralblender.h"
#include "ui.h"

#include "data/data.h"

#define CMDLINE_DEBUG_COLOR ANSI_MAGENTA
#include "cmdline/cmdline_debug.h"

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

// calibration default is written to config ONLY if all
// calib check boxes are on/off
void c_neuralblender_ui::write_calib_state_if_consistent () {
  bool all_on = true;
  bool all_off = true;

  for (size_t i = 0; i < NB_UI_MAX_LANES && i < NB_MAX_MODELS; ++i) {
    all_on  &= state.lanes[i].do_calib;
    all_off &= !state.lanes[i].do_calib;
  }

  if (!all_on && !all_off)
    return;

  configfile.set_item(CONFIG_KEY_NAME_CALIB, all_on ? "1" : "0");
  configfile.write_file();
}

void c_aboutwindow::create (c_neuralblender_ui *ui_) { CP
  ui = ui_;
  if (!ui || !ui->ui_ready || w)
    return;

  w = create_window (&ui->app, os_get_root_window (&ui->app, IS_WINDOW), 0, 0, 400, 450);
  if (!w)
    return;
    
  w->flags |= HIDE_ON_DELETE;
  os_set_transient_for_hint (ui->main_widget, w);

  w->func.expose_callback = cb_draw_main_window;
  set_x11_window_background (w, get_colortheme ()->window_bg);
  widget_set_title (w, "About NeuralBlender");
  btn_ok.create (ui, w, "OK", 160, 400, 80, 40);
  btn_ok.role = ROLE_ABOUTOK;

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
    labels [i].create (ui, w, text [i], 0, h, 400, 24);
    labels [i].align = TEXT_CENTER;
  }

  char buf [64];
  snprintf (buf, 63, "Build timestamp: %s", g_build_timestamp);

  labels [0].textsize = 1.5;
  labels [i].create (ui, w, buf, 0, 360, 400, 20);
  labels [i].textsize = 0.75;
  labels [i].align = TEXT_CENTER;

  img_logo.create (ui, w, "", (400-160)/2, 64, 160, 160);
  img_logo.set_png (data_neuralblender_logo_160_png);
}

void c_aboutwindow::show () { CP
  if (!w)
    create (ui);
  if (!w)
    return;

  widget_show_all (w);
  expose_widget (w);
}

void c_aboutwindow::hide () { CP
  if (!w)
    return;

  widget_hide (w);
}

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
  
  btn_browse.create (ui, wp, "Load", 0, 0, 100, 40, WSTYLE_IMAGE_BUTTON);
  btn_clear.create  (ui, wp, "Clear",     0, 0, 100, 40, WSTYLE_IMAGE_BUTTON);
  btn_excl.create   (ui, wp, "Use",       0, 0, 100, 40, WSTYLE_TOGGLE);
  btn_mute.create   (ui, wp, "Mute",      0, 0, 100, 40, WSTYLE_TOGGLE);
  btn_mute.set_value (false);
  btn_browse.lane = which;
  btn_clear.lane = which;
  btn_mute.lane = which;
  btn_browse.role = ROLE_BROWSE;
  btn_clear.role = ROLE_CLEAR;
  btn_mute.role = ROLE_MUTE;
  btn_excl.role = ROLE_EXCL_USE;
  btn_excl.lane = which;
  
  // advanced controls
  delay.role = ROLE_DELAY;
  delay.create (ui, wp, "Delay", 0, 0, 64, 64);
  delay.set_min (0);
  delay.set_max (30);
  delay.set_defaultvalue (0);
  delay.set_value (0);
  delay.set_step (0.01);
  delay.role = ROLE_DELAY;

  btn_flip.create   (ui, wp, "DC flip", 0, 0, 32, 32, WSTYLE_IMAGE_TOGGLE);
  btn_calib.create   (ui, wp, "Calib", 0, 0, 32, 32, WSTYLE_IMAGE_TOGGLE);
  btn_flip.role = ROLE_DCFLIP;
  btn_calib.role = ROLE_CALIBRATE;
  btn_flip.lane = which;
  btn_calib.lane = which;
  if (ui && which < NB_UI_MAX_LANES)
    btn_calib.set_value (ui->state.lanes [which].do_calib);
  //label_flip.create (ui, wp, "DC flip", 0, 0, 75, 32);
  //label_calib.create (ui, wp, "Calib.", 0, 0, 75, 32);
  label_frames.create (ui, wp, "(not loaded)", 0, 0, 75, 24);
  label_frames.textsize = 0.75;
  label_trim.create (ui, wp, "1.0", 0, 0, 75, 24);
  label_trim.textsize = 0.75;

  btn_browse.set_image_on (data_icon_folder_small_png);
  btn_browse.set_image_off (data_icon_folder_small_png);
  btn_browse.set_image_hover (data_icon_folder_small_png);
  btn_browse.set_image_down (data_icon_folder_small_png);
  btn_browse.set_image_down_hover (data_icon_folder_small_png);
  btn_browse.set_image_off_hover (data_icon_folder_small_png);
  
  btn_clear.set_image_on (data_icon_x_small_png);
  btn_clear.set_image_off (data_icon_x_small_png);
  btn_clear.set_image_hover (data_icon_x_small_png);
  btn_clear.set_image_down (data_icon_x_small_png);
  btn_clear.set_image_down_hover (data_icon_x_small_png);
  btn_clear.set_image_off_hover (data_icon_x_small_png);
  
  btn_calib.set_image_on (data_icon_calib_small_png);
  btn_calib.set_image_off (data_icon_calib_small_png);
  btn_calib.set_image_hover (data_icon_calib_small_png);
  btn_calib.set_image_down (data_icon_calib_small_png);
  btn_calib.set_image_down_hover (data_icon_calib_small_png);
  btn_calib.set_image_off_hover (data_icon_calib_small_png);
  
  btn_flip.set_image_on (data_icon_phase_small_png);
  btn_flip.set_image_off (data_icon_phase_small_png);
  btn_flip.set_image_hover (data_icon_phase_small_png);
  btn_flip.set_image_down (data_icon_phase_small_png);
  btn_flip.set_image_down_hover (data_icon_phase_small_png);
  btn_flip.set_image_off_hover (data_icon_phase_small_png);
  
  if (ui && which < NB_UI_MAX_LANES) {
    ui->filepickers [which].create (ui, btn_browse.widget, which, "Select file");
    btn_browse.filepicker = &ui->filepickers [which];
    btn_browse.lane = which;
    ui->filepickers [which].lane = which;
  }
  
}

void c_neuralblender_ui::on_button (c_button *btn, bool value) {
  if (!btn || updating_from_state)
    return;
    
  size_t lane = btn->lane;

  switch (btn->role) {
    case ROLE_BYPASS: CP
      on_bypass (btn, value);
      btn->set_label (value ? "Enabled" : "Bypass");
    break;

    case ROLE_MUTE: CP
      on_mute (btn, value);
      if (lane >= 0 && lane < NB_UI_MAX_LANES) {
        state.lanes [lane].lane_mute = value;
      }
    break;

    case ROLE_DCFLIP: CP
      on_dcflip (btn, value);
      if (lane >= 0 && lane < NB_UI_MAX_LANES) {
        state.lanes [lane].dcflip = value;
      }
    break;

    case ROLE_CALIBRATE: CP
      if (lane >= 0 && lane < NB_UI_MAX_LANES) {
        state.lanes [lane].do_calib = value;
      }
      write_calib_state_if_consistent ();
      on_calibrate (btn, value);
    break;

    case ROLE_MUTEALL: CP
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
      on_about (btn);
    break;

    case ROLE_ABOUTOK: CP
      aboutwindow.hide ();
    break;

    case ROLE_VUTOGGLE: CP
      vu_on (value);
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

void c_lane_widgets::move_resize (
    int x, int y, int w, int h) {
  
  if (!main_widget)
    return;

  lane_widget.move_resize (x, y, w, h);

  int split = 0;
  
  cont_regcontrols.move_resize (0, 0, w, h);
  
  int button_padding = 4;
  
  const int knob_size = 64;//std::max (64, h / 2);
  const int knob_top = (h - knob_size) / 2 - 8;
  const int knob_right = std::max (16, w - 160);
  const int reg_w = cont_regcontrols.w ();
  const int menu_x = 16 + knob_size;//delay.x () + delay.w () + 8;
  const int menu_width = std::max (64, w - menu_x - (w - knob_right) - button_padding);
  menu_list.move_resize (menu_x, 24, menu_width, 32);
  //int button_width = std::max (24, (menu_list.w () + button_padding) / 3 - button_padding);
  int button_left = menu_list.x ();
  int button_top = menu_list.y () + menu_list.h () + 8;
  int button_width = std::min (h - 76, w / 10);
  
  delay.move_resize (16, knob_top, knob_size, knob_size + 16);

  btn_browse.move_resize (button_left, button_top, button_width, button_width);
  btn_clear.move_resize (btn_browse.x () + btn_browse.w () + button_padding,
                         button_top, button_width, button_width);
  btn_flip.move_resize (btn_clear.x () + btn_browse.w () + button_padding,
                         button_top, button_width, button_width);
  btn_calib.move_resize (btn_flip.x () + btn_browse.w () + button_padding,
                         button_top, button_width, button_width);
                         
  int mute_x = btn_calib.x () + btn_calib.w () + button_padding;
  int mute_width = menu_list.x () + menu_list.w () - mute_x;
  btn_mute.move_resize (mute_x,
                         button_top, mute_width, button_width);
  btn_excl.move_resize (btn_mute.x (), btn_mute.y (), btn_mute.w (), btn_mute.h ());
  
  gain_in.move_resize (knob_right, knob_top, knob_size, knob_size + 16);
  gain_out.move_resize (knob_right + knob_size + 1, knob_top, knob_size, knob_size + 16);
  meter_out.move_resize (knob_right + 130, 16, 5, h - 32);
  
  int adv_btn_x = 84;
  int adv_btn_y = h * 2 / 11;
  label_frames.move_resize (delay.x (), h - 25, delay.w (), 16);
  label_trim.move_resize (gain_in.x (), h - 25, gain_in.w (), 16);
  //move_resize (x, y, w, h);
}

c_neuralblender_ui::c_neuralblender_ui () { CP
  memset (&app, 0, sizeof (app));
  for (size_t i = 0; i < NB_UI_MAX_LANES; ++i) {
    stats [i * 2] = 0.0f;
    stats [i * 2 + 1] = 1.0f;
  }
  display = NULL;
  window = 0;
  main_widget = NULL;
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
  
  std::string configitem;
  
  configfile.read_file ();
  
  if (configfile.istrue (CONFIG_KEY_NAME_ADV)) {
    CP
    state.showadvanced = true;
  }

  if (configfile.istrue (CONFIG_KEY_NAME_CALIB)) {
    calib_default = true;
    for (i = 0; i < NB_UI_MAX_LANES && i < NB_MAX_MODELS; ++i)
      state.lanes [i].do_calib = true;
  }

  parent = parent_;
  if (!parent)
    parent = DefaultRootWindow (display);

  main_widget = create_window (&app, parent, 0, 0, 640, 640);
  if (!main_widget)
    return false;
  
  main_widget->parent_struct = this;
  main_widget->func.resize_notify_callback = main_notify_callback;
  main_widget->scale.gravity = NONE;
  widget_set_icon_from_png (main_widget, data_neuralblender_logo_512_png);

  os_register_wm_delete_window (main_widget);
  widget_set_title (main_widget, "NeuralBlender");
  main_widget->func.expose_callback = cb_draw_main_window;
  set_x11_window_background (main_widget, get_colortheme ()->window_bg);
  label_big.create (this, main_widget, "NeuralBlender", 120, 24, 400, 40);
  label_big.align = TEXT_CENTER;
  label_big.textsize = 1.5;
  
  btn_enable.create (this, main_widget, "Enabled",  20, 12, 120, 40, WSTYLE_TOGGLE);
  btn_enable.set_image_on (data_xputty_approved_png);
  btn_enable.set_image_hover (data_xputty_exit_png);
  btn_enable.set_image_down_hover (data_xputty_eject_png);
  btn_enable.set_image_down (data_xputty_error_png);
  btn_enable.set_image_off (data_xputty_gear_png);
  btn_enable.set_image_off_hover (data_xputty_question_png);
  btn_enable.set_value (true);
  btn_enable.role = ROLE_BYPASS;
  btn_about.create (this, main_widget, "About...", 520, 600, 100, 40);
  btn_about.role = ROLE_ABOUT;
  btn_muteall.create (this, main_widget, "Mute all", 500, 12, 120, 40, WSTYLE_TOGGLE);
  btn_muteall.role = ROLE_MUTEALL;
  
  cont_checkboxes.create (this, main_widget, "", 8, 604, 500, 40);
  cont_checkboxes.widget->scale.gravity = NONE;
  
  btn_vu.create (this, cont_checkboxes.widget, "VU meters", 0, 0, 120, 32, WSTYLE_CHECKBOX);
  btn_vu.role = ROLE_VUTOGGLE;
  btn_vu.set_value (state.do_vu);
  
  //btn_advanced.create (this, cont_checkboxes.widget, "Adv.", 140, 0, 32, 32, WSTYLE_CHECKBOX);
  //label_advanced.create (this, cont_checkboxes.widget, "Show advanced", 172, 0, 150, 32);
  //btn_advanced.set_value (state.showadvanced);
  //btn_advanced.role = ROLE_ADV_TOGGLE;
  //label_advanced.widget->scale.gravity = WESTCENTER;
  
  btn_exclmode.create (this, cont_checkboxes.widget, "Exclusive mode", 140, 0, 170, 32, WSTYLE_CHECKBOX);
  btn_exclmode.set_value (state.do_excl);
  btn_exclmode.role = ROLE_EXCL_TOGGLE;
  
  aboutwindow.create (this);
  for (i = 0; i < NB_UI_MAX_LANES; i++) {
    lanes [i].create (this, main_widget, i, 0, 0, 640, 130);
  }
  meter_in.create (this, main_widget, "", 6, 70, 5, 520);
  meter_in.set_vudata (&vudata_in);
  meter_in.set_stereo (false);
  vudata_in.set_l (0.0, 0.0);

  if (blender) {
    for (i = 0; i < NB_UI_MAX_LANES; i++) {
      blender->meters_out [i] = &lanes [i].vudata_out;
    }
    blender->meter_in = &vudata_in;
  }
  
  //if (state.showadvanced) {
  //  show_advanced_settings ();
  //} else {
  //  hide_advanced_settings ();
  //}
  
  reposition_widgets ();

  widget_show_all (main_widget);
  widget_draw (main_widget, NULL);

  window = main_widget->widget;
  ui_ready = true;
  CP
  XFlush (display);
  CP
  return true;
}

void c_neuralblender_ui::destroy () { CP
  if (ui_ready)
    main_quit (&app);

  memset (&app, 0, sizeof (app));
  display = NULL;
  window = 0;
  main_widget = NULL;
  ui_ready = false;
}

void c_neuralblender_ui::reposition_widgets (bool snap_to_default) {
  CP
  //state.showadvanced = b;
  
  if (!ui_resize_lock) {
    Metrics_t metrics;
    os_get_window_metrics (main_widget, &metrics);
    ui_resize_lock = true;
    
    int min_window_width = 640;
    int min_window_height = 640;
    int window_width = min_window_width;
    int window_height = min_window_height;
    if (!snap_to_default && metrics.visible) {
      window_width = std::max (min_window_width, (int) (metrics.width / main_widget->app->hdpi));
      window_height = std::max (min_window_height, (int) (metrics.height / main_widget->app->hdpi));
    }
    
    os_set_window_min_size (main_widget, min_window_width, min_window_height,
                            min_window_width, min_window_height);
    int lane_width = window_width - 24;
    //int lane_height = 130;
    int lane_height = window_height / 5;
    
    debug ("window w/h %d,%d", window_width, window_height);
    //main_widget->func.configure_callback (main_widget, NULL);
    
    cont_checkboxes.move_resize (16, window_height - 44, 450, 40);
    
    btn_enable.move_resize (16, 12, 120, 40);
    btn_muteall.move_resize (window_width - 136, 12, 120, 40);
    btn_about.move_resize (btn_muteall.x (), window_height - 50, 120, 40);
    //label_exclmode.set_label ("Exclusive mode");
    label_big.move_resize (150, 8, window_width - 300, 48);
    
    size_t i;
    for (i = 0; i < NB_UI_MAX_LANES; i++) {
      lanes [i].move_resize (12, 60 + i * (lane_height + 5), lane_width, lane_height);
    }
    meter_in.move_resize (4, 64, 5, (lane_height + 5) * i - 12);
    
    ui_resize_lock = false;
  }
}

// called from lv2_ui - runs in UI thread
void c_neuralblender_ui::update_stats () {
  char buf [128];
  
  for (size_t i = 0; i < NB_UI_MAX_LANES; i++) {
    int nframes = stats [i * 2];
    float trim = stats [i * 2 + 1];
    
    /*if (trim != 1.0f) {
      snprintf (buf, 127, "%d frames, trim=%.02f", nframes, trim);
    } else {
      snprintf (buf, 127, "%d frames", nframes);
    } */
    
    snprintf (buf, 127, "%d frames", nframes);
    lanes [i].label_frames.set_label (buf);
    if (trim == 1.00)
      snprintf (buf, 127, "");
    else
      snprintf (buf, 127, "Trim: %.02f", trim);
    lanes [i].label_trim.set_label (buf);
    
  }
}

/*void c_neuralblender_ui::show_advanced_settings (bool b) {
  //show_advanced = b;
  state.showadvanced = b;
  reposition_widgets (true);
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
  for (size_t i = 0; i < NB_UI_MAX_LANES; i++) {
    lanes [i].meter_out.show ();
  }
  on_vu (&btn_vu, b);
}

void c_neuralblender_ui::vu_off () { CP
  state.do_vu = false;

  meter_in.hide ();
  for (size_t i = 0; i < NB_UI_MAX_LANES; i++) {
    lanes [i].meter_out.hide ();
  }
  on_vu (&btn_vu, false);
}

size_t c_neuralblender_ui::choose_exclusive_lane () const {
  if (state.exclusive_lane > 0 &&
      state.exclusive_lane <= (int) NB_UI_MAX_LANES)
    return (size_t) state.exclusive_lane;

  if (last_exclusive_lane > 0 && last_exclusive_lane <= NB_UI_MAX_LANES)
    return last_exclusive_lane;

  for (size_t i = 0; i < NB_UI_MAX_LANES; ++i) {
    if (!state.lanes [i].filename.empty () &&
        !state.lanes [i].lane_mute)
      return i + 1;
  }

  for (size_t i = 0; i < NB_UI_MAX_LANES; ++i) {
    //if (!filepickers [i].selected_file.empty ())
    if (!state.lanes [i].filename.empty ())
      return i + 1;
  }

  for (size_t i = 0; i < NB_UI_MAX_LANES; ++i) {
    if (!state.lanes [i].lane_mute)
      return i + 1;
  }

  return 1;
}

void c_neuralblender_ui::on_window_resize (int w, int h) {
  if (ui_ready && !ui_resize_lock)
    reposition_widgets ();
}

bool c_neuralblender_ui::request_window_size (int w, int h) {
  if (!main_widget || !display)
    return false;

  os_resize_window (display, main_widget, w, h);
  return true;
}

void c_neuralblender_ui::on_excl (c_widget *w, int n) {
  debug ("n=%d", n);
  state.exclusive_lane = n;
  if (n > 0 && n <= (int) NB_UI_MAX_LANES)
    last_exclusive_lane = (size_t) n;
  if (!w)
    return;

  btn_exclmode.set_value (state.exclusive_lane != 0);
  apply_effective_controls();
  sync_widgets_from_state (state);
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
    for (int i = 0; i < NB_MAX_MODELS; i++) {
      lanes [i].meter_out.on_ui_timer ();
    }
  }
  run_embedded (&app);
  return 0;
}

void c_neuralblender_ui::draw () {
  if (!main_widget)
    return;

  widget_draw (main_widget, NULL);
}

void c_neuralblender_ui::clear_lane_model_ui (size_t which) {
  if (which >= NB_UI_MAX_LANES)
    return;

  //filepickers [which].selected_file.clear ();
  state.lanes [which].filename.clear ();
  lanes [which].menu_list.clear ();
}

void c_neuralblender_ui::set_lane_mute (size_t which, bool b) {
  debug ("which=%d, b=%d", (int) which, (int) b);
  if (which >= NB_UI_MAX_LANES)
    return;

  const bool old_updating = updating_from_state;
  updating_from_state = true;

  lanes [which].btn_mute.value = b;
  lanes [which].btn_mute.set_value (b);

  updating_from_state = old_updating;
}

void c_neuralblender_ui::apply_effective_controls () {
}

void c_neuralblender_ui::sync_widgets_from_state (const c_neuralblender_state &state_) {
  if (!ui_ready)
    return;
  const bool showadvanced = state.showadvanced;
  this->state = state_;
  this->state.showadvanced = showadvanced;
  if (state.exclusive_lane > 0 &&
      state.exclusive_lane <= (int) NB_UI_MAX_LANES)
    last_exclusive_lane = (size_t) state.exclusive_lane;

  updating_from_state = true;

  const size_t nlanes = NB_UI_MAX_LANES < NB_MAX_MODELS ? NB_UI_MAX_LANES : NB_MAX_MODELS;
  for (size_t i = 0; i < nlanes; ++i) {
    const c_neuralblender_lane_state &lane = state.lanes [i];

    lanes [i].gain_in.set_value (gain_to_db (lane.gain_in));
    lanes [i].gain_out.set_value (gain_to_db (lane.gain_out));
    lanes [i].delay.set_value (lane.delay_ms);
    lanes [i].btn_flip.set_value (lane.dcflip);
    lanes [i].btn_calib.set_value (lane.do_calib);

    //filepickers [i].selected_file = lane.filename;
    if (state.lanes [i].filename.empty ()) {
      lanes [i].menu_list.clear ();
    } else {
      filepickers [i].current_dir = path_dirname (state.lanes [i].filename);
      filepickers [i].scan_current_dir ();
      filepickers [i].add_files_from_dir (&lanes [i].menu_list);
    }
    update_stats ();
  }

  const bool enabled = !state.bypass;
  btn_enable.set_value (enabled);
  btn_enable.set_label (enabled ? "Enabled" : "Bypass");

  /*btn_advanced.set_value (state.showadvanced);
  show_advanced_settings (state.showadvanced);*/

  btn_vu.set_value (state.do_vu);
  if (state.do_vu) {
    meter_in.show ();
    for (size_t i = 0; i < NB_UI_MAX_LANES; ++i)
      lanes [i].meter_out.show ();
  } else {
    meter_in.hide ();
    for (size_t i = 0; i < NB_UI_MAX_LANES; ++i)
      lanes [i].meter_out.hide ();
  }

  btn_exclmode.set_value (state.exclusive_lane > 0);

  const bool exclusive_on = state.exclusive_lane > 0;
  for (size_t i = 0; i < NB_UI_MAX_LANES; ++i) {
    const bool selected =
      exclusive_on && state.exclusive_lane == i + 1;

    lanes [i].btn_mute.set_value (state.lanes [i].lane_mute);
    lanes [i].btn_excl.set_value (selected);

    for (size_t i = 0; i < NB_UI_MAX_LANES; i++) {
      //lanes [i].lane_widget.set_fg_color (0, 0, 0);
      if (state.lanes [i].lane_mute)
        lanes [i].lane_widget.set_state (WSTATE_DISABLED);
      else
        lanes [i].lane_widget.set_state (WSTATE_NORMAL);
    }
    if (exclusive_on) { CP
      widget_hide (lanes [i].btn_mute.widget);
      widget_show (lanes [i].btn_excl.widget);
    } else { CP
      widget_show (lanes [i].btn_mute.widget);
      widget_hide (lanes [i].btn_excl.widget);
    }
  }
  if (exclusive_on) {
    //lanes [state.exclusive_lane - 1].lane_widget.set_fg_color (0.1, 0.4, 0.4);
    lanes [state.exclusive_lane - 1].lane_widget.set_state (WSTATE_SELECTED);
  }

  updating_from_state = false;
}
