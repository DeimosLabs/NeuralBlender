
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
#include <string>
#include <vector>

#include "xputty_compat.h"
#include "config.h"
#include "widgets.h"

#define NB_UI_MAX_LANES 4
#define NB_BG_R 0.10
#define NB_BG_G 0.10
#define NB_BG_B 0.10
#define NB_BG_A 1.00

#define UI_BUTTON_RADIUS     12.0
#define UI_CHECKBOX_RADIUS   8.0
#define UI_FRAME_RADIUS      12.0


class c_neuralblender;
struct c_neuralblender_state;
class c_neuralblender_ui;
class c_filepicker;

class c_aboutwindow : public c_toplevelwindow {
public:
  void create (c_neuralblender_ui *ui);

  void show ();
  void hide ();

  void on_resize ();  
  void on_close ();

  //Widget_t *w = NULL;
  c_frame frame;
  c_button btn_ok;
  c_label labels [16];
  c_image img_logo;
  c_neuralblender_ui *ui = NULL;
};

class c_lane_widgets {
public:
  //c_lane_widgets ();
  //~c_lane_widgets ();

  void create (
      c_neuralblender_ui *ui,
      Widget_t *parent,
      size_t which,
      int x, int y, int w, int h);
      
  void move_resize (int x, int y, int w, int h);
  
  //bool user_mute = false;
  size_t lane_id = -1;
  c_neuralblender_ui *ui = NULL;
  size_t which_lane = 0;
  Widget_t *main_widget = NULL;
  //Widget_t *wreg = NULL;
  //Widget_t *wadv = NULL;
  c_frame lane_widget;
  c_container cont_regcontrols;
  //c_container cont_advcontrols;
  c_knob gain_in;
  c_knob gain_out;
  c_knob delay;
  c_button btn_mute;
  c_button btn_excl;
  c_button btn_browse;
  c_button btn_clear;
  c_button btn_flip;
  c_button btn_calib;
  c_combobox menu_list;
  //c_label label_flip;
  //c_label label_calib;
  c_label label_frames;
  c_label label_trim;
  
  //int knob_size = 64;
  //int knob_top = 0;
  //int knob_right = 0;
  
  //c_meterwidget meter_in; // we only have one input
  c_meter meter_out;
  c_vudata vudata_out;
};

class c_neuralblender_ui {
public:
  c_neuralblender_ui ();
  virtual ~c_neuralblender_ui ();
  bool create (Window parent = 0);
  void destroy ();
  int idle ();
  void draw ();
  void clear_lane_model_ui (size_t which);
  void update_cwd (std::string path);

  void set_lane_mute (size_t which, bool b);
  void vu_on (bool b = true);
  void vu_off ();
  //void show_advanced_settings (bool b = true);
  //void hide_advanced_settings ();
  void reposition_widgets (bool default_size = false);
  size_t choose_exclusive_lane () const;
  void update_stats ();
  //void excl_select (size_t which);
  void sync_widgets_from_state (const c_neuralblender_state &state);
  void write_calib_state_if_consistent ();
  virtual void apply_effective_controls ();

  virtual bool load_model (size_t which, const char *filename) = 0;
  virtual void on_gain_in (c_widget *w, float f)               = 0;
  virtual void on_gain_out (c_widget *w, float f)              = 0;
  virtual void on_delay (c_widget *w, float f)                 = 0;
  virtual void on_filebrowse (c_widget *w)                     = 0;
  virtual void on_fileselected (c_widget *w, const char *path) = 0;
  virtual void on_fileclear (c_widget *w)                      = 0;
  virtual void on_mute (c_widget *w, bool b)                   = 0;
  virtual void on_muteall (c_widget *w, bool b)                = 0;
  virtual void on_dcflip (c_widget *w, bool b)                 = 0;
  virtual void on_calibrate (c_widget *w, bool b)              = 0;
  virtual void on_vu (c_widget *w, bool b)                     = 0;
  virtual void on_excl (c_widget *w, int n)                       ; // UI only
          void on_excl_use (c_widget *w, bool b)                  ;
          void on_button (c_button *btn, bool value)               ;
  virtual void on_window_resize (int w, int h)                    ;
  virtual bool request_window_size (int w, int h)                 ;
  //virtual void on_advanced (c_widget *w, bool b)                  ; 
  virtual void on_bypass (c_widget *w, bool b)                 = 0;
  virtual void on_about (c_widget *w)                          = 0;

  Display *display = NULL;
  Window window;
  c_neuralblender *blender = NULL;
  Xputty app;
  c_mainwindow mainwindow;
  Window parent;
  
  c_container    cont_checkboxes;
  c_label        label_big;
  c_button       btn_enable;
  c_button       btn_muteall;
  c_button       btn_about;
  c_button       btn_vu;
  c_button       btn_exclmode;
  c_button       btn_advanced;
  //c_label        label_vu;
  //c_label        label_exclmode;
  //c_label        label_advanced;
  c_lane_widgets lanes [NB_UI_MAX_LANES];
  c_filepicker   filepickers [NB_UI_MAX_LANES];
  c_meter        meter_in;
  
  c_vudata vudata_in;
  c_aboutwindow aboutwindow;
  c_configfile configfile;
  //size_t exclusive_lane = 0; // 0: normal mode, 1-4: exclusive mode with lane [n - 1]
  //bool user_bypass = false;
  //bool do_vu = true;
  //bool do_excl = false;
  c_neuralblender_state state;
  size_t last_exclusive_lane = 0; // 1-based lane remembered when exclusive mode is off
  bool ui_ready;
  bool updating_from_state = false;
  bool calib_default = false;
  bool config_file_read = false;
  bool config_file_written = false;
  //bool show_advanced = false;
  bool ui_resize_lock = false;
  float stats [NB_UI_MAX_LANES * 2];
  //inline void reposition_widgets () { show_advanced_settings (show_advanced); }
};
