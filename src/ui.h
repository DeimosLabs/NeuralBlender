
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

#include "configfile.h"
#include "native_compat.h"
#include "widgets.h"
#include "tuner.h"

#define UI_BUTTON_RADIUS     12.0
#define UI_CHECKBOX_RADIUS   8.0
#define UI_COMBOBOX_RADIUS   8.0
#define UI_TEXTBOX_RADIUS    8.0
#define UI_MENU_RADIUS       6.0
#define UI_FRAME_RADIUS      12.0
#define UI_LIST_RADIUS       6.0
#define UI_SCROLLBAR_WIDTH   16
#define UI_SCROLLBAR_RADIUS  8.0
#define UI_STATS_PER_LANE    NB_STATS_PER_LANE
#define UI_DOUBLECLICK_MS    300

enum _ui_page {
  PAGE_PEDAL = 0,
  PAGE_AMP,
  PAGE_CAB,
  PAGE_OTHER,
  PAGE_COUNT
};

class c_neuralblender;
struct c_neuralblender_state;
class c_neuralblender_ui;
//class c_filepicker;

typedef struct {
  float vu_scale_db        = -40.0f;
  float vu_headroom_db     = 6.0f;
  bool  bypass_doubleclick = false;
  bool  bypass_rightclick  = true;
} t_prefs;

bool read_prefs_from_config  (c_configfile &configfile, t_prefs &prefs);
bool write_prefs_to_config   (c_configfile &configfile, const t_prefs &prefs);

class c_prefswindow : public c_tktoplevelwindow {
public:
  void create (c_neuralblender_ui *ui);
  void show ();
  void hide ();
  
  void on_resize () override;
  void on_tk_action (nbtk::t_action_event &event) override;
  void get_prefs_from (t_prefs &prefs);
  void set_prefs_to   (t_prefs &prefs);
  void load_defaults ();

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
};

class c_tkaboutwindow;

class c_tkaboutwindow : public c_tktoplevelwindow {
public:
  void create (c_neuralblender_ui *ui);

  void show ();
  void hide ();

  void on_tk_action (nbtk::t_action_event &event);

  nbtk::c_frame tk_frame;
  nbtk::c_staticimage tk_toplogo;
  nbtk::c_staticimage tk_logo;
  nbtk::c_label tk_labels [8];
  nbtk::c_label tk_link;
  nbtk::c_label tk_build;
  nbtk::c_button tk_ok;
  nbtk::c_knob test_knob;
  nbtk::c_listbox test_listbox;
  nbtk::c_scrollbar test_scrollbar;
};

class c_lane_widgets;

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
  void set_state (_widget_state state);
  
  //bool user_mute = false;
  size_t lane_id = -1;
  size_t bank_id = -1;
  c_neuralblender_ui *ui = NULL;
  nbtk::t_native_handle native_owner = nullptr;
  size_t which_lane = 0;
  nbtk::t_native_handle main_widget = nullptr;
  nbtk::c_widget lane_root;
  nbtk::c_frame lane_frame;
  _widget_state lane_state = WSTATE_NORMAL;
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
  nbtk::c_combobox menu_list;
  //c_label label_flip;
  //c_label label_calib;
  nbtk::c_label label_frames;
  nbtk::c_label label_trim;
  nbtk::c_label label_engine;
  
  nbtk::c_filepicker filepicker;
  
  //c_meterwidget meter_in; // we only have one input
  c_meterwidget meter_out;
  c_vudata vudata_out;
  c_widget proxy;

  void on_tk_action (nbtk::t_action_event &event);
  
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
  void draw ();
  void clear_lane_model_ui (_lane_bank bank, size_t which);
  void clear_lane_model_ui (size_t which);
  void update_ir_cwd (std::string path);
  void update_model_cwd (std::string path);

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
  int exclusive_lane_for_bank (_lane_bank bank) const;
  void set_exclusive_lane_for_bank (_lane_bank bank, int lane);
  bool linked_calib_for_bank (_lane_bank bank) const;
  void set_linked_calib_for_bank (_lane_bank bank, bool b);
  void update_stats ();
  //void excl_select (size_t which);
  void sync_widgets_from_state (const c_neuralblender_state &state, bool scan_dirs = false);
  void write_calib_state_if_consistent ();
  virtual void apply_effective_controls ();
  void set_threshgain (float f);

  bool load_model (size_t which, const char *filename);
  virtual bool load_model (_lane_bank bank, size_t which, const char *filename) = 0;
  virtual void on_gain_in (c_widget *w, float f)               = 0;
  virtual void on_ir_pitch (c_widget *w, float f)              = 0;
  virtual void on_gain_out (c_widget *w, float f)              = 0;
  virtual void on_dry_out (c_widget *w, float f)               = 0;
  virtual void on_delay (c_widget *w, float f)                 = 0;
  virtual void on_filebrowse (c_widget *w)                     = 0;
  virtual void on_fileselected (c_widget *w, const char *path) = 0;
  virtual void on_fileclear (c_widget *w)                      = 0;
  virtual void on_mute (c_widget *w, bool b)                   = 0;
  virtual void on_muteall (c_widget *w, bool b)                = 0;
  virtual void on_dcflip (c_widget *w, bool b)                 = 0;
  virtual void on_calibrate (c_widget *w, bool b)              = 0;
  virtual void on_vu (c_widget *w, bool b)                     = 0;
  virtual void on_linked_calib (c_widget *w, bool b)           = 0;
  virtual void on_calib_bass (c_widget *w, bool b)             = 0;
  virtual void on_bypass (c_widget *w, bool b)                 = 0;
  virtual void on_bank_bypass (c_widget *w, _lane_bank bank, bool b) = 0;
  virtual void on_noisegate (c_widget *w, bool b)              = 0;
  virtual void on_noisethresh (c_widget *w, float f)           = 0;
  virtual void on_noiseattack (c_widget *w, float f)           = 0;
  virtual void on_noisehold (c_widget *w, float f)             = 0;
  virtual void on_noiserelease (c_widget *w, float f)          = 0;
  virtual void on_tuner (c_widget *w, bool b)                  = 0;
  virtual void on_tuner_base_freq (c_widget *w, float f)       = 0;
  virtual void on_calib_target_db (c_widget *w, float f)       = 0;
  virtual void on_master_gain (c_widget *w, float f)           = 0;
  virtual void on_presence (c_widget *w, float f)              = 0;
  virtual void on_threshgain (c_widget *w, float f)            = 0;
  virtual void on_excl (c_widget *w, int n)                       ; // UI only
          void on_excl_use (c_widget *w, bool b)                  ;
          void on_tk_action (nbtk::t_action_event &event)         ;
  virtual void on_bank_switch (c_widget *w, int n)                ;
          void sync_page_visibility ()                            ;
          void ensure_tuner_created ()                            ;
          void sync_tuner_visibility ()                           ;
  virtual void on_window_resize (int w, int h)                    ;
          void on_window_configured ()                            ;
  virtual bool request_window_size (int w, int h)                 ;
          void on_about ()                                        ;
          void on_prefs ()                                        ;
          void on_prefs_ok ()                                     ;
  virtual void apply_prefs (t_prefs &p)                           ;
  virtual void write_prefs_to (t_prefs &p)                        ;
          void apply_ui_prefs (t_prefs &p)                        ;
	  
  nbtk::t_native_display display = NULL;
  nbtk::t_native_window window;
  c_neuralblender *blender = NULL;
  nbtk::t_native_app app;
  c_tkappbridge tk_app;
  c_mainwindow mainwindow;
  c_tkaboutwindow tkaboutwindow;
  c_prefswindow prefswindow;
  nbtk::t_native_window parent;
  int tuner_height = 56;
  bool do_set_min_size = false; // ugly hack for ardour's window size shenanigans
  
  nbtk::c_container cont_toparea;
  nbtk::c_container cont_pedals;
  nbtk::c_container cont_models;
  nbtk::c_container cont_cabs;
  nbtk::c_container cont_other;
  nbtk::c_staticimage img_logo;
  nbtk::c_button btn_tab_pedals;
  nbtk::c_button btn_tab_models;
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
  nbtk::c_knob   knob_noisethresh;
  nbtk::c_knob   knob_noiseattack;
  nbtk::c_knob   knob_noisehold;
  nbtk::c_knob   knob_noiserelease;
  nbtk::c_frame  frame_other_linkexcl;
  nbtk::c_label  label_other_byp;
  nbtk::c_label  label_other_link;
  nbtk::c_label  label_other_excl;
  nbtk::c_checkbox btn_other_link_pedal;
  nbtk::c_checkbox btn_other_link_amp;
  nbtk::c_checkbox btn_other_link_cab;
  nbtk::c_checkbox btn_other_byp_pedal;
  nbtk::c_checkbox btn_other_byp_amp;
  nbtk::c_checkbox btn_other_byp_cab;
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
  
  c_lane_widgets lanes_pedals [NB_NUM_MODELS];
  c_lane_widgets lanes_models [NB_NUM_MODELS];
  c_lane_widgets lanes_cabs [NB_NUM_MODELS];
  c_meterwidget  meter_in [PAGE_COUNT];
  c_meterwidget  meter_masterout;
  c_tunerwidget  tuner;
  
  t_prefs        prefs;
  
  c_vudata vudata_in [BANK_COUNT];
  c_vudata vudata_masterin;
  c_vudata vudata_masterout;
  c_configfile configfile;
  c_neuralblender_state state;
  _lane_bank visible_bank = BANK_AMP;
  _ui_page visible_page = PAGE_AMP;
  size_t last_exclusive_lane [BANK_COUNT] = {0, 0, 0}; // 1-based lane remembered when exclusive mode is off
  bool ui_ready;
  bool updating_from_state = false;
  bool calib_default = false;
  bool config_file_read = false;
  bool config_file_written = false;
  bool ui_resize_lock = false;
  bool ui_resize_pending = false;
  int pending_resize_w = 0;
  int pending_resize_h = 0;
  float stats [BANK_COUNT] [NB_NUM_MODELS * UI_STATS_PER_LANE];
};
