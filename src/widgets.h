
/* NeuralBlender - RTNeural / NAM based amp modeler
 *
 * This is an "addendum" / wrapper classes around xputty to make it more
 * fit to my style of coding, and work around some of its internals/callbacks
 */

#pragma once

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <cairo/cairo.h>
#include "xputty_compat.h"

struct t_gradient;
struct t_gradientcolors;

enum _textalign {
  TEXT_LEFT,
  TEXT_CENTER,
  TEXT_RIGHT
};

enum _widget_style {
  WSTYLE_BUTTON, // TODO: decide what to do with these 2 vs redundant IMAGE_..
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
  WSTATE_DEFAULT,
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

enum _mouse_cursor {
  MOUSE_CURSOR_DEFAULT,
  MOUSE_CURSOR_HAND
};

enum _scrollbar_orientation {
  SCROLLBAR_VERTICAL,
  SCROLLBAR_HORIZONTAL
};

namespace nbtk {

using t_native_handle = nbtk_native_handle_t;

struct t_point {
  int x = 0;
  int y = 0;
};

struct t_rect {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;

  bool contains (int px, int py) const;
};

class c_app;
class c_widget;
class c_button;
class c_listbox;
class c_scrollbar;
class c_nativewindow;
class c_popupwindow;
class c_native_backend;
class c_menu;
class c_tooltip;
class c_filepicker;

enum class e_event_type {
  unknown,
  action,
  command,
  mouse,
  key,
  value_changed
};

struct t_event {
  e_event_type type = e_event_type::unknown;
  c_widget *source = nullptr;
  uint64_t source_id = 0;
  int source_index = -1;
  bool handled = false;

  virtual ~t_event () = default;
};

struct t_action_event : public t_event {
  int mouse_button = 0;
  bool value = false;

  t_action_event ();
};

struct t_command_event : public t_action_event {
  int command = 0;

  t_command_event ();
};

enum e_key {
  KEY_UNKNOWN = 0,
  KEY_TAB,
  KEY_BACKSPACE,
  KEY_DELETE,
  KEY_UP,
  KEY_DOWN,
  KEY_LEFT,
  KEY_RIGHT,
  KEY_HOME,
  KEY_END,
  KEY_RETURN,
  KEY_ESCAPE
};

class c_widget {
public:
  virtual ~c_widget () = default;

  virtual void create (
      c_widget *parent,
      const char *label,
      int x,
      int y,
      int w,
      int h);

  virtual void draw (cairo_t *cr);
  virtual bool on_mouse_down (int x, int y, int button);
  virtual bool on_mouse_up (int x, int y, int button);
  virtual bool on_mouse_move (int x, int y);
  virtual void on_mouse_enter ();
  virtual void on_mouse_leave ();
  virtual bool on_key_down (int key);
  virtual bool on_key_up (int key);
  virtual bool on_text_input (const char *text);
  virtual void clear_hover ();
  virtual void on_event (t_event &event);
  virtual void on_action (t_action_event &event);
  virtual void on_command (t_command_event &event);

  void draw_tree (cairo_t *cr);
  bool mouse_down_tree (int x, int y, int button);
  bool mouse_up_tree (int x, int y, int button);
  bool mouse_move_tree (int x, int y);
  bool update_hover_tree (int x, int y);
  void clear_hover_tree ();

  t_point local_to_root (t_point p) const;
  t_point root_to_local (t_point p) const;
  t_point local_to_screen (t_point p) const;

  t_rect rect () const;
  bool contains_local (int px, int py) const;
  float font_multiplier () const;
  void show ();
  void hide ();
  virtual void move (int x, int y);
  virtual void resize (int w, int h);
  virtual void move_resize (int x, int y, int w, int h);
  void invalidate ();
  void invalidate_rect (int x, int y, int w, int h);
  
  c_app *app = nullptr;
  c_widget *parent = nullptr;
  c_widget *action_parent = nullptr;
  std::vector<c_widget *> children;
  uint64_t id = 0;
  
  std::string label;
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
  float text_size = 1.0f;
  _textalign align = TEXT_CENTER;
  bool visible = true;
  bool enabled = true;
  bool wants_mouse = false;
  bool wants_hover = false;
  bool doublebuffer = false;
  bool mouse_inside = false;
  bool hovered = false;
  bool pressed = false;
  int last_mouse_button = Button1;
};

class c_frame : public c_widget {
public:
  void draw (cairo_t *cr) override;

  _widget_state state = WSTATE_NORMAL;
};

class c_label : public c_widget {
public:
  void draw (cairo_t *cr) override;
  bool on_mouse_down (int x, int y, int button) override;
  bool on_mouse_move (int x, int y) override;
  void on_mouse_enter () override;
  void on_mouse_leave () override;
  void clear_hover () override;

  float size = 13.0f;
  bool link = false;
};

class c_button : public c_widget {
public:
  c_button ();
  ~c_button () override;

  void draw (cairo_t *cr) override;
  bool on_mouse_down (int x, int y, int button) override;
  bool on_mouse_up (int x, int y, int button) override;
  bool on_mouse_move (int x, int y) override;
  void on_mouse_enter () override;
  void on_mouse_leave () override;
  void clear_hover () override;
  bool set_value (bool value);
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
  inline void set_image_default (const unsigned char *png)
    { set_image (png, WSTATE_DEFAULT); }
  inline void set_image_all (const unsigned char *png)
    { set_image (png, WSTATE_ALL); }

  bool is_toggle = false;
  bool value = false;
  float padding = 8.0f;

private:
  cairo_surface_t *image_for_state () const;
  void destroy_images ();

  cairo_surface_t *img_off          = nullptr;
  cairo_surface_t *img_on           = nullptr;
  cairo_surface_t *img_hover        = nullptr;
  cairo_surface_t *img_down         = nullptr;
  cairo_surface_t *img_down_hover   = nullptr;
  cairo_surface_t *img_off_hover    = nullptr;
  cairo_surface_t *img_default      = nullptr;
  const unsigned char *img_default_source = nullptr;
};

class c_checkbox : public c_button {
public:
  c_checkbox ();

  void draw (cairo_t *cr) override;
};

class c_container;

struct t_listrow {
  std::string label;
  std::string path;
  bool directory = false;
  bool symlink = false;
};

class c_scrollbar : public c_widget {
public:
  c_scrollbar ();

  void draw (cairo_t *cr) override;
  bool on_mouse_down (int x, int y, int button) override;
  bool on_mouse_up (int x, int y, int button) override;
  bool on_mouse_move (int x, int y) override;
  void on_mouse_leave () override;

  bool set_value (float value, bool notify = false);
  void set_page_size (float value);
  void set_step (float value);
  void set_container (c_container *container);
  void set_orientation (_scrollbar_orientation orientation);
  t_rect thumb_rect () const;
  void emit_action ();

  float value = 0.0f;
  float page_size = 0.1f;
  float step = 0.05f;
  _scrollbar_orientation orientation = SCROLLBAR_VERTICAL;
  c_container *container = nullptr;

private:
  bool dragging = false;
  int drag_start_x = 0;
  int drag_start_y = 0;
  float drag_start_value = 0.0f;
};

class c_container : public c_widget {
public:
  void draw (cairo_t *cr) override;
  void set_scrollbar (c_scrollbar *scrollbar);
  void set_vscrollbar (c_scrollbar *scrollbar);
  void set_hscrollbar (c_scrollbar *scrollbar);
  virtual void sync_scrollbar ();
  virtual void on_scrollbar_action (t_action_event &event);
  virtual void set_vscroll_value (float value);
  virtual void set_hscroll_value (float value);
  virtual float vscroll_value () const;
  virtual float hscroll_value () const;
  virtual float vscroll_page_size () const;
  virtual float hscroll_page_size () const;
  virtual float vscroll_step () const;
  virtual float hscroll_step () const;
  void on_action (t_action_event &event) override;

  c_scrollbar *vscrollbar = nullptr;
  c_scrollbar *hscrollbar = nullptr;
};

class c_listbox : public c_container {
public:
  c_listbox ();

  void draw (cairo_t *cr) override;
  bool on_mouse_down (int x, int y, int button) override;
  bool on_mouse_up (int x, int y, int button) override;
  bool on_key_down (int key) override;

  void clear ();
  void add (const std::string &text);
  void set_items (const std::vector<std::string> &items);
  void set_rows (const std::vector<t_listrow> &rows);
  void set_item_flags (
      const std::vector<bool> &directories,
      const std::vector<bool> &symlinks = {});
  bool set_selected (int index, bool notify = false);
  bool scroll_to (int first_row);
  void sync_scrollbar () override;
  void set_vscroll_value (float value) override;
  float vscroll_value () const override;
  float vscroll_page_size () const override;
  float vscroll_step () const override;
  int row_at (int y) const;
  int visible_rows () const;
  void emit_action (bool activated);
  void emit_action (bool activated, int index);

  virtual void on_select (int index);
  virtual void on_activate (int index);

  std::vector<t_listrow> rows;
  int selected = -1;
  int first_visible = 0;
  int row_height = 24;
  float size = 13.0f;
  bool activate_on_doubleclick = true;
  bool activate_on_click_again = false;
  bool activate_on_single_click = false;

private:
  uint64_t last_click_ms = 0;
  int mouse_down_row = -1;
  bool mouse_activate_pending = false;
};

class c_combobox : public c_widget {
public:
  c_combobox ();
  ~c_combobox () override;

  void create (
      c_widget *parent,
      const char *label,
      int x,
      int y,
      int w,
      int h) override;

  void draw (cairo_t *cr) override;
  bool on_mouse_down (int x, int y, int button) override;
  bool on_mouse_up (int x, int y, int button) override;
  bool on_key_down (int key) override;
  void on_action (t_action_event &event) override;

  void clear ();
  void add (const std::string &text);
  void set_items (const std::vector<std::string> &items);
  bool set_selected (int index, bool notify = false);
  void set_selection (int index) { set_selected (index); }
  int get_selection () const;
  std::string selected_text () const;
  void show_list ();
  void hide_list ();
  void toggle_list ();
  void sync_list_geometry ();
  void emit_action ();

  virtual void on_change (int index);
  void update_widget ();

  std::vector<std::string> items;
  int selected = -1;
  int visible_rows_max = 8;
  int dropdown_row_height = 24;
  c_listbox listbox;
  c_scrollbar vscrollbar;
  std::unique_ptr<c_popupwindow> popup;
  bool list_visible = false;
  bool toggle_on_mouse_up = false;
};

class c_knob : public c_widget {
public:
  c_knob ();

  void draw (cairo_t *cr) override;
  bool on_mouse_down (int x, int y, int button) override;
  bool on_mouse_up (int x, int y, int button) override;
  bool on_mouse_move (int x, int y) override;
  void on_mouse_leave () override;

  bool set_value (float value, bool notify = false);
  void set_range (float min, float max);
  void set_min (float min);
  void set_max (float max);
  void set_default (float value);
  void set_step (float step);
  float normalized_value () const;
  float angle_from_value () const;

  float min = 0.0f;
  float max = 1.0f;
  float value = 0.0f;
  float default_value = 0.0f;
  float step = 0.01f;
  float drag_sensitivity = 0.005f;
  bool reset_on_doubleclick = true;
  bool show_value = true;
  float size = 12.0f;

private:
  void emit_action ();
  float quantize (float value) const;

  bool dragging = false;
  int drag_start_y = 0;
  float drag_start_value = 0.0f;
  uint64_t last_click_ms = 0;
};

class c_textbox : public c_widget {
public:
  c_textbox ();

  void draw (cairo_t *cr) override;
  bool on_mouse_down (int x, int y, int button) override;
  bool on_key_down (int key) override;
  bool on_text_input (const char *text) override;

  bool set_text (const char *text);
  const std::string &text () const;

  std::string value;
  size_t cursor = 0;
  float size = 13.0f;

private:
  void emit_action ();
};

class c_staticimage : public c_widget {
public:
  ~c_staticimage () override;

  void set_png (const unsigned char *png);
  void draw (cairo_t *cr) override;

private:
  cairo_surface_t *surface = nullptr;
};

class c_app {
public:
  virtual ~c_app () = default;

  virtual void create (int w, int h);
  virtual void draw ();
  virtual void dispatch_mouse_down (int x, int y, int button);
  virtual void dispatch_mouse_up (int x, int y, int button);
  virtual void dispatch_mouse_move (int x, int y);
  virtual void dispatch_key_down (int key);
  virtual void dispatch_key_up (int key);
  virtual void dispatch_text_input (const char *text);
  virtual void invalidate_rect (int x, int y, int w, int h);
  virtual void set_mouse_cursor (_mouse_cursor cursor);
  virtual void set_focus (c_widget *widget);
  virtual void clear_focus (c_widget *widget = nullptr);

  virtual std::unique_ptr<c_popupwindow> create_popup (c_widget *owner);
  virtual t_point root_to_screen (t_point p) const;
  virtual t_point screen_to_root (t_point p) const;
  virtual void on_event (t_event &event);
  virtual void on_action (t_action_event &event);
  virtual void on_command (t_command_event &event);

  template <class T>
  T *create_widget (
      c_widget *parent,
      const char *label,
      int x,
      int y,
      int w,
      int h) {

    auto ptr = std::make_unique<T> ();
    T *ret = ptr.get ();
    ret->create (parent, label, x, y, w, h);
    widgets.push_back (std::move (ptr));
    return ret;
  }

  template <class T>
  T *create_root (int w, int h) {
    auto ptr = std::make_unique<T> ();
    T *ret = ptr.get ();
    ret->app = this;
    ret->parent = nullptr;
    ret->x = 0;
    ret->y = 0;
    ret->w = w;
    ret->h = h;
    root = ret;
    widgets.push_back (std::move (ptr));
    return ret;
  }

  cairo_t *cr = nullptr;
  std::unique_ptr<c_native_backend> backend;
  c_nativewindow *main_window = nullptr;
  c_widget *root = nullptr;
  std::vector<std::unique_ptr<c_widget>> widgets;
  std::vector<std::unique_ptr<c_popupwindow>> popups;
  c_widget *focused_widget = nullptr;
  c_widget *hovered_widget = nullptr;
  bool mouse_captured = false;
  float font_scale = 1.0f;
};

class c_native_backend {
public:
  virtual ~c_native_backend () = default;

  virtual void invalidate (t_native_handle widget) = 0;
  virtual void flush (t_native_handle widget) = 0;
  virtual bool grab_pointer (t_native_handle widget) = 0;
  virtual void ungrab_pointer (t_native_handle widget) = 0;
  virtual bool query_pointer (t_native_handle widget, t_point *local) const = 0;
  virtual void set_window_background (
      t_native_handle widget,
      const t_gradientcolors &colors) = 0;
  virtual void disable_window_background (t_native_handle widget) = 0;
  virtual void set_min_size (t_native_handle widget, int w, int h) = 0;
  virtual bool request_size (t_native_handle widget, int w, int h) = 0;
  virtual void move_resize (t_native_handle widget, int x, int y, int w, int h) = 0;
  virtual void set_mouse_cursor (t_native_handle widget, _mouse_cursor cursor) = 0;
  virtual t_point root_to_screen (t_native_handle widget, t_point p) const = 0;
  virtual t_point screen_to_root (t_native_handle widget, t_point p) const = 0;
};

std::unique_ptr<c_native_backend> create_native_backend ();

class c_nativewindow {
public:
  virtual ~c_nativewindow () = default;

  virtual void create (c_app *app, int x, int y, int w, int h);
  virtual void show ();
  virtual void hide ();
  virtual void move_resize (int x, int y, int w, int h);
  virtual void invalidate_rect (int x, int y, int w, int h);

  virtual cairo_t *begin_paint ();
  virtual void end_paint ();
  virtual void on_paint (cairo_t *cr);
  virtual void on_close ();
  virtual bool on_key_down (int key);
  virtual bool on_key_up (int key);
  virtual bool on_mouse_down (int x, int y, int button);
  virtual bool on_mouse_up (int x, int y, int button);
  virtual bool on_mouse_move (int x, int y);
  virtual void on_action (t_action_event &event);

  virtual t_point local_to_screen (t_point p) const;
  virtual t_point screen_to_local (t_point p) const;

  c_app *app = nullptr;
  c_widget root;
  c_widget *focused_widget = NULL;
  c_widget *hovered_widget = NULL;
  bool mouse_captured = false;
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
  bool visible = false;
};

class c_toplevelwindow : public c_nativewindow {
public:
  void on_close () override;
};

class c_embeddedwindow : public c_nativewindow {
public:
  void create_for_parent (c_app *app, void *native_parent, int w, int h);

  void *native_parent = nullptr;
};

class c_popupwindow : public c_nativewindow {
public:
  void create_for_owner (c_app *app, c_widget *owner, int w, int h);
  void create_native_for_owner (
      c_app *app,
      c_widget *owner,
      t_native_handle native_owner,
      int w,
      int h);
  virtual void show_at_screen_pos (int x, int y);
  virtual void close ();
  virtual bool close_on_outside_click () const;
  virtual bool takes_focus () const;
  void on_action (t_action_event &event) override;
  void invalidate_rect (int x, int y, int w, int h) override;
  void move_resize (int x, int y, int w, int h) override;
  void show () override;
  void hide () override;

  static void cb_expose (void *w, void *user_data);
  static void cb_button_press (void *w, void *event, void *user_data);
  static void cb_button_release (void *w, void *event, void *user_data);
  static void cb_motion (void *w, void *event, void *user_data);

  c_widget *owner = nullptr;
  t_native_handle widget = nullptr;
  bool pointer_grabbed = false;
  bool close_on_release = false;
};

class c_menu : public c_popupwindow {
public:
  bool close_on_outside_click () const override;
  bool takes_focus () const override;
  bool on_key_down (int key) override;
  bool on_mouse_down (int x, int y, int button) override;
};

class c_tooltip : public c_popupwindow {
public:
  bool close_on_outside_click () const override;
  bool takes_focus () const override;
  void set_text (const char *text);
};

class c_canvas : public c_widget {
public:
  c_canvas ();

  void draw (cairo_t *cr) override;
  bool on_mouse_down (int x, int y, int button) override;
  bool on_mouse_up (int x, int y, int button) override;
  bool on_mouse_move (int x, int y) override;
  void on_mouse_leave () override;
  bool on_key_down (int key) override;
  bool on_key_up (int key) override;

  void move_resize (int x, int y, int w, int h) override;
  void resize (int w, int h) override;

  void invalidate_base ();
  void invalidate_overlay ();
  void invalidate_overlay_rect (int x, int y, int w, int h);
  void expose ();

  bool button_left_down () const;
  bool button_middle_down () const;
  bool button_right_down () const;
  bool check_click_distance (int which_button) const;

protected:
  virtual void render_base (cairo_t *cr);
  virtual void render_overlay (cairo_t *cr);
  virtual void on_paint (cairo_t *cr);
  virtual void on_resize (int w, int h);
  virtual void on_mousemove (int x, int y);
  virtual void on_mousedown (int which);
  virtual void on_mouseup (int which);
  virtual void on_mousedown_left ();
  virtual void on_mouseup_left ();
  virtual void on_mousedown_middle ();
  virtual void on_mouseup_middle ();
  virtual void on_mousedown_right ();
  virtual void on_mouseup_right ();
  virtual void on_mouseleave ();
  virtual void on_mousewheel_v (int howmuch);
  virtual void on_mousewheel_h (int howmuch);
  virtual void on_keydown (int keycode, bool is_repeat);
  virtual void on_keyup (int keycode);
  virtual void on_visible ();

  int opacity = 255;
  int mouse_x = -1;
  int mouse_y = -1;
  int mousedown_x [8] = { 0 };
  int mousedown_y [8] = { 0 };
  int mouse_buttons = 0;
  int click_distance = 5;
  bool base_image_valid = false;
};

} // namespace nbtk

enum _widget_role {
  ROLE_NONE = 0,
  ROLE_BANKSWITCH,
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
  ROLE_UNKNOWN
};

class c_neuralblender_ui;
class c_neuralblender;

typedef struct t_gradientcolors {
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

inline uint64_t now_ms () {
  using clock = std::chrono::steady_clock;
  return std::chrono::duration_cast<std::chrono::milliseconds> (
    clock::now ().time_since_epoch ()
  ).count ();
}

struct c_printfps {
  std::string m_str;
  uint64_t count = 0;
  uint64_t last = now_ms ();

  c_printfps (std::string str) : m_str (std::move (str)) { }

  inline void tick () {
    count++;

    uint64_t now = now_ms ();
    if (now - last >= 1000) {
      std::cout << m_str << count << "\n";
      count = 0;
      last = now;
    }
  }
};

const t_colortheme *get_colortheme ();
void tk_path_rounded_rect (cairo_t *cr,
    double x, double y, double w, double h,
    double r);
void tk_set_gradient (cairo_t *cr, double h, const t_gradientcolors &colors);

std::string path_dirname (const std::string &path);
std::string path_basename (const std::string &path);
bool is_supported_model_filename (const std::string &path);

class c_widget {
public:
  virtual void create (
      c_neuralblender_ui *ui,
      nbtk::t_native_handle parent,
      const char *label,
      int x, int y, int w, int h);
  
  virtual void move_resize (int x, int y, int w, int h);
  virtual void move (int x, int y);
  virtual void resize (int w, int h);
  virtual bool set_label (const char *label);
  virtual bool set_tooltip (const char *text);
  // TODO: complete key event propagation/dispatch, widget focus etc.
  virtual bool on_keydown (XKeyEvent *key);
  virtual bool on_keyup (XKeyEvent *key);
  virtual void show ();
  virtual void hide ();
  virtual void focus ();
  virtual void clear_focus ();
  virtual void on_focus_lost ();
  
  void set_state (_widget_state state);
  const t_gradientcolors *bg_override_for_state (_widget_state state) const;
  const t_gradientcolors *fg_override_for_state (_widget_state state) const;
  void expose ();
  
  bool get_label_size (int *w, int *h, const char *text = NULL);
  
  // -1 for padding x/y means stay at that size
  void shrinkwrap (int padding_x = 16, int padding_y = -1);
  int x ();
  int y ();
  int w ();
  int h ();
  
  std::string label;
  std::string tooltip;
  _widget_role role        = ROLE_UNKNOWN;
  _widget_style wstyle     = WSTYLE_UNKNOWN;
  _widget_state wstate     = WSTATE_UNKNOWN;
  bool created             = false;
  uint64_t id              = -1;
  uint64_t lane            = -1;
  uint64_t bank            = -1;
  uint64_t page            = 0;
  int corner_radius        = 4;
  float text_size          = 1.0;
  float text_r             = 1.0;
  float text_g             = 1.0;
  float text_b             = 1.0;
  float padding            = 8.0;
  const t_gradientcolors *bg_colors = NULL;
  const t_gradientcolors *active_bg_colors = NULL;
  const t_gradientcolors *active_fg_colors = NULL;
  const t_gradientcolors *highlight_bg_colors = NULL;
  const t_gradientcolors *highlight_fg_colors = NULL;
  const t_gradientcolors *disabled_bg_colors = NULL;
  const t_gradientcolors *disabled_fg_colors = NULL;
  nbtk::t_native_handle widget = nullptr;
  
  void *userdata1          = NULL;
  void *userdata2          = NULL;

  // backpointers to parent/related objects
  nbtk::t_native_handle parent = nullptr;
  c_widget *parent_struct  = NULL;
  c_neuralblender_ui *ui   = NULL;
  nbtk::c_filepicker *filepicker = NULL;

};

class c_toplevelwindow : public c_widget {
public:
  bool create (
      c_neuralblender_ui *ui,
      Window parent,
      const char *title,
      int x, int y, int w, int h,
      nbtk::t_native_handle owner = nullptr);

  void set_min_size_to_current ();
  virtual void set_min_size (int w, int h);
  void show ();
  void hide ();
  void auto_close (bool b = true);
  void set_title (const char *title);
  void set_icon_from_png (const unsigned char *png);
  bool request_size (int w, int h);
  bool is_created () const;
  nbtk::t_native_handle native_handle () const;
  bool get_metrics (int *w, int *h, bool *visible = nullptr) const;
  void force_draw ();
  void set_mouse_cursor (_mouse_cursor cursor);
  void set_focused_widget (c_widget *w);
  void clear_focus ();
  
  virtual void on_expose ();
  virtual void on_resize ();
  virtual void on_configure_notify ();
  bool on_keydown (XKeyEvent *key) override;
  virtual void on_tk_action (nbtk::t_action_event &event);
  virtual void on_close ()  {};
  
  static void cb_expose (void *w, void *user_data);
  static void cb_resize (void *w, void *user_data);
  static void cb_configure_notify (void *w, void *user_data);
  static void cb_key_press (void *w, void *event, void *user_data);
  static void cb_close (void *w, void *user_data);
  
  Window window = 0;
  Window parent = 0;
  c_widget *focused_widget = NULL;
};

class c_tktoplevelwindow;

class c_tkappbridge : public nbtk::c_app {
public:
  void invalidate_rect (int x, int y, int w, int h) override;
  void set_mouse_cursor (_mouse_cursor cursor) override;
  void set_focus (nbtk::c_widget *widget) override;
  void clear_focus (nbtk::c_widget *widget = nullptr) override;
  std::unique_ptr<nbtk::c_popupwindow> create_popup (
      nbtk::c_widget *owner) override;
  nbtk::t_point root_to_screen (nbtk::t_point p) const override;
  nbtk::t_point screen_to_root (nbtk::t_point p) const override;
  void on_action (nbtk::t_action_event &event) override;

  c_tktoplevelwindow *native_window = NULL;
  c_tktoplevelwindow *action_owner = NULL;
};

class c_tktoplevelwindow : public c_toplevelwindow {
public:
  ~c_tktoplevelwindow ();

  bool create_tk (
      c_neuralblender_ui *ui,
      c_tkappbridge *tk_app,
      Window parent,
      const char *title,
      int x, int y, int w, int h,
      nbtk::t_native_handle owner = nullptr);

  void on_resize () override;
  void on_configure_notify () override;
  void on_expose () override;
  void on_close () override;
  void set_min_size (int w, int h) override;
  void redraw_child (nbtk::c_widget &child, int pad = 1);
  void activate_tk ();
  void save_tk_state ();
  void auto_hide_on_close (bool b = true);
  void auto_quit_on_close (bool b = true);

  static void cb_button_press (void *w, void *event, void *user_data);
  static void cb_button_release (void *w, void *event, void *user_data);
  static void cb_motion (void *w, void *event, void *user_data);
  static void cb_enter (void *w, void *user_data);
  static void cb_leave (void *w, void *user_data);
  static void cb_key_press (void *w, void *event, void *user_data);
  static void cb_key_release (void *w, void *event, void *user_data);

  c_tkappbridge *tk_app = NULL;
  nbtk::c_widget tk_root;
  nbtk::c_widget *focused_widget = NULL;
  nbtk::c_widget *hovered_widget = NULL;
  bool mouse_captured = false;
  bool hide_on_close = true;
  bool quit_on_close = false;
  bool handling_close = false;

private:
  void clear_tk_buffer ();
  bool ensure_tk_buffer (int w, int h);

  cairo_surface_t *tk_surface = NULL;
  cairo_t *tk_cr = NULL;
  int tk_surface_w = 0;
  int tk_surface_h = 0;
};

namespace nbtk {

class c_filepicker : public ::c_tktoplevelwindow {
public:
  void create (
      c_neuralblender_ui *ui,
      c_tkappbridge *tk_app,
      Window parent,
      t_native_handle owner,
      size_t lane,
      uint64_t bank,
      const char *title);

  void show ();
  void hide ();
  void on_resize () override;
  void on_tk_action (t_action_event &event) override;

  void sync_owner_metadata ();
  void scan_current_dir ();
  void add_files_from_dir (c_combobox *cb);
  std::string get_current_dir () const;
  void set_current_dir (std::string str);
  bool is_visible () const;
  std::string selected_path () const;

  virtual void on_file_select (const std::string &filename);

  c_frame frame;
  c_label label_path;
  c_listbox listbox;
  c_scrollbar vscrollbar;
  c_button btn_ok;
  c_button btn_cancel;

  std::string title;
  std::string current_dir;
  std::string combo_dir;
  std::vector<std::string> filelist;
  std::vector<t_listrow> rows;
  t_native_handle owner_widget = nullptr;
};

} // namespace nbtk

class c_mainwindow : public c_tktoplevelwindow {
public:
  bool create (
      c_neuralblender_ui *ui,
      Window parent,
      const char *title,
      int x, int y, int w, int h,
      nbtk::t_native_handle owner = nullptr);

  void show ();
  void show_children ();
  void on_expose () override;
  void on_resize () override;
  void on_configure_notify () override;
  void on_tk_action (nbtk::t_action_event &event) override;

private:
  bool children_mapped = false;
};

#include "meter.h"

class c_meterwidget : public nbtk::c_canvas {
public:
  void create (nbtk::c_widget *parent,
               const char *label,
               int x, int y, int w, int h);

  void show ();
  void hide ();
  void move_resize (int x, int y, int w, int h) override;
  void move (int x, int y) override;
  void resize (int w, int h) override;

  void set_db_scale (float f);
  void set_headroom (float f);
  void set_vudata (c_vudata *v);
  c_vudata *get_vudata ();
  bool needs_redraw ();
  void on_ui_timer ();

  void set_stereo (bool b);
  void set_l (float level, float hold, bool clip = false, bool xrun = false);
  void set_r (float level, float hold, bool clip = false, bool xrun = false);
  void set_compression_gain (float gain);
  void set_compression_db (float db);

  bool created = false;
  bool vertical = false;
  int width = 0;
  int height = 0;
  int clip_size = 0;
  int rec_size = 0;
  bool rec_enabled = false;
  c_vudata *data = NULL;
  float db_scale = DEFAULT_VU_DB;

protected:
  void render_base (cairo_t *cr) override;
  void on_paint (cairo_t *cr) override;
  void on_resize (int w, int h) override;

private:
  void draw_bar (cairo_t *cr, int at, int th, float level, float hold);
  void draw_warning_text (cairo_t *cr, const char *text, double x, double y,
                          double w, double h);
  void update_geometry ();

  float compressor_gain = 1.0f;
  bool stereo = true;
  int met_len = -1;
  float headroom = DEFAULT_VU_HEADROOM;
  int ln = 0;
  int th = 0;
  int t1 = 0;
  int t2 = 0;
  int t3 = 0;
  int t4 = 0;
  int tp = 1;
};
