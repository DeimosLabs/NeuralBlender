
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
  ROLE_PREFS,
  ROLE_PREFSOK,
  ROLE_PREFSCANCEL,
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
  ROLE_LINKED_CALIB,
  ROLE_EXCL_TOGGLE,
  ROLE_ADV_TOGGLE,
  ROLE_EXCL_USE,
  ROLE_BYPASS,
  ROLE_MASTER,
  ROLE_UNKNOWN
};

enum _widget_style {
  WSTYLE_BUTTON,
  WSTYLE_TOGGLE,
  WSTYLE_CHECKBOX,
  WSTYLE_RADIO,
  WSTYLE_IMAGE,
  WSTYLE_IMAGE_BUTTON,
  WSTYLE_IMAGE_BUTTON_NOFRAME,
  WSTYLE_IMAGE_TOGGLE,
  WSTYLE_IMAGE_TOGGLE_NOFRAME,
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
  WSTATE_ALL,
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

void knob_double_click (void *w_, void *event, void *user_data);
void knob_value_changed (void *w_, void *value);
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
  // TODO: complete key event propagation/dispatch, widget focus etc.
  virtual bool on_keydown (XKeyEvent *key);
  virtual bool on_keyup (XKeyEvent *key);
  void focus ();
  void clear_focus ();
  
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
  float padding            = 8.0;
  Widget_t *widget         = NULL;
  
  void *userdata1          = NULL;
  void *userdata2          = NULL;

  // backpointers to parent/related objects
  Widget_t *parent         = NULL;
  c_widget *parent_struct  = NULL;
  c_neuralblender_ui *ui   = NULL;
  c_filepicker *filepicker = NULL;

  inline void expose () { if (widget) expose_widget (widget); }
  
  bool get_label_size (int *w, int *h, const char *text = NULL);
  
  // -1 for padding x/y means stay at that size
  void resize_to_label (int padding_x = 16, int padding_y = -1);
  int x ();
  int y ();
  int w ();
  int h ();
};

class c_toplevelwindow : public c_widget {
public:
  bool create (
      c_neuralblender_ui *ui,
      Window parent,
      const char *title,
      int x, int y, int w, int h,
      Widget_t *owner = NULL);

  void set_min_size_to_current ();
  void set_min_size (int w, int h);
  void show ();
  void hide ();
  void auto_close (bool b = true);
  void set_title (const char *title);
  void set_icon_from_png (const unsigned char *png);
  bool request_size (int w, int h);
  void set_focused_widget (c_widget *w);
  void clear_focus ();
  
  virtual void on_expose ();
  virtual void on_resize ();
  bool on_keydown (XKeyEvent *key) override;
  virtual void on_close ()  {};
  
  static void cb_expose (void *w, void *user_data);
  static void cb_resize (void *w, void *user_data);
  static void cb_key_press (void *w, void *event, void *user_data);
  static void cb_close (void *w, void *user_data);
  
  Window window = 0;
  Window parent = 0;
  c_widget *focused_widget = NULL;
};

class c_mainwindow : public c_toplevelwindow {
public:
  bool create (
      c_neuralblender_ui *ui,
      Window parent,
      const char *title,
      int x, int y, int w, int h,
      Widget_t *owner = NULL);

  void on_resize () override;
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
  inline void set_db_scale (float f) { meter.set_db_scale (f); }
  inline void set_headroom (float f) { meter.set_headroom (f); }
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

class c_textbox : public c_widget {
public:
  void create (c_neuralblender_ui *ui, Widget_t *parent,
              const char *label, int x, int y, int w, int h);

  bool set_text (const char *s);
  const std::string &text () const;

  virtual void on_change (const std::string &s);
  bool on_keydown (XKeyEvent *key) override;

  static void cb_draw (void *w, void *user_data);
  static void cb_button_press (void *w, void *event, void *user_data);
  static void cb_key_press (void *w, void *event, void *user_data);

  std::string value;
  size_t cursor = 0;
  bool focused = false;
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
      _widget_style s = WSTYLE_IMAGE_BUTTON);

  bool set_value (bool value);
  virtual void on_mouseup ();
  
  static void cb_draw (void *w, void *userdata);
  static bool draw_button_image (Widget_t *w, c_button *b);
  void set_image (const unsigned char *png, _widget_state which = WSTATE_ALL);
  
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
  inline void set_image_all (const unsigned char *png)
    { set_image (png, WSTATE_ALL); }
  
  bool is_toggle = false;
  bool value = false;
  
  cairo_surface_t *img_off = NULL;
  cairo_surface_t *img_on = NULL;
  cairo_surface_t *img_hover = NULL;
  cairo_surface_t *img_down = NULL;
  cairo_surface_t *img_down_hover = NULL;
  cairo_surface_t *img_off_hover = NULL;
  cairo_surface_t *img_all = NULL;
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
  static void cb_draw (void *w, void *userdata);
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
