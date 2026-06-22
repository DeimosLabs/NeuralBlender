
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

#include "string"

#include "xputty.h"
#include "xframe.h"
#include "xbutton.h"
#include "xknob.h"
#include "xcombobox.h"
#include "xlabel.h"

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#define NB_UI_MAX_LANES 4

class c_neuralblender;

enum _textalign {
  TEXT_LEFT,
  TEXT_CENTER,
  TEXT_RIGHT
};

class c_widget {
public:
  virtual void create (Widget_t *parent, const char *label,
                       int x, int y, int w, int h) = 0;
  Widget_t *widget;
  std::string label;
};

class c_frame : public c_widget {
public:
  void create (Widget_t *parent, const char *label, int x, int y, int w, int h);
};

class c_label : public c_widget {
public:
  void create (Widget_t *parent, const char *label, int x, int y, int w, int h);
  static void draw (void *w, void *userdata);
  float textsize = 1.0;
  _textalign align = TEXT_CENTER;
};

class c_button : public c_widget {
public:
  void create (Widget_t *parent, const char *label, int x, int y, int w, int h);
};

class c_knob : public c_widget {
public:
  void create (Widget_t *parent, const char *label, int x, int y, int w, int h);
};

class c_combobox : public c_widget {
public:
  void create (Widget_t *parent, const char *label, int x, int y, int w, int h);
};

class c_lane_widgets {
public:
  //c_lane_widgets ();
  //~c_lane_widgets ();
  
  void create (Widget_t *parent, size_t which, int x, int y, int width, int height);
  
  size_t lane_id = -1;
  c_neuralblender *blender = NULL;
  size_t which_lane = 0;
  Widget_t *main_widget;
  c_frame lane_widget;
  c_knob gain_in;
  c_knob gain_out;
  c_button btn_mute;
  c_button btn_browse;
  c_button btn_clear;
  c_combobox menu_list;
};

class c_neuralblender_ui {
public:
  c_neuralblender_ui ();
  virtual ~c_neuralblender_ui ();
  bool create (Window parent = 0);
  void destroy ();
  int idle ();
  void draw ();
  
  virtual void on_gain_in (void *data, float f)  = 0;
  virtual void on_gain_out (void *data, float f) = 0;
  virtual void on_fileselect (void *data)       = 0;
  virtual void on_fileclear (void *data)        = 0;
  virtual void on_mute (void *data, bool b)     = 0;

//private:
  Display *display = NULL;
  Window window;
  Xputty app;
  Widget_t *main_widget = NULL;
  c_label  label_big;
  c_button btn_enable;
  c_button btn_about;
  c_lane_widgets lanes [NB_UI_MAX_LANES];
  bool ui_ready;
};

