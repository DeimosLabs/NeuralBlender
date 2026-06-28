
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

#define NB_UI_MAX_LANES 4
#define NB_BG_R 0.10
#define NB_BG_G 0.10
#define NB_BG_B 0.10
#define NB_BG_A 1.00


class c_neuralblender;
struct c_neuralblender_state;
class c_neuralblender_ui;

enum _textalign {
  TEXT_LEFT,
  TEXT_CENTER,
  TEXT_RIGHT
};

enum _widget_role {
  ROLE_NONE,
  ROLE_ABOUT,
  ROLE_ABOUTOK,
  ROLE_UNKNOWN,
  ROLE_MUTE,
  ROLE_MUTEALL,
  ROLE_BROWSE,
  ROLE_LOADFILE,
  ROLE_CLEAR,
  ROLE_GAIN_IN,
  ROLE_GAIN_OUT,
  ROLE_DELAY,
  ROLE_DCFLIP,
  ROLE_CALIBRATE,
  ROLE_VUTOGGLE,
  ROLE_EXCL_TOGGLE,
  ROLE_ADV_TOGGLE,
  ROLE_EXCL_USE,
  ROLE_BYPASS,
  ROLE_MASTER
};

enum _button_style {
  BTN_NORMAL,
  BTN_TOGGLE,
  BTN_CHECKBOX
  //BTN_RADIO
};

class c_filepicker;

class c_widget {
public:
  virtual void create (
      c_neuralblender_ui *ui,
      Widget_t *parent,
      const char *label,
      int x, int y, int w, int h);
  
  virtual void move_resize (int x, int y, int w, int h);
  virtual void move (int x, int y);
  virtual void resize (int w, int h);
  virtual bool set_label (const char *label);
  virtual void set_bg_color (const float r,
                             const float g,
                             const float b);
  virtual void set_fg_color (const float r,
                             const float g,
                             const float b);

  // backpointers to parent objects
  Widget_t *widget         = NULL;
  Widget_t *parent         = NULL;
  c_widget *parent_struct  = NULL;
  c_neuralblender_ui *ui   = NULL;
  c_filepicker *filepicker = NULL;
  std::string label;
  _widget_role role        = ROLE_UNKNOWN;
  uint64_t id              = -1;
  uint64_t lane            = -1;
  
  inline int x () { return widget && widget->app ? (int) (widget->scale.init_x / widget->app->hdpi) : 0; }
  inline int y () { return widget && widget->app ? (int) (widget->scale.init_y / widget->app->hdpi) : 0; }
  inline int w () { return widget && widget->app ? (int) (widget->scale.init_width / widget->app->hdpi) : 0; }
  inline int h () { return widget && widget->app ? (int) (widget->scale.init_height / widget->app->hdpi) : 0; }
};

#include "meter.h"

class c_frame : public c_widget {
public:
  void create (
      c_neuralblender_ui *ui,
      Widget_t *parent,
      const char *label,
      int x, int y, int w, int h);
  void set_bg_color (const float r, const float g, const float b) override;
  void set_fg_color (const float r, const float g, const float b) override;
      
  float frame_thickness = 2.0;
};

class c_container : public c_widget {
public:
  void create (
      c_neuralblender_ui *ui,
      Widget_t *parent,
      const char *label,
      int x, int y, int w, int h);
  void set_bg_color (const float r, const float g, const float b) override;
};

class c_meter : public c_widget {
public:
  void create (
      c_neuralblender_ui *ui,
      Widget_t *parent,
      const char *label,
      int x, int y, int w, int h);
  void move_resize (int x, int y, int w, int h) override;
  void show ();
  void hide ();
      
  c_meterwidget meter;
  
  inline void on_ui_timer () { meter.on_ui_timer (); }
  inline void set_vudata (c_vudata *p) {meter.set_vudata (p); }
  inline void set_stereo (bool b) { meter.set_stereo (b); }
  inline bool needs_redraw () { return meter.needs_redraw (); }
};

class c_label : public c_widget {
public:
  void create (
      c_neuralblender_ui *ui,
      Widget_t *parent,
      const char *label,
      int x, int y, int w, int h);

  void set_bg_color (const float r, const float g, const float b) override;
  void set_fg_color (const float r, const float g, const float b) override;
  static void draw (void *w, void *userdata);
  float textsize = 1.0;
  _textalign align = TEXT_LEFT;
};

class c_image : public c_widget {
public:
  void create (
      c_neuralblender_ui *ui,
      Widget_t *parent,
      const char *label,
      int x, int y, int w, int h) override;

  void set_png (const unsigned char *png);
};

class c_button : public c_widget {
public:
  void create (
      c_neuralblender_ui *ui,
      Widget_t *parent,
      const char *label,
      int x, int y, int w, int h,
      _button_style s = BTN_NORMAL);

  /*void create (
      c_neuralblender_ui *ui,
      Widget_t *parent,
      const char *label,
      int x, int y, int w, int h, bool is_toggle);*/

  bool set_value (bool value);
  void set_bg_color (const float r, const float g, const float b) override;
  void set_fg_color (const float r, const float g, const float b) override;
  virtual void on_mouseup ();
  
  bool is_toggle = false;
  bool value = false;

};

class c_knob : public c_widget {
public:
  void create (
      c_neuralblender_ui *ui,
      Widget_t *parent,
      const char *label,
      int x, int y, int w, int h);

  void set_value (float v);
  void set_defaultvalue (float v);
  void set_min (float min);
  void set_max (float max);
  void set_step (float max);
  void set_bg_color (const float r, const float g, const float b) override;
  void set_fg_color (const float r, const float g, const float b) override;
  virtual void on_change ();
  virtual void on_doubleclick ();
  float min = 0;
  float max = 100;
  float value = 0;
  float defaultvalue = 0;
  float step = 1;
  bool reset_on_doubleclick = true;
};

class c_combobox : public c_widget {
public:
  void create (
      c_neuralblender_ui *ui,
      Widget_t *parent,
      const char *label,
      int x, int y, int w, int h);

  void clear ();
  void add (const std::string &str);
  void set_selection (int n);
  int get_selection ();

  void move_resize (int x, int y, int w, int h) override;
  void set_bg_color (const float r, const float g, const float b) override;
  void set_fg_color (const float r, const float g, const float b) override;

  virtual void on_change (int x);

  void update_widget ();

  std::vector<std::string> items;
  int selected = -1;
  bool strip_directories = true;
  bool updating_widget = false;
};

class c_filepicker : c_widget {
public:
  void create (c_neuralblender_ui *ui, Widget_t *parent, size_t lane, const char *title);

  void show ();
  void hide ();
  void scan_current_dir ();
  void add_files_from_dir (c_combobox *cb);
  std::string get_current_dir ();
  void set_current_dir (std::string str);
  bool is_visible () const;

  void on_file_select (c_widget *w, const std::string &filename);

  c_neuralblender_ui *ui = nullptr;
  Widget_t *parent = nullptr;
  Widget_t *dialog = nullptr;
  size_t lane = -1;

  std::string title;
  //std::string selected_file;
  std::string current_dir;
  std::vector<std::string> filelist;
};

class c_aboutwindow {
public:
  void create (c_neuralblender_ui *ui);

  void show ();
  void hide ();

  Widget_t *w = NULL;
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
  Widget_t *wreg = NULL;
  Widget_t *wadv = NULL;
  c_frame lane_widget;
  c_container cont_regcontrols;
  c_container cont_advcontrols;
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
  c_label label_flip;
  c_label label_calib;
  
  int knob_size = 64;
  int knob_top = 0;
  int knob_right = 0;

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
  void show_advanced_settings (bool b = true);
  void hide_advanced_settings ();
  void reposition_widgets (bool default_size = false);
  size_t choose_exclusive_lane () const;
  //void excl_select (size_t which);
  void sync_widgets_from_state (const c_neuralblender_state &state);
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
  virtual void on_window_resize (int w, int h)                    ;
  virtual bool request_window_size (int w, int h)                 ;
  virtual void on_advanced (c_widget *w, bool b)                  ; 
  virtual void on_bypass (c_widget *w, bool b)                 = 0;
  virtual void on_about (c_widget *w)                          = 0;

  Display *display = NULL;
  Window window;
  c_neuralblender *blender = NULL;
  Xputty app;
  Widget_t *main_widget = NULL;
  Window parent;
  
  std::vector<float> calib_data;
  
  c_container    cont_checkboxes;
  c_label        label_big;
  c_button       btn_enable;
  c_button       btn_muteall;
  c_button       btn_about;
  c_button       btn_vu;
  c_button       btn_exclmode;
  c_button       btn_advanced;
  c_label        label_vu;
  c_label        label_exclmode;
  c_label        label_advanced;
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
  bool config_file_read = false;
  bool config_file_written = false;
  //bool show_advanced = false;
  bool ui_resize_lock = false;
  //inline void reposition_widgets () { show_advanced_settings (show_advanced); }
};
