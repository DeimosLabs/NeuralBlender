
/* NBTK - NeuralBlender Tool Kit
 * Formerly (and very briefly) known as Simple Halfassed Interface Toolkit
 *
 * This started out as a set of wrapper classes around xputty to make it
 * more fit to my style of coding, but then it quickly grew into its own
 * lightweight UI toolkit. I'm keeping it separate from the DSP and UI code,
 * since it may be reused in other projects. Huge thanks to Codex for lots
 * of help with this!!
 */

#pragma once

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <cairo/cairo.h>
#include "native.h"

#define NBTK_BUTTON_RADIUS      8.0
#define NBTK_CHECKBOX_RADIUS    6.0
#define NBTK_COMBOBOX_RADIUS    8.0
#define NBTK_TEXTBOX_RADIUS     8.0
#define NBTK_MENU_RADIUS        6.0
#define NBTK_FRAME_RADIUS       12.0
#define NBTK_TOOLTIP_RADIUS     4.0
#define NBTK_LIST_RADIUS        6.0
#define NBTK_SCROLLBAR_WIDTH    16
#define NBTK_SCROLLBAR_RADIUS   8.0
#define NBTK_POPUP_MAX_WIDTH    640
#define NBTK_DOUBLECLICK_MS     300
#define NBTK_MOUSEWHEEL_ROWS    2
#define NBTK_AUTOSCROLL_MS      20

namespace nbtk {

static constexpr float FONTSIZE_BASE    = 13.0f; // BASE font size
static constexpr float TEXTSIZE_MINI    = 0.75f; // about  9pt on 13pt base
static constexpr float TEXTSIZE_SMALL   = 0.80f; // about 10pt on 13pt base
static constexpr float TEXTSIZE_COMPACT = 0.90f; // about 11pt on 13pt base
static constexpr float TEXTSIZE_NORMAL  = 1.00f; // about 12pt on 13pt base
static constexpr float TEXTSIZE_LARGE   = 1.10f; // about 14pt on 13pt base
static constexpr float TEXTSIZE_HUGE    = 1.25f; // about 15pt on 13pt base

struct t_gradient;
struct t_gradientcolors;

enum _textalign {
  TEXT_LEFT,
  TEXT_CENTER,
  TEXT_RIGHT
};

enum _label_position {
  LABEL_NONE,
  LABEL_ABOVE,
  LABEL_BELOW,
  LABEL_LEFT,
  LABEL_RIGHT
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
class c_toplevelwindow;
class c_popupwindow;
class c_menu;
class c_menulistbox;
class c_topmenu;
class c_menubar;
class c_native_backend;
class c_tooltip;
class c_filepicker;
class c_valueeditor_popup;
class c_askstring_dialog;
class c_askyesno_dialog;

enum class _event_type {
  unknown,
  action,
  command,
  mouse,
  key,
  value_changed
};

enum class _command_result {
  none,
  accepted,
  rejected,
  cancelled
};

enum _nbtk_command : int64_t {
  NBTK_CMD_FILEPICKER_OVERWRITE = -3,
  NBTK_CMD_NONE = -2,
  NBTK_CMD_SET_VALUE = -1
};

struct t_event {
  _event_type type = _event_type::unknown;
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
  int64_t command = 0;
  _command_result result = _command_result::none;
  std::string text;

  t_command_event ();
};

enum _special_key {
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
  KEY_PAGE_UP,
  KEY_PAGE_DOWN,
  KEY_SPACE,
  KEY_RETURN,
  KEY_ESCAPE
};

enum e_key_mod {
  KEYMOD_SHIFT = 1 << 0,
  KEYMOD_CTRL  = 1 << 1,
  KEYMOD_ALT   = 1 << 2
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
  virtual bool on_tab (bool shift);
  virtual void clear_hover ();
  virtual _mouse_cursor mouse_cursor () const;
  virtual void on_event (t_event &event);
  virtual void on_action (t_action_event &event);
  virtual void on_command (t_command_event &event);
  bool emit_command (
      bool value = false,
      int source_index = -1,
      const std::string &text = "");

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
  float font_size (float multiplier = 1.0f) const;
  bool is_visible () const;
  void show ();
  void hide ();
  virtual void move (int x, int y);
  virtual void resize (int w, int h);
  virtual void move_resize (int x, int y, int w, int h);
  virtual void shrinkwrap (
      int padding_x = 16,
      int padding_y = -1,
      bool center_x = false,
      bool center_y = true);
  virtual bool highlighted () const;
  bool set_external_highlight (bool b);
  void invalidate ();
  void invalidate_rect (int x, int y, int w, int h);
  bool set_label (const char *text);
  bool set_label (const std::string &text);
  void set_tooltip (const char *text);
  void set_font (cairo_t *cr) {}
  float get_app_font_size ();
  
  c_app *app = nullptr;
  c_toplevelwindow *toplevel = nullptr;
  c_widget *parent = nullptr;
  c_widget *action_parent = nullptr;
  std::vector<c_widget *> children;
  uint64_t id = 0;
  int64_t role = -1;
  uint64_t lane = (uint64_t) -1;
  uint64_t bank = (uint64_t) -1;
  uint64_t page = 0;
  int64_t command = NBTK_CMD_NONE;
  void *userdata1 = nullptr;
  void *userdata2 = nullptr;
  
  std::string label;
  std::string tooltip;
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
  float text_size = 1.0f;
  float corner_radius = 0.0f;
  float line_width = 2.0f;
  float line_width_highlight = 2.5f;
  _textalign align = TEXT_CENTER;
  bool visible = true;
  bool enabled = true;
  bool wants_mouse = false;
  bool wants_hover = false;
  bool wants_keyboard_focus = false;
  bool clears_background = false;
  bool redraw_on_show = false;
  bool doublebuffer = false;
  bool mouse_inside = false;
  bool hovered = false;
  bool pressed = false;
  bool external_highlight = false;
  int last_mouse_button = Button1;
};

class c_frame : public c_widget {
public:
  c_frame ();

  void draw (cairo_t *cr) override;

  _widget_state state = WSTATE_NORMAL;
};

class c_valuewidget : public c_widget {
public:
  ~c_valuewidget () override;

  virtual bool set_value (float value, bool notify = false);
  virtual void set_range (float min, float max);
  virtual void set_min (float min);
  virtual void set_max (float max);
  virtual void set_default (float value);
  virtual void set_step (float step);
  virtual float normalized_value () const;
  virtual float value_from_normalized (float normalized) const;
  virtual std::string get_value_string () const;
  virtual std::string get_label_string () const;
  virtual bool parse_value_string (const std::string &text, float &out) const;
  virtual bool hit_value_label (int x, int y) const;
  virtual void show_value_editor ();
  virtual void emit_action ();
  virtual t_rect value_area_rect () const;
  virtual t_rect value_label_rect () const;
  virtual void draw_value_label (cairo_t *cr);
  void on_command (t_command_event &event) override;

  float min = 0.0f;
  float max = 1.0f;
  float value = 0.0f;
  float default_value = 0.0f;
  float step = 0.01f;
  _label_position label_position = LABEL_NONE;
  _textalign label_align = TEXT_CENTER;
  float label_text_size = 1.0f;
  int label_gap = 4;
  bool reset_on_doubleclick = true;
  bool edit_on_label_doubleclick = true;

protected:
  int label_extent () const;
  float quantize_value (float value) const;

private:
  std::unique_ptr<c_valueeditor_popup> value_editor;
};

class c_label : public c_widget {
public:
  void draw (cairo_t *cr) override;
  bool on_mouse_down (int x, int y, int button) override;
  bool on_mouse_move (int x, int y) override;
  bool on_key_down (int key) override;
  void on_mouse_enter () override;
  void on_mouse_leave () override;
  void clear_hover () override;
  _mouse_cursor mouse_cursor () const override;
  bool highlighted () const override;

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
  bool on_key_down (int key) override;
  void on_mouse_enter () override;
  void on_mouse_leave () override;
  void clear_hover () override;
  bool highlighted () const override;
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
  bool mouse_down_inside = false;
  float padding = 8.0f;

protected:
  cairo_surface_t *image_for_state () const;

private:
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

class c_imagebutton : public c_button {
public:
  c_imagebutton ();

  void draw (cairo_t *cr) override;

  bool draw_frame = false;
};

class c_checkbox : public c_button {
public:
  c_checkbox ();

  void draw (cairo_t *cr) override;
  void shrinkwrap (
      int padding_x = 16,
      int padding_y = -1,
      bool center_x = false,
      bool center_y = true) override;
};

class c_container;

struct t_listrow {
  std::string label;
  std::string path;
  size_t size;
  int32_t timestamp;
  bool directory = false;
  bool symlink = false;
};

class c_scrollbar : public c_valuewidget {
public:
  c_scrollbar ();

  void draw (cairo_t *cr) override;
  bool on_mouse_down (int x, int y, int button) override;
  bool on_mouse_up (int x, int y, int button) override;
  bool on_mouse_move (int x, int y) override;
  void on_mouse_leave () override;
  bool highlighted () const override;

  bool set_value (float value, bool notify = false) override;
  void set_page_size (float value);
  void set_step (float value) override;
  void set_container (c_container *container);
  void set_orientation (_scrollbar_orientation orientation);
  virtual t_rect track_rect () const;
  virtual t_rect thumb_rect () const;
  void emit_action () override;

  float page_size = 0.1f;
  _scrollbar_orientation orientation = SCROLLBAR_VERTICAL;
  c_container *container = nullptr;
  bool solid_thumb = false;

protected:
  bool dragging = false;
  int drag_start_x = 0;
  int drag_start_y = 0;
  float drag_start_value = 0.0f;
};

class c_slider : public c_scrollbar {
public:
  c_slider ();

  void draw (cairo_t *cr) override;
  bool on_mouse_down (int x, int y, int button) override;
  bool on_mouse_up (int x, int y, int button) override;
  bool on_mouse_move (int x, int y) override;
  bool on_key_down (int key) override;
  bool highlighted () const override;
  bool set_value (float value, bool notify = false) override;
  void set_range (float min, float max) override;
  void set_min (float min) override;
  void set_max (float max) override;
  void set_step (float step) override;
  float real_value () const;
  std::string get_value_string () const override;
  void emit_action () override;
  virtual t_rect slider_control_rect () const;
  t_rect track_rect () const override;
  virtual t_rect thumb_rect () const;

  float slider_value = 0.0f;
  float slider_step = 0.05f;
  int track_size = 0;

private:
  float quantize (float value) const;
  float normalized_from_real (float value) const;
  float real_from_normalized (float value) const;
  void sync_real_from_normalized ();
  uint64_t last_click_ms = 0;
};

class c_container : public c_widget {
public:
  c_container ();

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
  void resize (int w, int h) override;
  bool highlighted () const override;
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
  bool highlighted () const override;
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
  int measure_dropdown_width ();
  void emit_action ();
  bool on_popup_mouse_down (c_popupwindow *popup, int x, int y, int button);
  bool on_popup_mouse_move (c_popupwindow *popup, int x, int y);
  bool on_popup_mouse_up (c_popupwindow *popup, int x, int y, int button);
  void tick_drag ();

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
  bool select_on_popup_mouse_up = false;
  int drag_scroll_dir = 0;
  uint64_t drag_last_scroll_ms = 0;
  bool wheel_selects_item = false;
  int measured_dropdown_width = 0;
  bool dropdown_width_dirty = true;
};

enum _knob_doubleclick_action {
  _KNOB_DOUBLECLICK_NONE,
  _KNOB_DOUBLECLICK_SETDEFAULT,
  _KNOB_DOUBLECLICK_EDIT
};

class c_knob : public c_valuewidget {
public:
  c_knob ();
  
  virtual void draw (cairo_t *cr, bool show_label, bool show_value);
  void draw (cairo_t *cr) override;
  bool on_mouse_down (int x, int y, int button) override;
  bool on_mouse_up (int x, int y, int button) override;
  bool on_mouse_move (int x, int y) override;
  bool on_key_down (int key) override;
  void on_mouse_leave () override;
  bool highlighted () const override;

  bool set_value (float value, bool notify = false) override;
  float normalized_value () const override;
  float value_from_normalized (float normalized) const override;
  float angle_from_value () const;
  std::string get_value_string () const override;
  std::string get_label_string () const override;
  bool hit_value_label (int x, int y) const override;
  t_rect value_area_rect () const override;
  t_rect value_label_rect () const override;
  void shrinkwrap (
      int padding_x = 16,
      int padding_y = -1,
      bool center_x = false,
      bool center_y = true) override;

  float drag_sensitivity = 0.005f;
  float log_taper = 1.0f;
  bool show_value = true;
  _knob_doubleclick_action doubleclick_action = _KNOB_DOUBLECLICK_SETDEFAULT;

private:
  bool dragging = false;
  int drag_start_y = 0;
  float drag_start_normalized = 0.0f;
  uint64_t last_click_ms = 0;
};

// same as c_knob, except its label reflects its value
class c_simpleknob : public c_knob {
public:
  c_simpleknob ();
  std::string get_label_string () const override;
};

class c_textbox : public c_widget {
public:
  c_textbox ();

  void draw (cairo_t *cr) override;
  bool on_mouse_down (int x, int y, int button) override;
  bool on_mouse_up (int x, int y, int button) override;
  bool on_mouse_move (int x, int y) override;
  bool on_key_down (int key) override;
  bool on_text_input (const char *text) override;
  bool highlighted () const override;

  bool set_text (const char *text);
  const std::string &text () const;

  std::string value;
  std::string accepted_chars;
  size_t cursor = 0;
  size_t selection_anchor = 0;

private:
  void emit_action ();
  bool has_selection () const;
  size_t selection_start () const;
  size_t selection_end () const;
  void clear_selection ();
  bool erase_selection ();
  void select_all ();
  void select_word_at (size_t pos);
  bool word_bounds_at (size_t pos, size_t *start, size_t *end) const;
  void select_word_drag_to (size_t pos);
  size_t cursor_from_x (cairo_t *cr, double text_x) const;
  double text_width_to (cairo_t *cr, size_t pos) const;
  void scroll_cursor_into_view (cairo_t *cr, double clip_w);

  bool selecting = false;
  bool selecting_words = false;
  size_t word_drag_start = 0;
  size_t word_drag_end = 0;
  double scroll_x = 0.0;
  uint64_t last_click_ms = 0;
  int click_count = 0;
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
  virtual ~c_app ();

  virtual void create (int w, int h);
  virtual void draw ();
  virtual void dispatch_mouse_down (int x, int y, int button);
  virtual void dispatch_mouse_up (int x, int y, int button);
  virtual void dispatch_mouse_move (int x, int y);
  virtual bool dispatch_key_down (int key);
  virtual bool dispatch_key_up (int key);
  virtual void dispatch_text_input (const char *text);
  virtual bool focus_next (bool reverse);
  virtual void invalidate_rect (int x, int y, int w, int h);
  virtual void set_mouse_cursor (_mouse_cursor cursor);
  virtual void set_focus (c_widget *widget);
  virtual void clear_focus (c_widget *widget = nullptr);
  virtual void tick ();
  virtual void update_tooltip (c_widget *widget, int root_x, int root_y);
  virtual void hide_tooltip ();
  virtual void ask_string (
      c_widget *response_target,
      int64_t command,
      const std::string &title,
      const std::string &prompt,
      const std::string &initial_value = "",
      const std::string &accept_chars = "");
  virtual void ask_yes_no (
      c_widget *response_target,
      int64_t command,
      const std::string &title,
      const std::string &question,
      const std::string &cancel_text = "",
      const std::string &no_text = "No",
      const std::string &yes_text = "Yes");

  virtual void show_message (const std::string title, const std::string msg);

  virtual std::unique_ptr<c_popupwindow> create_popup (c_widget *owner);
  virtual std::unique_ptr<c_tooltip> create_tooltip (c_widget *owner);
  virtual t_point root_to_screen (t_point p) const;
  virtual t_point screen_to_root (t_point p) const;
  virtual t_rect screen_bounds_at (t_point p) const;
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
  t_native_app *native_app = nullptr;
  t_native_display display = nullptr;
  c_nativewindow *embedded_window = nullptr;
  c_toplevelwindow *active_toplevel = nullptr;
  c_toplevelwindow *action_toplevel = nullptr;
  c_widget *root = nullptr;
  std::vector<std::unique_ptr<c_widget>> widgets;
  std::vector<std::unique_ptr<c_popupwindow>> popups;
  std::unique_ptr<c_tooltip> tooltip_popup;
  std::unique_ptr<c_askstring_dialog> askstring_dialog;
  std::unique_ptr<c_askyesno_dialog> askyesno_dialog;
  bool show_tooltips = true;
  c_widget *focused_widget = nullptr;
  c_widget *hovered_widget = nullptr;
  c_widget *tooltip_widget = nullptr;
  c_widget *tooltip_pending_widget = nullptr;
  c_widget *mouse_capture_widget = nullptr;
  c_combobox *active_combobox = nullptr;
  c_menu *active_menu = nullptr;
  uint64_t tooltip_pending_since = 0;
  int tooltip_root_x = 0;
  int tooltip_root_y = 0;
  uint64_t tooltip_delay = 400;
  bool mouse_captured = false;
  bool key_shift = false;
  bool key_ctrl = false;
  bool key_alt = false;
  float fontsize = 13.0f;
  float font_scale = 1.0f;
};

class c_native_backend {
public:
  virtual ~c_native_backend () = default;

  virtual void init_app (t_native_app *app) = 0;
  virtual void shutdown_app (t_native_app *app) = 0;
  virtual void run_events (t_native_app *app) = 0;
  virtual void flush_dirty (t_native_app *app) = 0;
  virtual bool is_running (const t_native_app *app) const = 0;
  virtual t_native_display display (const t_native_app *app) const = 0;
  virtual t_native_window default_root_window (t_native_display display) const = 0;
  virtual bool window_size (
      t_native_display display,
      t_native_window window,
      double hdpi,
      int *w,
      int *h) const = 0;
  virtual void invalidate (t_native_handle widget) = 0;
  virtual void flush (t_native_handle widget) = 0;
  virtual bool grab_pointer (
      t_native_handle widget,
      bool owner_events = false) = 0;
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
  virtual void set_keyboard_focus (t_native_handle widget) = 0;
  virtual t_native_window root_window (t_native_handle widget, bool is_widget) const = 0;
  virtual t_point root_to_screen (t_native_handle widget, t_point p) const = 0;
  virtual t_point screen_to_root (t_native_handle widget, t_point p) const = 0;
  virtual t_rect screen_bounds_at (t_native_handle widget, t_point p) const = 0;
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
  virtual bool pointer_grab_owner_events () const;
  bool on_mouse_up (int x, int y, int button) override;
  void on_action (t_action_event &event) override;
  void invalidate_rect (int x, int y, int w, int h) override;
  void move_resize (int x, int y, int w, int h) override;
  void show () override;
  void hide () override;

  static void cb_expose (void *w, void *user_data);
  static void cb_button_press (void *w, void *event, void *user_data);
  static void cb_button_release (void *w, void *event, void *user_data);
  static void cb_motion (void *w, void *event, void *user_data);
  static void cb_key_press (void *w, void *event, void *user_data);
  static void cb_key_release (void *w, void *event, void *user_data);

  c_widget *owner = nullptr;
  t_native_handle widget = nullptr;
  bool pointer_grabbed = false;
  bool close_on_release = false;
};

enum _menuitem_type {
  MENUITEM_REGULAR,
  MENUITEM_CHECK,
  MENUITEM_RADIO,
  MENUITEM_SEPARATOR
};

struct t_menuitem {
  std::string label;
  int64_t command = NBTK_CMD_NONE;
  c_menu *submenu = nullptr;
  bool enabled = true;
  _menuitem_type type = MENUITEM_REGULAR;
  bool checked = false;
};

class c_menulistbox : public c_listbox {
public:
  void draw (cairo_t *cr) override;
  bool on_mouse_move (int x, int y) override;
  bool on_key_down (int key) override;
  void on_select (int index) override;
  void on_activate (int index) override;

  c_menu *menu = nullptr;
};

class c_menu : public c_popupwindow {
public:
  ~c_menu () override;

  void configure (c_app *app, c_widget *command_target, c_menu *parent = nullptr);
  int add_item (
      const std::string &label,
      int64_t command,
      _menuitem_type type = MENUITEM_REGULAR,
      bool enabled = true,
      bool checked = false);
  int add_separator ();
  c_menu *add_submenu (const std::string &label, bool enabled = true);
  void clear ();
  void show_below (c_widget *anchor);
  void show_beside (c_menu *parent, int row);
  void select_item (int index);
  void activate_item (int index);
  c_menu *menu_at_screen (t_point point);
  void close_tree ();
  bool item_selectable (int index) const;
  int next_selectable (int from, int direction) const;
  bool on_key_down (int key) override;
  bool pointer_grab_owner_events () const override;
  void close () override;
  void hide () override;

  std::vector<t_menuitem> items;
  c_menulistbox listbox;
  c_widget *command_target = nullptr;
  c_menu *parent_menu = nullptr;
  c_menu *open_submenu = nullptr;
  int parent_row = -1;
  int row_height = 24;
  int min_width = 120;
  int max_width = 480;

private:
  void ensure_native ();
  void sync_geometry (int max_height);
  void show_submenu (int index);
  c_menu *root_menu ();

  std::vector<std::unique_ptr<c_menu>> owned_submenus;
};

class c_topmenu : public c_button {
public:
  c_topmenu ();
  ~c_topmenu () override;

  void draw (cairo_t *cr) override;
  bool on_mouse_down (int x, int y, int button) override;
  bool on_mouse_up (int x, int y, int button) override;
  bool on_key_down (int key) override;
  void on_mouse_enter () override;
  c_menu *get_menu ();
  int add_item (
      const std::string &label,
      int64_t command,
      _menuitem_type type = MENUITEM_REGULAR,
      bool enabled = true,
      bool checked = false);
  int add_separator ();
  c_menu *add_submenu (const std::string &label, bool enabled = true);
  void show_menu ();
  void hide_menu ();
  void menu_closed ();

  bool menu_visible = false;

private:
  std::unique_ptr<c_menu> popup_menu;
};

class c_menubar : public c_container {
public:
  ~c_menubar () override;

  c_topmenu *add_menu (const std::string &label);
  void layout_menus ();
  void move_resize (int x, int y, int w, int h) override;
  void open_menu (c_topmenu *menu);
  void menu_closed (c_topmenu *menu);
  void close_menus ();

  int item_padding = 20;
  int item_gap = 0;
  c_topmenu *open_topmenu = nullptr;

private:
  std::vector<std::unique_ptr<c_topmenu>> menus;
};

class c_valueeditor_popup : public c_popupwindow {
public:
  void create_for_value (c_app *app, c_valuewidget *owner);
  void show_near_owner ();
  void commit ();
  bool on_key_down (int key) override;
  void on_action (t_action_event &event) override;

  c_valuewidget *value_widget = nullptr;
  c_textbox textbox;
  bool ignore_next_mouse_up = false;
};

class c_tooltip : public c_popupwindow {
public:
  bool close_on_outside_click () const override;
  bool takes_focus () const override;
  void show () override;
  void set_text (const char *text);

  c_frame frame;
  c_label label;
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
  
  virtual void hide () { c_widget::hide (); }
  virtual void show () { c_widget::show (); }
  
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
  
  inline void cairo_move_to (cairo_t *cr, float x, float y) {
    ::cairo_move_to (cr, x + 0.5, y + 0.5);
  }
  inline void cairo_line_to (cairo_t *cr, float x, float y) {
    ::cairo_line_to (cr, x + 0.5, y + 0.5);
  }
  
  int opacity = 255;
  int mouse_x = -1;
  int mouse_y = -1;
  int mousedown_x [8] = { 0 };
  int mousedown_y [8] = { 0 };
  int mouse_buttons = 0;
  int click_distance = 5;
  bool base_image_valid = false;
};

typedef struct t_gradientcolors {
  float r1 = 0.00, g1 = 0.00, b1 = 0.00, a1 = 1.00,
        r2 = 0.00, g2 = 0.00, b2 = 0.00, a2 = 1.00;
  void swap ();
} t_gradientcolors;

typedef struct {
  float r = 0.0f;
  float g = 0.0f;
  float b = 0.0f;
} t_rgb;

typedef struct {
  float h = 0.0f;
  float s = 0.0f;
  float l = 0.0f;
} t_hsl;

typedef struct {
  t_gradientcolors bg;
  t_gradientcolors fg;
  t_gradientcolors outline;
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
  t_gradientcolors text_fg;
  t_gradientcolors text_disabled;
  t_gradientcolors link_fg;

  t_controlcolors button;
  t_controlcolors radio;

  t_statecolors frame_normal;
  t_statecolors frame_selected;
  t_statecolors frame_disabled;
} t_colortheme;

extern t_colortheme *g_colors;

void colortheme_apply (float r, float g, float b,
                       float h, float s, float l,
                       float contrast, float contrast_mid = 0.3,
                       bool flip = false);

#ifndef NB_DEBUG_RATE_HELPERS
#define NB_DEBUG_RATE_HELPERS
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
#endif

#ifndef DEBUG_SHOW_RATE
#ifdef DEBUG
#define DEBUG_SHOW_RATE(x) {static c_printfps fps(x);fps.tick();}
#else
#define DEBUG_SHOW_RATE(x)
#endif
#endif

const t_colortheme *get_colortheme ();
t_hsl rgb_to_hsl (float r, float g, float b);
t_rgb hsl_to_rgb (float h, float s, float l);
std::string path_dirname (const std::string &path);
std::string path_basename (const std::string &path);
void tk_path_rounded_rect (cairo_t *cr,
    double x, double y, double w, double h,
    double r);
void tk_set_gradient (cairo_t *cr, double h, const t_gradientcolors &colors);

class c_native_toplevelwindow {
public:
  virtual ~c_native_toplevelwindow () = default;

  bool create (
      c_app *app,
      nbtk::t_native_window parent,
      const char *title,
      int x, int y, int w, int h,
      nbtk::t_native_handle owner = nullptr);

  void set_min_size_to_current ();
  virtual void set_min_size (int w, int h);
  void center_over (nbtk::t_native_handle owner);
  void center_over_transient_owner ();
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
  void set_mouse_cursor (nbtk::_mouse_cursor cursor);
  void clear_focus ();
  int x ();
  int y ();
  int w ();
  int h ();
  
  virtual void on_expose ();
  virtual void on_resize ();
  virtual void on_configure_notify ();
  virtual void on_action (nbtk::t_action_event &event);
  virtual void on_command (nbtk::t_command_event &event);
  virtual void on_close ()  {};
  
  static void cb_expose (void *w, void *user_data);
  static void cb_resize (void *w, void *user_data);
  static void cb_configure_notify (void *w, void *user_data);
  static void cb_key_press (void *w, void *event, void *user_data);
  static void cb_close (void *w, void *user_data);
  
  uint64_t id = 0;
  std::string label;
  c_app *app_context = NULL;
  c_native_backend *backend = NULL;
  nbtk::t_native_handle widget = nullptr;
  nbtk::t_native_handle transient_owner = nullptr;
  nbtk::t_native_window window = 0;
  nbtk::t_native_window parent = 0;
};

class c_toplevelwindow : public c_native_toplevelwindow {
public:
  ~c_toplevelwindow ();

  bool create (
      c_app *app,
      nbtk::t_native_window parent,
      const char *title,
      int x, int y, int w, int h,
      nbtk::t_native_handle owner = nullptr);

	  void on_resize () override;
	  void on_configure_notify () override;
	  void on_expose () override;
	  void on_close () override;
	  virtual bool on_key_down (int key);
	  virtual void on_hover_changed (nbtk::c_widget *hovered);
	  void set_min_size (int w, int h) override;
  void redraw_child (nbtk::c_widget &child, int pad = 1);
  void redraw_child_rect (
      nbtk::c_widget &child,
      int x,
      int y,
      int w,
      int h,
      int pad = 1);
  void activate ();
  void save_state ();
  void auto_hide_on_close (bool b = true);
  void auto_quit_on_close (bool b = true);

  static void cb_button_press (void *w, void *event, void *user_data);
  static void cb_button_release (void *w, void *event, void *user_data);
  static void cb_motion (void *w, void *event, void *user_data);
  static void cb_enter (void *w, void *user_data);
  static void cb_leave (void *w, void *user_data);
  static void cb_key_press (void *w, void *event, void *user_data);
  static void cb_key_release (void *w, void *event, void *user_data);

  c_app *app = NULL;
  nbtk::c_widget root_widget;
  nbtk::c_widget *focused_widget = NULL;
  nbtk::c_widget *hovered_widget = NULL;
  nbtk::c_widget *mouse_capture_widget = NULL;
  bool mouse_captured = false;
  bool hide_on_close = true;
  bool quit_on_close = false;
  bool handling_close = false;

private:
  void clear_buffer ();
  bool ensure_buffer (int w, int h);

  cairo_surface_t *buffer_surface = NULL;
  cairo_t *buffer_cr = NULL;
  int buffer_surface_w = 0;
  int buffer_surface_h = 0;
};

class c_askstring_dialog : public c_toplevelwindow {
public:
  bool create (
      c_app *app,
      t_native_window parent,
      t_native_handle owner);
  void ask (
      c_widget *response_target,
      int64_t command,
      const std::string &title,
      const std::string &prompt,
      const std::string &initial_value,
      const std::string &accept_chars = "");
  void on_resize () override;
  void on_action (t_action_event &event) override;
  bool on_key_down (int key) override;
  void on_close () override;

private:
  void finish (_command_result result);

  c_widget *response_target = nullptr;
  int64_t response_command = NBTK_CMD_NONE;
  c_frame frame;
  c_label label_prompt;
  c_textbox textbox;
  c_button btn_ok;
  c_button btn_cancel;
};

class c_askyesno_dialog : public c_toplevelwindow {
public:
  bool create (
      c_app *app,
      t_native_window parent,
      t_native_handle owner);
  void ask (
      c_widget *response_target,
      int64_t command,
      const std::string &title,
      const std::string &question,
      const std::string &cancel_text = "",
      const std::string &no_text = "No",
      const std::string &yes_text = "Yes");
  void on_resize () override;
  void on_action (t_action_event &event) override;
  bool on_key_down (int key) override;
  void on_close () override;

private:
  void finish (_command_result result);

  c_widget *response_target = nullptr;
  int64_t response_command = NBTK_CMD_NONE;
  c_frame frame;
  c_label label_question;
  c_button btn_yes;
  c_button btn_no;
  c_button btn_cancel;
};

class c_filepicker : public c_toplevelwindow {
public:
  class c_path_textbox : public c_textbox {
  public:
    bool on_tab (bool shift) override;

    c_filepicker *filepicker = nullptr;
  };

  void create (
      c_app *app,
      t_native_window parent,
      t_native_handle owner,
      const char *title);

  virtual void show ();

  void show_open (
      c_widget *response_target,
      int64_t command);

  void show_save_as (
      c_widget *response_target,
      int64_t command,
      const std::string &filename = "");

  void show_save_as (const std::string &filename = "");
  void set_save_as (bool enabled, const std::string &filename = "");
  void set_save_suffix (std::string suffix);
  void hide ();
  void on_resize () override;
  void on_action (t_action_event &event) override;
  void on_command (t_command_event &event) override;
  bool on_key_down (int key) override;
  void on_close () override;

  void scan_current_dir ();
  bool complete_path_from_textbox ();
  virtual void add_files_from_dir (
      c_combobox *cb,
      const std::string &selected_file = "");
  void clear_allowed_filters ();
  void add_allowed_filter (std::string filter, std::string filter_label);
  void set_active_filter (int index);
  std::string get_current_dir () const;
  virtual void set_current_dir (std::string str);
  bool is_visible () const;
  std::string selected_path () const;

  virtual void on_file_select (const std::string &filename);

  c_frame frame;
  //c_label label_path;
  c_path_textbox text_path;
  c_listbox listbox;
  c_scrollbar vscrollbar;
  c_label label_filename;
  c_textbox text_filename;
  c_combobox combo_filter;
  c_checkbox btn_show_hidden;
  c_button btn_ok;
  c_button btn_cancel;

  std::string title;
  std::string current_dir;
  std::string combo_dir;
  std::vector<std::string> filter_labels;
  std::vector<std::vector<std::string>> filter_suffixes;
  std::vector<std::string> accepted_suffixes;
  std::vector<std::string> filelist;
  std::vector<t_listrow> rows;
  t_native_handle owner_widget = nullptr;
  bool show_hidden = false;
  bool sort_case_sensitive = false;
  bool save_as = false;
  std::string save_suffix;
  std::string pending_save_path;
  std::string last_completion_text;
  std::string last_completion_dir_path;
  bool last_completion_was_dir = false;
  bool last_completion_can_focus_list = false;

private:
  void finish (
      _command_result result,
      const std::string &path = "");
  void accept_path (const std::string &path);

  c_widget *response_target = nullptr;
  int64_t response_command = NBTK_CMD_NONE;
};

} // namespace nbtk
