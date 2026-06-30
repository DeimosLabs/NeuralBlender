
/* NeuralBlender - RTNeural / NAM based amp modeler
 *
 * This is an "addendum" / wrapper classes around xputty to make it more
 * fit to my style of coding, and work around some of its internals/callbacks
 */

#include "ui.h"

struct t_gradient;

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

enum _widget_style {
  WSTYLE_BUTTON,
  WSTYLE_TOGGLE,
  WSTYLE_CHECKBOX,
  WSTYLE_RADIO,
  WSTYLE_IMAGE,
  WSTYLE_IMAGE_BUTTON,
  WSTYLE_IMAGE_TOGGLE,
  WSTYLE_DISABLED,
  WSTYLE_FRAME,
  WSTYLE_FRAME_HIGHLIGHT,
  WSTYLE_FRAME_DISABLED,
  WSTYLE_UNKNOWN
};

enum _widget_state {
  WSTATE_OFF,
  WSTATE_ON,
  WSTATE_HOVER,
  WSTATE_DOWN,
  WSTATE_DOWN_HOVER,
  WSTATE_OFF_HOVER,
  WSTATE_ON_HOVER,
  WSTATE_NORMAL,
  WSTATE_SELECTED,
  WSTATE_DISABLED,
  WSTATE_UNKNOWN
};

class c_filepicker;
class c_neuralblender_ui;
class c_neuralblender;

typedef struct  {
  float r1 = 0.00, g1 = 0.00, b1 = 0.00, a1 = 1.00,
        r2 = 0.00, g2 = 0.00, b2 = 0.00, a2 = 1.00;
} t_gradientcolors;

typedef struct {
  t_gradientcolors bg;
  t_gradientcolors fg;
} t_statecolors;

typedef struct {
  t_statecolors normal;
  t_statecolors hover;
  t_statecolors on;
  t_statecolors on_hover;
  t_statecolors down;
  t_statecolors disabled;
} t_controlcolors;

typedef struct {
  t_gradientcolors window_bg;

  t_controlcolors button;
  t_controlcolors checkbox;
  t_controlcolors radio;

  t_statecolors frame_normal;
  t_statecolors frame_selected;
  t_statecolors frame_disabled;
} t_colortheme;

const t_colortheme *get_colortheme ();
void set_x11_window_background (Widget_t *w, const t_gradientcolors &colors);
void cb_draw_main_window (void *w_, void *user_data);

void knob_double_click (void *w_, void *event, void *user_data);
void knob_value_changed (void *w_, void *value);
void main_notify_callback (void *w_, void *user_data);
std::string path_dirname (const std::string &path);
std::string path_basename (const std::string &path);
bool is_supported_model_filename (const std::string &path);

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
  
  void set_state (_widget_state state);
  static void draw_text_centered (Widget_t *w, 
      const char *text,
      float r, float g, float b);
  
  std::string label;
  _widget_role role        = ROLE_UNKNOWN;
  _widget_style wstyle     = WSTYLE_UNKNOWN;
  _widget_state wstate     = WSTATE_UNKNOWN;
  uint64_t id              = -1;
  uint64_t lane            = -1;
  int corner_radius        = 4;
  float text_size          = 1.0;
  float text_r             = 1.0;
  float text_g             = 1.0;
  float text_b             = 1.0;
  Widget_t *widget         = NULL;

  // backpointers to parent/related objects
  Widget_t *parent         = NULL;
  c_widget *parent_struct  = NULL;
  c_neuralblender_ui *ui   = NULL;
  c_filepicker *filepicker = NULL;

  inline void expose () { if (widget) expose_widget (widget); }
  
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
      
  static void cb_draw (void *w, void *userdata);
  float frame_thickness = 2.0;
};

class c_container : public c_widget {
public:
  void create (
      c_neuralblender_ui *ui,
      Widget_t *parent,
      const char *label,
      int x, int y, int w, int h);
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

  static void cb_draw (void *w, void *userdata);

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
  c_button ();
  ~c_button ();
  void create (
      c_neuralblender_ui *ui,
      Widget_t *parent,
      const char *label,
      int x, int y, int w, int h,
      _widget_style s = WSTYLE_BUTTON);

  bool set_value (bool value);
  virtual void on_mouseup ();
  
  static void cb_draw (void *w, void *userdata);
  static bool draw_button_image (Widget_t *w, c_button *b);
  void set_image (const unsigned char *png, _widget_state which);
  
  inline void set_image_on (const unsigned char *png)
    { set_image (png, WSTATE_ON); }
  inline void set_image_off (const unsigned char *png)
    { set_image (png, WSTATE_OFF); }
  inline void set_image_hover (const unsigned char *png)
    { set_image (png, WSTATE_HOVER); }
  inline void set_image_down (const unsigned char *png)
    { set_image (png, WSTATE_DOWN); }
  inline void set_image_down_hover (const unsigned char *png)
    { set_image (png, WSTATE_DOWN_HOVER); }
  inline void set_image_off_hover (const unsigned char *png)
    { set_image (png, WSTATE_OFF_HOVER); }
  
  bool is_toggle = false;
  bool value = false;
  
  cairo_surface_t *img_off = NULL;
  cairo_surface_t *img_on = NULL;
  cairo_surface_t *img_hover = NULL;
  cairo_surface_t *img_down = NULL;
  cairo_surface_t *img_down_hover = NULL;
  cairo_surface_t *img_off_hover = NULL;
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

  void clear ();
  void add (const std::string &str);
  void set_selection (int n);
  int get_selection ();

  void move_resize (int x, int y, int w, int h) override;

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
