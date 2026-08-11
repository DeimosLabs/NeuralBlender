
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
 * Shared UI code used by standalone and LV2
 */

#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "state.h"
#include "nbtk.h"
#include "tuner.h"
#include "eqgraph.h"

#define UI_STATS_PER_LANE    NB_STATS_PER_LANE

enum _ui_page {
  PAGE_NONE = -1,
  PAGE_PEDAL = 0,
  PAGE_EQPRE,
  PAGE_AMP,
  PAGE_EQPOST,
  PAGE_CAB,
  PAGE_OTHER,
  PAGE_COUNT
};

enum _ui_eq_band_mode {
  UI_EQ_HIPASS1 = 0,
  UI_EQ_HIPASS2,
  UI_EQ_HIPASS3,
  UI_EQ_HIPASS4,
  UI_EQ_LOWSHELF,
  UI_EQ_BELL,
  UI_EQ_HISHELF,
  UI_EQ_LOWPASS1,
  UI_EQ_LOWPASS2,
  UI_EQ_LOWPASS3,
  UI_EQ_LOWPASS4,
  UI_EQ_KEEP
};

enum _widget_role {
  ROLE_NONE = 0,
  ROLE_PAGESWITCH,
  ROLE_ABOUT,
  ROLE_ABOUTOK,
  ROLE_PREFS,
  ROLE_PREFSDEFAULTS,
  ROLE_PREFSOK,
  ROLE_PREFSCANCEL,
  ROLE_MUTE,
  ROLE_MUTEALL,
  ROLE_BROWSE,
  ROLE_LOADFILE,
  ROLE_CLEAR,
  ROLE_GAIN_IN,
  ROLE_IR_PITCH,
  ROLE_GAIN_OUT,
  ROLE_DRY_OUT,
  ROLE_DELAY,
  ROLE_DCFLIP,
  ROLE_CALIBRATE,
  ROLE_CALIBBASS,
  ROLE_NOISEGATE,
  ROLE_VUTOGGLE,
  ROLE_BANK_BYPASS,
  ROLE_LINKED_CALIB,
  ROLE_EXCL_TOGGLE,
  ROLE_ADV_TOGGLE,
  ROLE_EXCL_USE,
  ROLE_BYPASS,
  ROLE_MASTER,
  ROLE_PRESENCE,
  ROLE_NOISETHRESH,
  ROLE_NOISEATTACK,
  ROLE_NOISEHOLD,
  ROLE_NOISERELEASE,
  ROLE_TUNER,
  ROLE_TUNER_BASE_FREQ,
  ROLE_TUNER_DOWN,
  ROLE_TUNER_UP,
  ROLE_TUNER_DEFAULT,
  ROLE_CALIB_TARGET_DB,
  ROLE_EQ_ENABLED,
  ROLE_EQ_MODE,
  ROLE_EQ_FREQ,
  ROLE_EQ_GAIN,
  ROLE_EQ_Q,
  ROLE_EQ_MASTER_GAIN,
  ROLE_EQ_GRAPH,
  ROLE_EQ_LOAD_PRESET,
  ROLE_EQ_SAVE_PRESET,
  ROLE_EQ_DELETE_PRESET,
  ROLE_EQ_GRAPHHOLD,
  ROLE_UNKNOWN
};

enum _ui_command {
  CMD_NOOP,
  CMD_LOAD_PRESET,
  CMD_SAVE_PRESET,
  CMD_LOAD_PRESET_CB,
  CMD_SAVE_PRESET_CB,
  CMD_PREFS,
  CMD_ABOUT,
  CMD_QUIT,
  CMD_CLOSE,
  CMD_EQ_LOAD_PRESET,
  CMD_EQ_SAVE_PRESET,
  CMD_EQ_REPLACE_PRESET,
  CMD_EQ_DELETE_PRESET,
  CMD_EQ_RESET,
  CMD_MODEL_FILE_SELECTED
};

class c_neuralblender;
struct c_neuralblender_state;
struct c_eq_state;
class c_neuralblender_ui;
//class c_filepicker;

typedef struct {
  float vu_scale_db        = -40.0f;
  float vu_headroom_db     = 6.0f;
  bool  calib_default      = false;
  bool  bypass_doubleclick = false;
  bool  bypass_rightclick  = true;
  bool  show_tooltips      = true;
} t_prefs;

bool read_prefs_from_config  (c_configfile &configfile, t_prefs &prefs);
bool write_prefs_to_config   (c_configfile &configfile, const t_prefs &prefs);
bool is_supported_model_filename (const std::string &path);

class c_prefswindow : public nbtk::c_toplevelwindow {
public:
  void create (c_neuralblender_ui *ui);
  void show ();
  void hide ();
  
  void on_resize () override;
  bool on_key_down (int key) override;
  void on_action (nbtk::t_action_event &event) override;
  void get_prefs_from (t_prefs &prefs);
  void set_prefs_to   (t_prefs &prefs);
  void load_defaults ();

  c_neuralblender_ui *ui = NULL;
  nbtk::c_frame frame1;
  nbtk::c_button btn_cancel;
  nbtk::c_button btn_ok;
  nbtk::c_button btn_defaults;
  
  nbtk::c_label label_vuscale;
  nbtk::c_label label_vuheadroom;
  nbtk::c_label label_spacer1;
  
  nbtk::c_textbox text_vuscale;
  nbtk::c_textbox text_vuheadroom;
  nbtk::c_checkbox btn_bypass_doubleclick;
  nbtk::c_checkbox btn_bypass_rightclick;
  nbtk::c_checkbox btn_calib_default;
  nbtk::c_checkbox btn_show_tooltips;
};

class c_aboutwindow : public nbtk::c_toplevelwindow {
public:
  void create (c_neuralblender_ui *ui);
  
  void show ();
  void hide ();
  
  void on_resize () override;
  bool on_key_down (int key) override;
  void on_action (nbtk::t_action_event &event);
  
  c_neuralblender_ui *ui = NULL;
  nbtk::c_frame frame_main;
  nbtk::c_staticimage image_toplogo;
  nbtk::c_staticimage image_logo;
  nbtk::c_label label_text [8];
  //nbtk::c_label label_link1;
  //nbtk::c_label label_link2;
  nbtk::c_label label_build;
  nbtk::c_button btn_ok;
};

class c_neuralblendermainwindow : public nbtk::c_toplevelwindow {
public:
  bool create (
      c_neuralblender_ui *ui,
      nbtk::t_native_window parent,
      const char *title,
      int x, int y, int w, int h,
      nbtk::t_native_handle owner = nullptr);

  void show ();
  void show_children ();
  void on_expose () override;
  void on_resize () override;
  void on_configure_notify () override;
  void on_action (nbtk::t_action_event &event) override;
  void on_command (nbtk::t_command_event &event) override;
  void on_hover_changed (nbtk::c_widget *hovered) override;

private:
  c_neuralblender_ui *ui = NULL;
  bool children_mapped = false;
};

class c_neuralblender_filepicker : public nbtk::c_filepicker {
public:
  void create (
      c_neuralblender_ui *ui,
      nbtk::c_app *nbtk_app,
      nbtk::t_native_window parent,
      nbtk::t_native_handle owner,
      size_t lane,
      uint64_t bank,
      const char *title);

  void show () override;
  void set_current_dir (std::string str) override;
  void add_files_from_dir (
      nbtk::c_combobox *cb,
      const std::string &selected_file = "") override;
  void on_file_select (const std::string &filename) override;

  size_t lane = (size_t) -1;
  uint64_t bank = (uint64_t) -1;
  c_neuralblender_ui *ui = NULL;
};

class c_freqknob : public nbtk::c_knob {
public:
  c_freqknob () {
    show_value = false;
  }

  std::string get_label_string () const override {
    return nbtk::c_knob::get_value_string () + "Hz";
  }
};

class c_qknob : public nbtk::c_knob {
public:
  c_qknob () {
    show_value = false;
  }

  std::string get_label_string () const override {
    return "Q=" + nbtk::c_knob::get_value_string ();
  }
};

class c_gainknob : public nbtk::c_knob {
public:
  c_gainknob () {
    show_value = false;
  }

  std::string get_label_string () const override {
    return /*"Gain: " + */nbtk::c_knob::get_value_string () + "dB";
  }
};

class c_gainslider : public nbtk::c_slider {
public:
  std::string get_label_string () const override {
    return nbtk::c_slider::get_value_string () + "dB";
  }
};

class c_eqband_widgets {
public:
  //nbtk::c_container   container;
  c_gainslider        slider_gain;
  nbtk::c_checkbox    btn_on;
  nbtk::c_combobox    cb_mode;
  c_freqknob          knob_freq;
  c_qknob             knob_q;
};

class c_eqpage_widgets {
public:
  void create (
      c_neuralblender_ui *ui,
      nbtk::c_widget *parent,
      nbtk::t_native_handle native_owner,
      size_t bank_id,
      //size_t lane_id,
      int x, int y, int w, int h);
  
  void move_resize (int x, int y, int w, int h);
  void sync_from_state (const c_eq_state &state);
  void sync_band_from_state (const c_eq_state &state, size_t band);
  void sync_highlight_from_hover (nbtk::c_widget *hovered);
  void set_state (nbtk::_widget_state state);
  bool on_action (nbtk::t_action_event &event);
  bool on_command (nbtk::t_command_event &event);
  
  size_t              bank_id = BANK_NONE;
  nbtk::c_widget      cont_sliders;
  nbtk::c_container   cont_graph;
  nbtk::c_frame       frame;
  c_eqgraph           graph;
  nbtk::c_label       label;
  nbtk::c_combobox    cb_presets;
  nbtk::c_button      btn_savepreset;
  nbtk::c_button      btn_deletepreset;
  nbtk::c_button      btn_graphhold;
  c_gainknob          knob_gain;
  nbtk::c_frame       cont_bands;
  c_eqband_widgets    bands [EQ_NUM_BANDS];
  std::string         pending_preset_name;
  
  c_neuralblender_ui  *ui;
};

class c_lane_widgets {
public:
  //c_lane_widgets ();
  //~c_lane_widgets ();

  void create (
      c_neuralblender_ui *ui,
      nbtk::c_widget *parent,
      nbtk::t_native_handle native_owner,
      size_t bank_id,
      size_t lane_id,
      int x, int y, int w, int h);
	      
  void move_resize (int x, int y, int w, int h);
  void set_state (nbtk::_widget_state state);
  
  //bool user_mute = false;
  size_t lane_id = -1;
  size_t bank_id = -1;
  c_neuralblender_ui *ui = NULL;
  nbtk::t_native_handle native_owner = nullptr;
  size_t which_lane = 0;
  nbtk::t_native_handle main_widget = nullptr;
  nbtk::c_widget lane_root;
  nbtk::c_frame lane_frame;
  nbtk::_widget_state lane_state = nbtk::WSTATE_NORMAL;
  bool created = false;
  //c_container cont_regcontrols;
  //c_container cont_advcontrols;
  
  nbtk::c_knob knob_gain_in;
  nbtk::c_knob knob_ir_pitch;
  nbtk::c_knob knob_gain_out;
  nbtk::c_knob knob_dry_out;
  nbtk::c_knob knob_delay;
  nbtk::c_knob knob_dryout;
  
  nbtk::c_button btn_mute;
  nbtk::c_button btn_excl;
  nbtk::c_button btn_browse;
  nbtk::c_button btn_clear;
  nbtk::c_button btn_flip;
  nbtk::c_button btn_calib;
  nbtk::c_combobox cb_list;
  //c_label label_flip;
  //c_label label_calib;
  nbtk::c_label label_frames;
  nbtk::c_label label_trim;
  nbtk::c_label label_engine;
  
  c_neuralblender_filepicker filepicker;
  
  //c_meterwidget meter_in; // we only have one input
  c_meterwidget meter_out;
  c_vudata vudata_out;

  void on_action (nbtk::t_action_event &event);
  
  int last_x = 0;
  int last_y = 0;
  int last_w = 0;
  int last_h = 0;
};

class c_neuralblender_ui {
public:
  c_neuralblender_ui ();
  virtual ~c_neuralblender_ui ();
  bool create (nbtk::t_native_window parent = 0);
  void destroy ();
  virtual int idle ();
  void set_samplerate (int samplerate);
  void draw ();
  void clear_lane_model_ui (_lane_bank bank, size_t which);
  void clear_lane_model_ui (size_t which);
  void update_model_cwd (std::string path);
  void update_ir_cwd (std::string path);
  void update_preset_cwd (std::string path);

  void set_lane_mute (_lane_bank bank, size_t which, bool b);
  void set_lane_mute (size_t which, bool b);
  void vu_on (bool b = true);
  void vu_off ();
  //void show_advanced_settings (bool b = true);
  //void hide_advanced_settings ();
  void move_resize (bool default_size = false);
  size_t choose_exclusive_lane () const;
  c_lane_widgets *lanes_for_bank (_lane_bank bank);
  const c_lane_widgets *lanes_for_bank (_lane_bank bank) const;
  c_meterwidget &input_meter_for_bank (_lane_bank bank);
  c_vudata &input_vudata_for_bank (_lane_bank bank);
  void redraw_visible_meters ();
  void redraw_tuner_if_needed ();
  void refresh_config_if_needed ();
  int exclusive_lane_for_bank (_lane_bank bank) const;
  void set_exclusive_lane_for_bank (_lane_bank bank, int lane);
  bool linked_calib_for_bank (_lane_bank bank) const;
  void set_linked_calib_for_bank (_lane_bank bank, bool b);
  void update_stats ();
  void on_command (nbtk::t_command_event &event);
  //void excl_select (size_t which);
  void sync_widgets_from_state (const c_neuralblender_state &state, bool scan_dirs = false);
  void write_calib_state_if_consistent ();
  virtual void apply_effective_controls ();
  void set_threshgain (float f);
  void sync_eq_presets ();
  void load_eq_preset (int bank, int which);
  void save_eq_preset (int which);
  void save_eq_preset_callback (int which);
  void delete_preset (int which);
  void clear_hold (int bank);
  void load_eq_preset (size_t bank_id, std::string name);
  void save_eq_preset (size_t bank_id, std::string name);
  bool delete_selected_eq_preset (size_t bank_id);
  void load_preset_file (std::string name);
  void save_preset_file (std::string name);

  bool load_model (size_t which, const char *filename);
  virtual bool load_model (_lane_bank bank, size_t which, const char *filename) = 0;
  virtual void on_gain_in (nbtk::c_widget *w, float f)               = 0;
  virtual void on_ir_pitch (nbtk::c_widget *w, float f)              = 0;
  virtual void on_gain_out (nbtk::c_widget *w, float f)              = 0;
  virtual void on_dry_out (nbtk::c_widget *w, float f)               = 0;
  virtual void on_delay (nbtk::c_widget *w, float f)                 = 0;
  virtual void on_filebrowse (nbtk::c_widget *w)                     = 0;
  virtual void on_fileselected (nbtk::c_widget *w, const char *path) = 0;
  virtual void on_fileclear (nbtk::c_widget *w)                      = 0;
  virtual void on_mute (nbtk::c_widget *w, bool b)                   = 0;
  virtual void on_muteall (nbtk::c_widget *w, bool b)                = 0;
  virtual void on_dcflip (nbtk::c_widget *w, bool b)                 = 0;
  virtual void on_calibrate (nbtk::c_widget *w, bool b)              = 0;
  virtual void on_vu (nbtk::c_widget *w, bool b)                     = 0;
  virtual void on_linked_calib (nbtk::c_widget *w, bool b)           = 0;
  virtual void on_calib_bass (nbtk::c_widget *w, bool b)             = 0;
  virtual void on_bypass (nbtk::c_widget *w, bool b)                 = 0;
  virtual void on_bank_bypass (nbtk::c_widget *w, _lane_bank bank, bool b) = 0;
  virtual void on_noisegate (nbtk::c_widget *w, bool b)              = 0;
  virtual void on_noisethresh (nbtk::c_widget *w, float f)           = 0;
  virtual void on_noiseattack (nbtk::c_widget *w, float f)           = 0;
  virtual void on_noisehold (nbtk::c_widget *w, float f)             = 0;
  virtual void on_noiserelease (nbtk::c_widget *w, float f)          = 0;
  virtual void on_tuner (nbtk::c_widget *w, bool b)                  = 0;
  virtual void on_tuner_base_freq (nbtk::c_widget *w, float f)       = 0;
  virtual void on_calib_target_db (nbtk::c_widget *w, float f)       = 0;
  virtual void on_master_gain (nbtk::c_widget *w, float f)           = 0;
  virtual void on_presence (nbtk::c_widget *w, float f)              = 0;
  virtual void on_threshgain (nbtk::c_widget *w, float f)            = 0;
  virtual void on_eq_band (nbtk::c_widget *w, _lane_bank bank, size_t band) = 0;
  virtual void on_eq_master_gain (nbtk::c_widget *w, _lane_bank bank, float f) = 0;
  virtual void get_dsp_state (c_neuralblender_state &dest)           = 0;
  virtual bool set_dsp_state (const c_neuralblender_state &src)      = 0;
  virtual void on_excl (nbtk::c_widget *w, int n)                       ; // UI only
          void on_excl_use (nbtk::c_widget *w, bool b)                  ;
          void on_action (nbtk::t_action_event &event)                  ;
  virtual void on_bank_switch (nbtk::c_widget *w, int n)                ;
          c_eq_state &ui_eq_state_for_bank (_lane_bank bank)            ;
    const c_eq_state &ui_eq_state_for_bank (_lane_bank bank) const      ;
          void sync_page_visibility ()                                  ;
          void ensure_tuner_created ()                                  ;
          void sync_tuner_visibility ()                                 ;
	          void sync_eq_graph_highlight ()                               ;
	          void sync_eq_graph_highlight (nbtk::c_widget *hovered)        ;
  virtual void on_window_resize (int w, int h)                          ;
          void on_window_configured ()                                  ;
  virtual bool request_window_size (int w, int h)                       ;
          void on_about ()                                              ;
          void on_prefs ()                                              ;
          void on_prefs_ok ()                                           ;
  virtual void apply_prefs (t_prefs &p)                                 ;
  virtual void write_prefs_to (t_prefs &p)                              ;
          void apply_ui_prefs (t_prefs &p)                              ;
	
  int samplerate = 0;
  nbtk::t_native_display display = NULL;
  nbtk::t_native_window window;
  c_neuralblender *blender = NULL;
  nbtk::t_native_app app;
  nbtk::c_app nbtk_app;
  c_neuralblendermainwindow mainwindow;
  c_aboutwindow aboutwindow;
  c_prefswindow prefswindow;
  c_neuralblender_filepicker filepicker;
  nbtk::t_native_window parent;
  int tuner_height = 56;
  bool do_set_min_size = false; // ugly hack for ardour's window size shenanigans
  
  nbtk::c_menubar menubar;
  nbtk::c_topmenu *menu_presets;
  nbtk::c_topmenu *menu_misc;
  nbtk::c_container cont_toparea;
  nbtk::c_container cont_pedals;
  nbtk::c_container cont_eqpre;
  nbtk::c_container cont_models;
  nbtk::c_container cont_eqpost;
  nbtk::c_container cont_cabs;
  nbtk::c_container cont_other;
  nbtk::c_imagebutton img_logo;
  nbtk::c_button btn_tab_pedals;
  nbtk::c_button btn_tab_eqpre;
  nbtk::c_button btn_tab_models;
  nbtk::c_button btn_tab_eqpost;
  nbtk::c_button btn_tab_cabs;
  nbtk::c_button btn_tab_other;
  nbtk::c_button btn_enable;
  nbtk::c_button btn_muteall;
  nbtk::c_button btn_noisegate;
  nbtk::c_button btn_tuner;
  
  nbtk::c_frame  frame_other_volumepresence;
  nbtk::c_knob   knob_mastervolume;
  nbtk::c_knob   knob_presence;
  nbtk::c_frame  frame_other_noisegate;
  nbtk::c_label  label_other_noisegate;
  nbtk::c_button btn_other_noisegate;
  nbtk::c_knob   knob_noisethresh;
  nbtk::c_knob   knob_noiseattack;
  nbtk::c_knob   knob_noisehold;
  nbtk::c_knob   knob_noiserelease;
  nbtk::c_frame  frame_other_linkexcl;
  nbtk::c_label  label_other_byp;
  nbtk::c_label  label_other_link;
  nbtk::c_label  label_other_excl;
  nbtk::c_checkbox btn_other_byp_pedal;
  nbtk::c_checkbox btn_other_byp_eq1;
  nbtk::c_checkbox btn_other_byp_amp;
  nbtk::c_checkbox btn_other_byp_eq2;
  nbtk::c_checkbox btn_other_byp_cab;
  nbtk::c_checkbox btn_other_link_pedal;
  nbtk::c_checkbox btn_other_link_amp;
  nbtk::c_checkbox btn_other_link_cab;
  nbtk::c_checkbox btn_other_excl_pedal;
  nbtk::c_checkbox btn_other_excl_amp;
  nbtk::c_checkbox btn_other_excl_cab;
  nbtk::c_frame  frame_other_misc;
  nbtk::c_label  label_other_tuner;
  nbtk::c_label  label_other_calib;
  nbtk::c_textbox text_other_tuner;
  nbtk::c_textbox text_other_calib;
  nbtk::c_button btn_other_tuner_down;
  nbtk::c_button btn_other_tuner_up;
  nbtk::c_button btn_other_tuner_default;
  nbtk::c_checkbox btn_other_vu;
  nbtk::c_checkbox btn_other_bass;
  nbtk::c_button btn_other_prefs;
  nbtk::c_button btn_other_about;
  
  c_lane_widgets   lanes_pedals [NB_NUM_MODELS];
  c_lane_widgets   lanes_models [NB_NUM_MODELS];
  c_lane_widgets   lanes_cabs [NB_NUM_MODELS];
  c_meterwidget    meter_in [PAGE_COUNT];
  c_meterwidget    meter_eqout [2];
  c_meterwidget    meter_masterout;
  c_tunerwidget    tuner;
  c_eqpage_widgets eqpage_pre;
  c_eqpage_widgets eqpage_post;
  
  t_prefs        prefs;
  
  c_vudata vudata_in [BANK_COUNT];
  c_vudata vudata_masterin;
  c_vudata vudata_masterout;
  c_configfile configfile;
  c_neuralblender_state state;
  c_eq_state ui_eqpre;
  c_eq_state ui_eqpost;
  uint64_t last_config_check_ms = 0;
  _lane_bank visible_bank = BANK_AMP;
  _ui_page visible_page = PAGE_AMP;
  size_t last_exclusive_lane [BANK_COUNT] = {0, 0, 0}; // 1-based lane remembered when exclusive mode is off
  bool ui_ready;
  bool updating_from_state = false;
  bool eq_auto_enable = true;
  bool calib_default = false;
  bool config_file_read = false;
  bool config_file_written = false;
  bool ui_resize_lock = false;
  float stats [BANK_COUNT] [NB_NUM_MODELS * UI_STATS_PER_LANE];
};
