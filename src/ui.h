
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

#include "xputty.h"
#include "xwidgets.h"

// why does xputty define this?
#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#define NB_UI_MAX_LANES 4

class c_neuralblender;
class c_neuralblender_ui;

enum _textalign {
  TEXT_LEFT,
  TEXT_CENTER,
  TEXT_RIGHT
};

enum _widget_role {
  ROLE_ABOUT,
  ROLE_ABOUTOK,
  ROLE_UNKNOWN,
  ROLE_MUTE,
  ROLE_BROWSE,
  ROLE_CLEAR,
  ROLE_GAIN_IN,
  ROLE_GAIN_OUT,
  ROLE_DELAY,
  ROLE_BYPASS,
  ROLE_MASTER
};

class c_widget {
public:
  virtual void create (
      c_neuralblender_ui *ui,
      Widget_t *parent,
      const char *label,
      int x, int y, int w, int h) = 0;
      
  // backpointers to parent objects
  Widget_t *widget         = NULL;
  Widget_t *parent_struct  = NULL;
  c_neuralblender_ui *ui   = NULL;
  std::string label;
  _widget_role role        = ROLE_UNKNOWN;
  uint64_t id              = -1;
  uint64_t lane            = -1;
};

class c_frame : public c_widget {
public:
  void create (
      c_neuralblender_ui *ui,
      Widget_t *parent,
      const char *label,
      int x, int y, int w, int h);
};

class c_label : public c_widget {
public:
  void create (
      c_neuralblender_ui *ui,
      Widget_t *parent,
      const char *label,
      int x, int y, int w, int h);
      
  static void draw (void *w, void *userdata);
  float textsize = 1.0;
  _textalign align = TEXT_CENTER;
};

class c_button : public c_widget {
public:
  void create (
      c_neuralblender_ui *ui,
      Widget_t *parent,
      const char *label,
      int x, int y, int w, int h);
      
  void create (
      c_neuralblender_ui *ui,
      Widget_t *parent,
      const char *label, 
      int x, int y, int w, int h, bool is_toggle);
      
  bool set_value (bool value);
  bool set_label (const char *label);
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
  
  size_t lane_id = -1;
  c_neuralblender_ui *ui = NULL;
  size_t which_lane = 0;
  Widget_t *main_widget;
  c_frame lane_widget;
  c_knob gain_in;
  c_knob gain_out;
  c_knob delay;
  c_button btn_mute;
  c_button btn_browse;
  c_button btn_clear;
  c_combobox menu_list;
};

class c_aboutwindow {
public:
  void create (c_neuralblender_ui *ui);
  
  void show ();
  void hide ();
  
  Widget_t *parent = NULL;
  Widget_t *w = NULL;
  c_button btn_ok;
  c_label labels [16];
  c_neuralblender_ui *ui = NULL;
};

class c_neuralblender_ui {
public:
  c_neuralblender_ui ();
  virtual ~c_neuralblender_ui ();
  bool create (Window parent = 0);
  void destroy ();
  int idle ();
  void draw ();
  
  virtual void on_gain_in (c_widget *w, float f)  = 0;
  virtual void on_gain_out (c_widget *w, float f) = 0;
  virtual void on_delay (c_widget *w, float f)    = 0;
  virtual void on_fileselect (c_widget *w)        = 0;
  virtual void on_fileclear (c_widget *w)         = 0;
  virtual void on_mute (c_widget *w, bool b)      = 0;
  virtual void on_bypass (c_widget *w, bool b)    = 0;
  virtual void on_about (c_widget *w)             = 0;
//private:
  Display *display = NULL;
  Window window;
  c_neuralblender *blender = NULL;
  Xputty app;
  Widget_t *main_widget = NULL;
  Window parent;
  c_label  label_big;
  c_button btn_enable;
  c_button btn_about;
  c_lane_widgets lanes [NB_UI_MAX_LANES];
  c_aboutwindow aboutwindow;
  bool ui_ready;
};
