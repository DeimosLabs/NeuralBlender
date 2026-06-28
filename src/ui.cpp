
/* NeuralBlender - RTNeural / NAM based amp modeler
 *
 * Shared UI code
 */

#include <string.h>
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>

#include "neuralblender.h"
#include "ui.h"

#include "xdrawing_area.h"
#include "xpngloader.h"
#include "widgets/xcombobox_private.h"

#include "data/data.h"

#ifdef CMDLINE_DEBUG
#include "cmdline/cmdline.h"
#define debug(...) cmdline_debug(stderr,ANSI_MAGENTA,__FILE__,__LINE__,__FUNC__,__VA_ARGS__)
#else
#define debug(...)
#define CP
#define BP
#endif

static uint64_t get_unique_id ();
static void button_mouse_up (void *w_, void *event, void *user_data);
static void button_double_click (void *w_, void *event, void *user_data);
static void button_value_changed (void *w_, void *value);
static void knob_value_changed (void *w_, void *value);
static void knob_double_click (void *w_, void *event, void *user_data);
static std::string path_dirname (const std::string &path);
static std::string path_basename (const std::string &path);

extern const char *g_build_timestamp;

static void set_widget_color_all_states (
    Widget_t *w,
    Color_mod mod,
    const float r,
    const float g,
    const float b,
    const float a = 1.0f) {

  if (!w)
    return;

  set_widget_color (w, NORMAL_,      mod, r, g, b, a);
  set_widget_color (w, PRELIGHT_,    mod, r, g, b, a);
  set_widget_color (w, SELECTED_,    mod, r, g, b, a);
  set_widget_color (w, ACTIVE_,      mod, r, g, b, a);
  set_widget_color (w, INSENSITIVE_, mod, r, g, b, a);
}

static void inherit_parent_bg_color (Widget_t *w, Widget_t *parent) {
  if (!w || !parent || !parent->color_scheme)
    return;

  Colors *c = get_color_scheme (parent, NORMAL_);
  if (!c)
    return;

  set_widget_color_all_states (
      w, BACKGROUND_, c->bg [0], c->bg [1], c->bg [2], c->bg [3]);
}

// this one must be called AFTER add_* (Widget_t *, ...) in child create functions
void c_widget::create (
    c_neuralblender_ui *ui_,
    Widget_t *parent,
    const char *label_,
    int x, int y, int w, int h) {

  debug ("label_='%s'", label_);
  id = get_unique_id ();
  label = label_ ? label_ : "";
  ui = ui_;
  if (!ui) { debug ("!ui"); }
  if (!widget) {
    debug ("!widget");
    return;
  }
  
  widget->scale.gravity = NONE;
  widget->parent_struct = this;
  widget->label = label.c_str ();
}

bool c_widget::set_label (const char *label_) {
  label = label_ ? label_ : "";
  if (!widget)
    return false;

  widget->label = label.c_str ();
  expose_widget (widget);
  return true;
}

void c_widget::set_bg_color (const float r, const float g, const float b) {
  set_widget_color_all_states (widget, BACKGROUND_, r, g, b);
  if (widget)
    expose_widget (widget);
}

void c_widget::set_fg_color (const float r, const float g, const float b) {
  set_widget_color_all_states (widget, TEXT_, r, g, b);
  set_widget_color_all_states (widget, FORGROUND_, r, g, b);
  if (widget)
    expose_widget (widget);
}

void c_widget::move_resize (int x, int y, int w, int h) {
  if (!widget)
    return;

  const int sx = x * widget->app->hdpi;
  const int sy = y * widget->app->hdpi;
  const int sw = std::max (1, (int) (w * widget->app->hdpi));
  const int sh = std::max (1, (int) (h * widget->app->hdpi));

  widget->x = sx;
  widget->y = sy;
  widget->scale.init_x = sx;
  widget->scale.init_y = sy;
  widget->scale.init_width = sw;
  widget->scale.init_height = sh;

  os_move_window (widget->app->dpy, widget, sx, sy);
  os_resize_window (widget->app->dpy, widget, sw, sh);
  widget->func.configure_callback (widget, NULL);
  expose_widget (widget);
}

void c_widget::move (int x, int y) {
  if (!widget)
    return;

  const int sx = x * widget->app->hdpi;
  const int sy = y * widget->app->hdpi;

  widget->x = sx;
  widget->y = sy;
  widget->scale.init_x = sx;
  widget->scale.init_y = sy;

  os_move_window (widget->app->dpy, widget, sx, sy);
  expose_widget (widget);
}

void c_widget::resize (int w, int h) {
  if (!widget)
    return;

  const int sw = std::max (1, (int) (w * widget->app->hdpi));
  const int sh = std::max (1, (int) (h * widget->app->hdpi));

  widget->scale.init_width = sw;
  widget->scale.init_height = sh;

  os_resize_window (widget->app->dpy, widget, sw, sh);
  widget->func.configure_callback (widget, NULL);
  expose_widget (widget);
}

void c_frame::create (
    c_neuralblender_ui *ui_,
    Widget_t *parent,
    const char *label_,
    int x, int y, int w, int h) {
  
  widget = add_frame (parent, label_ ? label_ : "", x, y, w, h);
  
  c_widget::create (ui_, parent, label_, x, y, w, h);
  inherit_parent_bg_color (widget, parent);
}

void c_frame::set_bg_color (const float r, const float g, const float b) {
  set_widget_color_all_states (widget, BACKGROUND_, r, g, b);
  set_widget_color_all_states (widget, FRAME_, r, g, b);
  if (widget)
    expose_widget (widget);
}

void c_frame::set_fg_color (const float r, const float g, const float b) {
  set_widget_color_all_states (widget, TEXT_, r, g, b);
  set_widget_color_all_states (widget, FRAME_, r, g, b);
  if (widget)
    expose_widget (widget);
}

void c_meter::create (
    c_neuralblender_ui *ui_,
    Widget_t *parent,
    const char *label_,
    int x, int y, int w, int h) {

  if (!parent || !parent->app)
    return;

  widget = create_widget (parent->app, parent, x, y, w, h);
  c_widget::create (ui_, parent, label_, x, y, w, h);
  meter.create (widget, label_, 0, 0, w, h);
}

void c_meter::move_resize (int x, int y, int w, int h) {
  c_widget::move_resize (x, y, w, h);

  Widget_t *child = meter.widget;
  if (!child)
    return;

  const int sw = std::max (1, (int) (w * child->app->hdpi));
  const int sh = std::max (1, (int) (h * child->app->hdpi));

  child->x = 0;
  child->y = 0;
  child->scale.init_x = 0;
  child->scale.init_y = 0;
  child->scale.init_width = sw;
  child->scale.init_height = sh;

  os_move_window (child->app->dpy, child, 0, 0);
  os_resize_window (child->app->dpy, child, sw, sh);
  child->func.configure_callback (child, NULL);
  expose_widget (child);
}

void c_meter::show () {
  if (widget)
    widget_show (widget);
  if (meter.widget)
    widget_show (meter.widget);
  if (widget)
    expose_widget (widget);
  if (meter.widget)
    expose_widget (meter.widget);
}

void c_meter::hide () {
  if (meter.widget)
    widget_hide (meter.widget);
  if (widget)
    widget_hide (widget);
}

void c_container::create (
    c_neuralblender_ui *ui_,
    Widget_t *parent,
    const char *label_,
    int x, int y, int w, int h) {
    
  widget = create_widget (parent->app, parent, x, y, w, h);
  c_widget::create (ui_, parent, label_, x, y, w, h);
  inherit_parent_bg_color (widget, parent);
}

void c_container::set_bg_color (const float r, const float g, const float b) {
  set_widget_color_all_states (widget, BACKGROUND_, r, g, b);
  if (widget)
    expose_widget (widget);
}


void c_label::create (
    c_neuralblender_ui *ui_,
    Widget_t *parent,
    const char *label_,
    int x, int y, int w, int h) {

  role = ROLE_UNKNOWN;
  widget = add_label (parent, label.c_str (), x, y, w, h);
  widget->func.expose_callback = c_label::draw;
  c_widget::create (ui_, parent, label_, x, y, w, h);
  inherit_parent_bg_color (widget, parent);
}

void c_label::set_bg_color (const float r, const float g, const float b) {
  set_widget_color_all_states (widget, BACKGROUND_, r, g, b);
  if (widget)
    expose_widget (widget);
}

void c_label::set_fg_color (const float r, const float g, const float b) {
  set_widget_color_all_states (widget, TEXT_, r, g, b);
  if (widget)
    expose_widget (widget);
}

void c_image::create (
    c_neuralblender_ui *ui,
    Widget_t *parent,
    const char *label,
    int x, int y, int w, int h) {
  widget = add_image (parent, label, x, y, w, h);
  c_widget::create (ui, parent, label, x, y, w, h);
}

void c_image::set_png (const unsigned char *png) {
  if (!widget || !png)
    return;

  widget_get_png(widget, png);
  expose_widget(widget);
}

void c_button::create (
    c_neuralblender_ui *ui_,
    Widget_t *parent,
    const char *label_,
    int x, int y, int w, int h,
    _button_style style) {

  switch (style) {
    case BTN_CHECKBOX:
      is_toggle = true;
      widget = add_check_button (parent, label_ ? label_ : "", x, y, w, h);
      widget->func.value_changed_callback = button_value_changed;
    break;

    case BTN_TOGGLE:
      is_toggle = true;
      widget = add_toggle_button (parent, label_ ? label_ : "", x, y, w, h);
      widget->func.value_changed_callback = button_value_changed;
    break;

    default:
      is_toggle = false;
      widget = add_button (parent, label_ ? label_ : "", x, y, w, h);
      widget->func.double_click_callback = button_double_click;
      widget->func.button_release_callback = button_mouse_up;
    break;
  }

  c_widget::create (ui_, parent, label_, x, y, w, h);
}

void c_button::on_mouseup () {
  if (!ui) {
    debug ("!ui");
    return;
  }

  if (ui->updating_from_state)
    return;

  switch (role) {
    case ROLE_BYPASS: CP
      ui->on_bypass (this, value);
      set_label (value ? "Enabled" : "Bypass");
    break;

    case ROLE_MUTE: CP
      ui->on_mute (this, value);
      if (lane >= 0 && lane < NB_UI_MAX_LANES) {
        ui->state.lanes [lane].lane_mute = value;
      }
    break;

    case ROLE_DCFLIP: CP
      ui->on_dcflip (this, value);
      if (lane >= 0 && lane < NB_UI_MAX_LANES) {
        ui->state.lanes [lane].dcflip = value;
      }
    break;

    case ROLE_CALIBRATE: CP
      ui->on_calibrate (this, value);
      if (lane >= 0 && lane < NB_UI_MAX_LANES) {
        ui->state.lanes [lane].do_calib = value;
      }
    break;

    case ROLE_MUTEALL: CP
      /*for (size_t i = 0; i < NB_UI_MAX_LANES; i++)
        ui->lanes [i].meter_out.set_l (0, 0);
      ui->on_muteall (this, value);*/
      ui->on_muteall (this, value);
    break;

    case ROLE_BROWSE: CP
      //ui->on_filebrowse_pre (this);
      ui->on_filebrowse (this);
      if (filepicker)
        filepicker->show ();
      else
        debug ("!filepicker");
    break;

    case ROLE_CLEAR: CP
      ui->on_fileclear (this);
    break;

    case ROLE_ABOUT: CP
      ui->aboutwindow.show ();
      ui->on_about (this);
    break;

    case ROLE_ABOUTOK: CP
      ui->aboutwindow.hide ();
    break;

    case ROLE_VUTOGGLE: CP
      ui->vu_on (value);
    break;

    case ROLE_EXCL_TOGGLE: CP
      if (value) {
        size_t exclusive_lane = ui->choose_exclusive_lane ();
        ui->on_excl (this, exclusive_lane); // this is 1-BASED, 0 = normal mode
      } else {
        ui->on_excl (this, 0);
      }
    break;

    case ROLE_EXCL_USE:
      ui->on_excl_use (this, value);
    break;
    
    case ROLE_ADV_TOGGLE:
      ui->on_advanced (this, value);
    break;

    default: CP
    break;
  }
  //ui->sync_widgets_from_state (ui->state);
}

void xevfunc_dummy (void *a, void *b)          { }
void evfunc_dummy (void *a, void *b, void *c)  { }

bool c_button::set_value (bool value_) {
  if (!widget || !widget->adj)
    return false;

  value = value_;

  // avoid firing unwanted callbacks
  xevfunc oldvaluechanged = widget->func.value_changed_callback;
  xevfunc oldadj = widget->func.adj_callback;
  widget->func.value_changed_callback = xevfunc_dummy;
  widget->func.adj_callback = xevfunc_dummy;

  adj_set_value (widget->adj, this->value ? 1.0f : 0.0f);
  widget->state = this->value ? 3 : 0;
  expose_widget (widget);
  widget->func.value_changed_callback = oldvaluechanged;
  widget->func.adj_callback = oldadj;

  return true;
}

void c_button::set_bg_color (const float r, const float g, const float b) {
  set_widget_color_all_states (widget, LIGHT_, r, g, b);
  if (widget)
    expose_widget (widget);
}

void c_button::set_fg_color (const float r, const float g, const float b) {
  set_widget_color_all_states (widget, TEXT_, r, g, b);
  set_widget_color_all_states (widget, FORGROUND_, r, g, b);
  if (widget)
    expose_widget (widget);
}

void c_knob::create (
    c_neuralblender_ui *ui_,
    Widget_t *parent,
    const char *label_,
    int x, int y, int w, int h) {

  label = label_;
  widget = add_knob (parent, label.c_str (), x, y, w, h);
  widget->func.value_changed_callback = knob_value_changed;
  widget->func.double_click_callback = knob_double_click;
  c_widget::create (ui_, parent, label_, x, y, w, h);
}

void c_knob::set_bg_color (const float r, const float g, const float b) {
  set_widget_color_all_states (widget, BASE_, r, g, b);
  if (widget)
    expose_widget (widget);
}

void c_knob::set_fg_color (const float r, const float g, const float b) {
  set_widget_color_all_states (widget, FORGROUND_, r, g, b);
  set_widget_color_all_states (widget, TEXT_, r, g, b);
  if (widget)
    expose_widget (widget);
}

void c_knob::set_min (float x) {
  min = x;
  adj_set_min_value (widget->adj, x);
}

void c_knob::set_max (float x) {
  max = x;
  adj_set_max_value (widget->adj, x);
}

void c_knob::set_value (float x) {
  if (!widget || !widget->adj) {
    value = x;
    return;
  }

  adj_set_value (widget->adj, x);
  value = adj_get_value (widget->adj);
  expose_widget (widget);
}

void c_knob::set_defaultvalue (float x) {
  defaultvalue = x;
}

void c_knob::set_step (float x) {
  step = widget->adj->step = x;
}

void c_knob::on_change () {
  //debug ("value=%f", value);
  if (ui && ui->updating_from_state)
    return;

  float g = db_to_gain (value);
  switch (role) {
    case ROLE_GAIN_IN:
      debug ("lane %d gain in %f", lane, g);
      ui->on_gain_in (this, g);
    break;

    case ROLE_GAIN_OUT:
      debug ("lane %d gain out %f", lane, g);
      ui->on_gain_out (this, g);
    break;

    case ROLE_DELAY:
      debug ("lane %d delay %f", lane, value);
      ui->on_delay (this, value);
    break;

    default:
      debug ("unknown knob set to %f", g);
    break;
  }
}

void c_knob::on_doubleclick () { CP
  if (reset_on_doubleclick)
    set_value (defaultvalue);
}

void c_combobox::create (
    c_neuralblender_ui *ui_,
    Widget_t *parent,
    const char *label_,
    int x, int y, int w, int h) {

  label = label_;
  widget = add_combobox (parent, label.c_str (), x, y, w, h);
  c_widget::create (ui_, parent, label_, x, y, w, h);
  combobox_set_menu_size (widget, 16);

  update_widget ();
}

void c_combobox::set_bg_color (const float r, const float g, const float b) {
  set_widget_color_all_states (widget, SHADOW_, r, g, b);
  set_widget_color_all_states (widget, BASE_, r, g, b);
  set_widget_color_all_states (widget, BACKGROUND_, r, g, b);

  if (widget && widget->childlist && widget->childlist->elem > 0)
    set_widget_color_all_states (widget->childlist->childs [0], LIGHT_, r, g, b);

  if (widget && widget->childlist && widget->childlist->elem > 1) {
    Widget_t *menu = widget->childlist->childs [1];
    set_widget_color_all_states (menu, BACKGROUND_, r, g, b);
    if (menu && menu->childlist && menu->childlist->elem > 0) {
      Widget_t *view_port = menu->childlist->childs [0];
      set_widget_color_all_states (view_port, BASE_, r, g, b);
      set_widget_color_all_states (view_port, BACKGROUND_, r, g, b);
    }
  }

  if (widget)
    expose_widget (widget);
}

void c_combobox::set_fg_color (const float r, const float g, const float b) {
  set_widget_color_all_states (widget, TEXT_, r, g, b);
  set_widget_color_all_states (widget, FRAME_, r, g, b);

  if (widget && widget->childlist && widget->childlist->elem > 0) {
    set_widget_color_all_states (widget->childlist->childs [0], TEXT_, r, g, b);
    set_widget_color_all_states (widget->childlist->childs [0], FORGROUND_, r, g, b);
  }

  if (widget && widget->childlist && widget->childlist->elem > 1) {
    Widget_t *menu = widget->childlist->childs [1];
    set_widget_color_all_states (menu, TEXT_, r, g, b);
    set_widget_color_all_states (menu, FRAME_, r, g, b);
    if (menu && menu->childlist && menu->childlist->elem > 0) {
      Widget_t *view_port = menu->childlist->childs [0];
      set_widget_color_all_states (view_port, TEXT_, r, g, b);
      set_widget_color_all_states (view_port, FRAME_, r, g, b);
    }
  }

  if (widget)
    expose_widget (widget);
}

// work around xputty's weirdness
void c_combobox::move_resize (int x, int y, int w, int h) {
  c_widget::move_resize (x, y, w, h);

  if (!widget || !widget->childlist || widget->childlist->elem < 1)
    return;

  Widget_t *button = widget->childlist->childs [0];
  if (!button)
    return;

  const int bw = 20;
  const int sx = std::max (0, (int) ((w - bw) * widget->app->hdpi));
  const int sw = std::max (1, (int) (bw * widget->app->hdpi));
  const int sh = std::max (1, (int) (h * widget->app->hdpi));

  button->x = sx;
  button->y = 0;
  button->scale.init_x = sx;
  button->scale.init_y = 0;
  button->scale.init_width = sw;
  button->scale.init_height = sh;

  os_move_window (button->app->dpy, button, sx, 0);
  os_resize_window (button->app->dpy, button, sw, sh);
  button->func.configure_callback (button, NULL);
  expose_widget (button);
  expose_widget (widget);
}

void c_combobox::clear () { CP
  items.clear ();
  selected = -1;
  update_widget ();
}

void c_combobox::add (const std::string &str) {
  debug ("str=%s", str.c_str ());
  items.push_back (str);
  update_widget ();
}

void c_combobox::on_change (int x) {
  debug ("x=%d", x);
  if (ui && ui->updating_from_state)
    return;

  set_selection (x);

  if (x < 0 || x >= (int) items.size ()) {
    debug ("item out of range: %d", x);
    return;
  }
  std::string fullpath;
  if (strip_directories) // yay spaghetti
    fullpath = ui->filepickers [lane].current_dir + "/" + items [x];
  else
    fullpath = items [x];

  ui->load_model (lane, fullpath.c_str ());
}

void c_combobox::set_selection (int n) {
  if (n >= 0 && n < (int) items.size())
    selected = n;
  else
    selected = -1;

  update_widget();
}

int c_combobox::get_selection () {
  return selected;
}

void c_combobox::update_widget () {
  int i, n = items.size ();
  debug ("%d items", n);

  updating_widget = true;
  combobox_delete_entrys (widget);
  for (i = 0; i < n; i++) {
    combobox_add_entry (widget, items [i].c_str ());
  }

  // more xputty internals... thanks to codex for the help on this!
  if (selected >= 0 && selected < i) {
    combobox_set_active_entry (widget, selected);

    Widget_t *menu = widget->childlist->childs [1];
    Widget_t *view_port = menu->childlist->childs [0];
    ComboBox_t *list = (ComboBox_t *) view_port->parent_struct;
    int show_items = list->show_items > 0 ? list->show_items : 16;
    int top = selected - (show_items / 2);
    int max_top = n - show_items;

    if (max_top < 0)
      max_top = 0;
    if (top < 0)
      top = 0;
    if (top > max_top)
      top = max_top;

    combobox_set_menu_size (widget, show_items);
    adj_set_value (view_port->adj, top);
    adj_set_state (list->slider->adj, adj_get_state (view_port->adj));
    expose_widget (view_port);
  } else {
    widget->label = label.c_str ();
  }
  updating_widget = false;

  expose_widget (widget);
}

void c_label::draw (void *w_, void *ptr) {
  (void) ptr;
  Widget_t *w = (Widget_t *) w_;
  if (!w)
    return;

  c_label *self = (c_label *) w->parent_struct;
  const float textsize = self ? self->textsize : 1.0f;
  const _textalign align = self ? self->align : TEXT_CENTER;
  const char *text = w->label ? w->label : "";

  Metrics_t metrics;
  os_get_window_metrics (w, &metrics);
  if (!metrics.visible)
    return;

  use_bg_color_scheme (w, NORMAL_);
  cairo_rectangle (w->crb, 0, 0, metrics.width, metrics.height);
  cairo_fill (w->crb);

  cairo_text_extents_t extents;
  use_text_color_scheme (w, NORMAL_);
  cairo_set_font_size (w->crb, (w->app->normal_font * textsize) / w->scale.ascale);
  cairo_text_extents (w->crb, text, &extents);
  
  const double padding = 2.0 * w->app->hdpi;
  double x = padding - extents.x_bearing;
  if (align == TEXT_CENTER)
    x = (metrics.width - extents.width) * 0.5 - extents.x_bearing;
  else if (align == TEXT_RIGHT)
    x = metrics.width - padding - extents.width - extents.x_bearing;

  const double y = (metrics.height - extents.height) * 0.5 - extents.y_bearing;

  cairo_move_to (w->crb, x, y);
  cairo_text_path (w->crb, text);
  cairo_fill (w->crb);
  cairo_new_path (w->crb);
}

static inline float db_to_bar_pos (float db, float min_db, float headroom_db) {
  return std::clamp ((db - min_db) / (headroom_db - min_db), 0.0f, 1.0f);
}

//extern "C" void configure_event (void *, void *);

static void main_notify_callback (void *w_, void *user_data) {
  //configure_event (w, user_data);
  
  Widget_t *w = (Widget_t *) w_;
  if (!w || !w->parent_struct)
    return;
    
  c_neuralblender_ui *ui = (c_neuralblender_ui *) w->parent_struct;

  Metrics_t metrics;
  os_get_window_metrics (w, &metrics);
  if (!metrics.visible)
    return;

  ui->on_window_resize (
      metrics.width / w->app->hdpi,
      metrics.height / w->app->hdpi);
}

static uint64_t get_unique_id () {
  static uint64_t current = 1;
  return current++;
}

static void draw_main_window (void *w_, void *user_data) {
  (void) user_data;
  Widget_t *w = (Widget_t *) w_;
  if (!w)
    return;

  Metrics_t metrics;
  os_get_window_metrics (w, &metrics);
  if (!metrics.visible)
    return;

  use_bg_color_scheme (w, get_color_state (w));
  //cairo_set_source_rgb (w->crb, NB_BG_R, NB_BG_G, NB_BG_B);
  cairo_rectangle (w->crb, 0, 0, metrics.width, metrics.height);
  cairo_fill (w->crb);
}

static void button_double_click (void *w_, void *event, void *user_data) {
  (void) event;
  (void) user_data;

  Widget_t *w = (Widget_t *) w_;
  if (!w)
    return;

  w->state = 0;
  if (w->adj_y)
    adj_set_value (w->adj_y, 0.0);
  expose_widget (w);
  auto *b = (c_button *) w->parent_struct;
  b->on_mouseup ();
}

static void knob_double_click (void *w_, void *event, void *user_data) {
  (void) event;
  (void) user_data;
  Widget_t *w = (Widget_t *) w_;
  if (!w)
    return;
  auto *k = (c_knob *) w->parent_struct;
  k->on_doubleclick ();
}

static void button_mouse_up (void *w_, void *event, void *user_data) {
  (void) event;
  (void) user_data;

  auto *w = (Widget_t *) w_;
  if (!w) return;
  w->state = 0;
  if (w->adj_y) adj_set_value (w->adj_y, 0.0);
  expose_widget (w);

  auto *b = (c_button *) w->parent_struct;
  b->on_mouseup ();
}

// here value_ points to a float
static void button_value_changed (void *w_, void *value_) {
  if (!value_)
    return;

  const float value = *(float *) value_;
  //debug ("value=%f", value);
  Widget_t *w = (Widget_t *) w_;
  if (!w || !w->parent_struct) {
    return;
  }

  c_button *b = (c_button *) w->parent_struct;
  b->value = value >= 0.5f;
  b->on_mouseup ();
}

static void knob_value_changed (void *w_, void *value_) {
  if (!value_)
    return;

  const float value = *(float *) value_;
  Widget_t *w = (Widget_t *) w_;
  if (!w || !w->parent_struct) {
    return;
  }

  c_knob *k = (c_knob *) w->parent_struct;
  if (k->value != value) {
    k->value = value;
    k->on_change ();
  }
}

static void filepicker_response(void *w_, void *user_data) { CP
  Widget_t *w = (Widget_t *) w_;
  if (!w || !w->parent_struct) {
    debug("!w || !w->parent_struct");
    return;
  }

  c_widget *cw = (c_widget *) w->parent_struct;
  c_filepicker *fp = cw ? cw->filepicker : NULL;
  if (!fp) {
    debug("!fp");
    return;
  }

  if (fp->dialog && fp->dialog->parent_struct) {
    FileDialog *fd = (FileDialog *) fp->dialog->parent_struct;
    if (fd->fp) {
      if (fd->fp->path)
        fp->current_dir = fd->fp->path;

      fp->filelist.clear();
      for (unsigned int i = 0; i < fd->fp->file_counter; i++) {
        if (fd->fp->file_names [i])
          fp->filelist.push_back (fd->fp->file_names[i]);
      }
    }
  }

  if (fp->ui && !fp->current_dir.empty ()) {
    fp->ui->configfile.set_item (CONFIG_KEY_NAME_CWD, fp->current_dir);
    fp->ui->configfile.write_file ();
  }

  fp->dialog = NULL;

  if (!user_data) {
    debug("!user_data");
    return;
  }

  const char *filename = *(const char **) user_data;
  if (!filename) {
    debug ("!filename");
    return;
  }

  c_neuralblender_ui *ui = fp->ui;
  size_t lane = fp->lane;

  //std::string selected (filename);
  //fp->on_file_select(cw, selected);
  debug ("lane %d", (int) lane);
  if (!ui) {
    debug ("!ui");
    return;
  }

  if (!cw) {
    debug ("!cw");
    return;
  }

  debug ("current_dir: '%s'", ui->state.current_dir.c_str ());

  //fp->selected_file = std::string (filename);
  ui->state.lanes [lane].filename = std::string (filename);
  ui->state.current_dir = path_dirname (ui->state.lanes [lane].filename);
  fp->scan_current_dir ();
  for (int i = 0; i < fp->filelist.size (); i++) {
    debug ("filelist [%d]: '%s'", i, fp->filelist [i].c_str ());
  }
  ui->load_model (cw->lane, filename);

  c_combobox *cb = &ui->lanes [lane].menu_list;
  cb->clear ();
  //cb->add (filename);
  fp->add_files_from_dir (cb);
  //ui->on_fileselected_pre (cw, filename);
  ui->on_fileselected (cw, filename);
}

static void combobox_selected_callback (void *w_, void *user_data) { CP
  Widget_t *w = (Widget_t *) w_;

  int index = (int) adj_get_value (w->adj);

  Widget_t *menu = w->childlist->childs[1];
  Widget_t *view_port = menu->childlist->childs[0];
  ComboBox_t *list = (ComboBox_t *) view_port->parent_struct;

  const char *label = NULL;
  if (index >= 0 && index < (int) list->list_size)
    label = list->list_names [index];

  // index + label are selected item
  c_combobox *cb = (c_combobox *) w->parent_struct;
  if (!cb) {
    debug ("!cb");
    return;
  }
  cb->selected = index;
  if (cb->updating_widget)
    return;

  cb->on_change (index);
}

static void combobox_changed (void *w_, void *user_data) {
  Widget_t *w = (Widget_t *) w_;
  if (!w)
    return;

  _set_entry(w, user_data);

  c_combobox *combo = (c_combobox *) w->parent_struct;
  if (!combo)
    return;

  int index = (int) adj_get_value(w->adj);
  combo->on_change (index);
}

void c_filepicker::create (
    c_neuralblender_ui *ui_,
    Widget_t *parent_,
    size_t lane_,
    const char *title_) {

  CP
  ui = ui_;
  parent = parent_;
  lane = lane_;
  c_widget::create (ui, parent, title_, 0, 0, 220, 220);
  //os_set_transient_for_hint (ui->main_widget, widget);
  title = std::string (title_);
}

void c_filepicker::show () { CP
  if (!parent) {
    debug ("!parent");
    return;
  }
  if (dialog) {
    destroy_widget (dialog, dialog->app);
    dialog = NULL;
  }
  parent->func.dialog_callback = filepicker_response;
  if (ui) {
    ui->configfile.read_file ();
    current_dir = ui->configfile.get_item (CONFIG_KEY_NAME_CWD);
  }
  debug ("current_dir='%s'", current_dir.c_str ());
  const char *path = current_dir.empty () ? CONFIG_DEFAULT_DIR : current_dir.c_str ();
  dialog = open_file_dialog (parent, path, ".nam|.json|.aidax");
}

void c_filepicker::hide () { CP
}

void c_filepicker::on_file_select (c_widget *cw, const std::string &filename) { CP
  debug ("current_dir='%s'", current_dir.c_str ());
  if (!ui)
    return;
}

std::string c_filepicker::get_current_dir () {
  return current_dir;
}

void c_filepicker::set_current_dir (std::string str) {
  current_dir = str;
  scan_current_dir ();
}

static bool is_supported_model_filename (const std::string &path) {
  std::string lower = path;
  std::transform (lower.begin (), lower.end (), lower.begin (),
                  [] (unsigned char c) { return (char) std::tolower (c); });

  return (lower.size () >= 4 && lower.rfind (".nam") == lower.size () - 4) ||
         (lower.size () >= 5 && lower.rfind (".json") == lower.size () - 5) ||
         (lower.size () >= 6 && lower.rfind (".aidax") == lower.size () - 6);
}

void c_filepicker::scan_current_dir () {
  filelist.clear ();

  if (current_dir.empty ())
    return;

  DIR *dir = opendir (current_dir.c_str ());
  if (!dir) {
    debug ("failed to scan '%s'", current_dir.c_str ());
    return;
  }

  struct dirent *entry = NULL;
  while ((entry = readdir (dir))) {
    const char *name = entry->d_name;
    if (!name || !strcmp (name, ".") || !strcmp (name, ".."))
      continue;

    if (!is_supported_model_filename (name))
      continue;

    std::string full = current_dir;
    if (!full.empty () && full.back () != '/')
      full += '/';
    full += name;

    struct stat st;
    if (stat (full.c_str (), &st) || !S_ISREG (st.st_mode))
      continue;

    filelist.push_back (name);
  }
  closedir (dir);

  std::sort (filelist.begin (), filelist.end ());
  debug ("scan '%s': %zu model files", current_dir.c_str (), filelist.size ());
}

void c_filepicker::add_files_from_dir (c_combobox *cb) {
  if (!cb)
    return;

  cb->items.clear();

  int sel = -1;
  std::string selected_file = ui->state.lanes [lane].filename;

  for (size_t i = 0; i < filelist.size (); i++) {
    cb->items.push_back (filelist [i]);

    std::string full = current_dir;
    if (!full.empty () && full.back () != '/')
      full += '/';
    full += filelist [i];


    if (full == selected_file || filelist [i] == selected_file) {
      sel = (int) i;
      debug ("found selected: %d", sel);
    }
  }

  cb->selected = sel;
  debug ("add_files_from_dir: dir='%s' selected='%s' files=%zu sel=%d",
         current_dir.c_str (), selected_file.c_str (), filelist.size (), sel);
  cb->update_widget();
}

void c_aboutwindow::create (c_neuralblender_ui *ui_) { CP
  ui = ui_;
  if (!ui || !ui->ui_ready || w)
    return;

  w = create_window (&ui->app, os_get_root_window (&ui->app, IS_WINDOW), 0, 0, 400, 450);
  if (!w)
    return;

  w->flags |= HIDE_ON_DELETE;
  os_set_transient_for_hint (ui->main_widget, w);

  w->func.expose_callback = draw_main_window;
  widget_set_title (w, "About NeuralBlender");
  btn_ok.create (ui, w, "OK", 160, 400, 80, 40);
  btn_ok.role = ROLE_ABOUTOK;

  const char *text [] = {
    "NeuralBlender",
    "",
    "An amp modeling plugin based on",
    "RTNeural and NeuralAmpModeler",
    "",
    "by Deimos Laboratories",
    "github.com/DeimosLabs/NeuralBlender",
    NULL
  };

  int i;
  for (i = 0; text [i]; i++) {
    int h = (i == 0 ? 20 : (180 + i * 24));
    labels [i].create (ui, w, text [i], 0, h, 400, 24);
    labels [i].align = TEXT_CENTER;
  }

  char buf [64];
  snprintf (buf, 63, "Build timestamp: %s", g_build_timestamp);

  labels [0].textsize = 1.5;
  labels [i].create (ui, w, buf, 0, 360, 400, 20);
  labels [i].textsize = 0.75;
  labels [i].align = TEXT_CENTER;

  img_logo.create (ui, w, "", (400-160)/2, 64, 160, 160);
  img_logo.set_png (data_neuralblender_logo_160_png);
}

void c_aboutwindow::show () { CP
  if (!w)
    create (ui);
  if (!w)
    return;

  widget_show_all (w);
  expose_widget (w);
}

void c_aboutwindow::hide () { CP
  if (!w)
    return;

  widget_hide (w);
}

void c_lane_widgets::create (
    c_neuralblender_ui *ui_,
    Widget_t *parent,
    size_t which,
    int x, int y, int w, int h) { CP
  
  knob_top = (h - knob_size) / 2;
  
  char label [64];
  ui = ui_;
  snprintf (label, 31, "Amp %c", (char) ('A' + which));
  lane_id = which;
  lane_widget.create (ui, parent, label, x, y, w, h);
  lane_widget.widget->scale.gravity = NONE;
  
  Widget_t *wp = lane_widget.widget;
  main_widget = wp;
  cont_regcontrols.create (ui, wp, "", 0, 0, 600, 64);
  cont_advcontrols.create (ui, wp, "", 0, 0, 300, 64);
  wreg = cont_regcontrols.widget;
  wadv = cont_advcontrols.widget;
  wreg->scale.gravity = NONE;
  wadv->scale.gravity = NONE;
  
  // regular controls
  menu_list.create (ui, wreg, label, 0, 0, 320, 32);
  menu_list.widget->func.value_changed_callback = combobox_selected_callback;
  menu_list.lane = which;

  int knobs_right = w - 180;
  gain_in.create (ui, wreg, "Input", knobs_right + 6, knob_top, knob_size, knob_size);
  gain_in.lane = gain_out.lane = delay.lane = which;
  gain_in.set_min (-40);
  gain_in.set_max (40);
  gain_in.set_defaultvalue (0);
  gain_in.set_value (0);
  gain_in.set_step (1);
  gain_in.role = ROLE_GAIN_IN;
  
  gain_out.create (ui, wreg, "Output", knobs_right + 75, knob_top, knob_size, knob_size);
  gain_out.role = ROLE_GAIN_OUT;
  gain_out.set_min (-40);
  gain_out.set_max (40);
  gain_out.set_defaultvalue (0);
  gain_out.set_value (0);
  gain_out.set_step (1);
  gain_out.role = ROLE_GAIN_OUT;
  
  meter_out.create (ui, wreg, "", 0, 0, 5, 120);
  meter_out.set_vudata (&vudata_out);
  meter_out.set_stereo (false);
  vudata_out.set_l (0.0, 0.0);
  
  btn_browse.create (ui, wreg, "Browse...", 0, 0, 100, 40);
  btn_clear.create  (ui, wreg, "Clear",     0, 0, 100, 40);
  btn_excl.create   (ui, wreg, "Use",       0, 0, 100, 40, BTN_TOGGLE);
  btn_mute.create   (ui, wreg, "Mute",      0, 0, 100, 40, BTN_TOGGLE);
  btn_mute.set_value (false);
  btn_browse.lane = which;
  btn_clear.lane = which;
  btn_mute.lane = which;
  btn_browse.role = ROLE_BROWSE;
  btn_clear.role = ROLE_CLEAR;
  btn_mute.role = ROLE_MUTE;
  btn_excl.role = ROLE_EXCL_USE;
  btn_excl.lane = which;
  
  // advanced controls
  delay.role = ROLE_DELAY;
  delay.create (ui, wadv, "Delay", 0, 0, knob_size, knob_size);
  delay.set_min (0);
  delay.set_max (30);
  delay.set_defaultvalue (0);
  delay.set_value (0);
  delay.set_step (0.01);
  delay.role = ROLE_DELAY;

  btn_flip.create   (ui, wadv, "DC flip", 0, 0, 32, 32, BTN_CHECKBOX);
  btn_calib.create   (ui, wadv, "Calib", 0, 0, 32, 32, BTN_CHECKBOX);
  label_flip.create (ui, wadv, "DC flip", 0, 0, 75, 32);
  label_calib.create (ui, wadv, "Calib.", 0, 0, 75, 32);
  
  if (ui && which < NB_UI_MAX_LANES) {
    ui->filepickers [which].create (ui, btn_browse.widget, which, "Select file");
    btn_browse.filepicker = &ui->filepickers [which];
    btn_browse.lane = which;
    ui->filepickers [which].lane = which;
  }
  
  move_resize (x, y, w, h);
}

void c_lane_widgets::move_resize (
    int x, int y, int w, int h) {
  
  if (!main_widget)
    return;

  lane_widget.move_resize (x, y, w, h);
  knob_top = (h - knob_size) / 2;

  int split = 0;
  
  if (ui && ui->state.showadvanced)
    split = w * 15 / 64;
  
  cont_advcontrols.move_resize (0, 0, split, h);
  cont_regcontrols.move_resize (split, 0, w - split, h);
  
  // regular controls
  const int reg_w = cont_regcontrols.w ();
  knob_right = std::max (16, reg_w - 150);
  const int menu_width = std::max (64, knob_right - 32);
  menu_list.move_resize (16, 24, menu_width, 32);
  int button_padding = 4;
  int button_width = std::max (24, (menu_list.w () + button_padding) / 3 - button_padding);
  int button_left = menu_list.x ();
  int button_top = menu_list.y () + menu_list.h () + 8;
  int button_height = h - 76;
  btn_browse.move_resize (button_left, button_top, button_width, button_height);
  btn_clear.move_resize (button_left + button_width + button_padding,
                         button_top, button_width, button_height);
  btn_mute.move_resize (button_left + (button_width + button_padding) * 2,
                         button_top, button_width, button_height);
  btn_excl.move_resize (btn_mute.x (), btn_mute.y (), btn_mute.w (), btn_mute.h ());
  
  gain_in.move_resize (knob_right, knob_top, knob_size, knob_size);
  gain_out.move_resize (knob_right + knob_size + 1, knob_top, knob_size, knob_size);
  meter_out.move_resize (knob_right + 130, 16, 5, h - 32);
  
  // advanced controls
  delay.move_resize (22, knob_top, knob_size, knob_size);
  if (ui && ui->state.showadvanced) {
    widget_show_all (cont_advcontrols.widget);
    expose_widget (cont_advcontrols.widget);
  } else {
    widget_hide (cont_advcontrols.widget);
  }

  int adv_btn_x = 84;
  int adv_btn_y = h / 5;
  btn_flip.move_resize (adv_btn_x, adv_btn_y, 32, 32);
  btn_calib.move_resize (adv_btn_x, h - adv_btn_y - 32, 32, 32);
  label_flip.move_resize (adv_btn_x + 32, adv_btn_y, 80, 32);
  label_calib.move_resize (adv_btn_x + 32, h - adv_btn_y - 32, 80, 32);
  
}

c_neuralblender_ui::c_neuralblender_ui () { CP
  memset (&app, 0, sizeof (app));
  display = NULL;
  window = 0;
  main_widget = NULL;
  ui_ready = false;
}

c_neuralblender_ui::~c_neuralblender_ui () { CP
  destroy ();
}

void c_neuralblender_ui::update_cwd (std::string path) {
  CP
  debug ("path='%s'", path.c_str ());
  configfile.set_item (CONFIG_KEY_NAME_CWD, path_dirname (path));
}

bool c_neuralblender_ui::create (Window parent_) { CP
  size_t i;
  destroy ();
  
  main_init (&app);
  app.small_font = 12 * app.hdpi;
  app.normal_font = 14 * app.hdpi;
  app.big_font = 20 * app.hdpi;
  display = app.dpy;

  parent = parent_;
  if (!parent)
    parent = DefaultRootWindow (display);

  main_widget = create_window (&app, parent, 0, 0, 640, 640);
  if (!main_widget)
    return false;
  
  main_widget->parent_struct = this;
  main_widget->func.resize_notify_callback = main_notify_callback;
  main_widget->scale.gravity = NONE;
  set_widget_color_all_states (main_widget, BACKGROUND_, 0.125, 0.125, 0.125);

  widget_set_icon_from_png (main_widget, data_neuralblender_logo_512_png);

  os_register_wm_delete_window (main_widget);
  widget_set_title (main_widget, "NeuralBlender");
  main_widget->func.expose_callback = draw_main_window;
  label_big.create (this, main_widget, "NeuralBlender", 120, 24, 400, 40);
  label_big.align = TEXT_CENTER;
  label_big.textsize = 1.5;
  
  btn_enable.create (this, main_widget, "Enabled",  20, 12, 120, 40, BTN_TOGGLE);
  btn_enable.set_value (true);
  btn_enable.role = ROLE_BYPASS;
  btn_about.create (this, main_widget, "About...", 520, 600, 100, 40);
  btn_about.role = ROLE_ABOUT;
  btn_muteall.create (this, main_widget, "Mute all", 500, 12, 120, 40, BTN_TOGGLE);
  btn_muteall.role = ROLE_MUTEALL;
  
  cont_checkboxes.create (this, main_widget, "", 8, 604, 500, 40);
  cont_checkboxes.widget->scale.gravity = NONE;
  
  btn_vu.create (this, cont_checkboxes.widget, "VU", 0, 0, 32, 32, BTN_CHECKBOX);
  label_vu.create (this, cont_checkboxes.widget, "VU meters", 32, 0, 80, 32);
  btn_vu.role = ROLE_VUTOGGLE;
  btn_vu.set_value (state.do_vu);
  label_vu.widget->scale.gravity = WESTCENTER;
  
  btn_advanced.create (this, cont_checkboxes.widget, "Adv.", 140, 0, 32, 32, BTN_CHECKBOX);
  label_advanced.create (this, cont_checkboxes.widget, "Show advanced", 172, 0, 150, 32);
  btn_advanced.set_value (state.showadvanced);
  btn_advanced.role = ROLE_ADV_TOGGLE;
  label_advanced.widget->scale.gravity = WESTCENTER;
  
  btn_exclmode.create (this, cont_checkboxes.widget, "Excl. mode", 320, 0, 32, 32, BTN_CHECKBOX);
  label_exclmode.create (this, cont_checkboxes.widget, "Excl. mode", 352, 0, 150, 32);
  btn_exclmode.set_value (state.do_excl);
  btn_exclmode.role = ROLE_EXCL_TOGGLE;
  label_exclmode.widget->scale.gravity = WESTCENTER;
  
  aboutwindow.create (this);
  for (i = 0; i < NB_UI_MAX_LANES; i++) {
    lanes [i].create (this, main_widget, i, 0, 0, 640, 130);
  }
  meter_in.create (this, main_widget, "", 6, 70, 5, 520);
  meter_in.set_vudata (&vudata_in);
  meter_in.set_stereo (false);
  vudata_in.set_l (0.0, 0.0);

  if (blender) {
    for (i = 0; i < NB_UI_MAX_LANES; i++) {
      blender->meters_out [i] = &lanes [i].vudata_out;
    }
    blender->meter_in = &vudata_in;
  }
  
  if (state.showadvanced) {
    show_advanced_settings ();
  } else {
    hide_advanced_settings ();
  }

  widget_show_all (main_widget);
  expose_widget (main_widget);

  window = main_widget->widget;
  ui_ready = true;
  CP
  XFlush (display);
  CP
  return true;
}

void c_neuralblender_ui::destroy () { CP
  if (ui_ready)
    main_quit (&app);

  memset (&app, 0, sizeof (app));
  display = NULL;
  window = 0;
  main_widget = NULL;
  ui_ready = false;
}

void c_neuralblender_ui::reposition_widgets (bool snap_to_default) {
  CP
  bool b = state.showadvanced;
  state.showadvanced = b;
  
  if (!ui_resize_lock) {
    Metrics_t metrics;
    ui_resize_lock = true;
    
    int min_window_width = b ? 720 : 640;
    int min_window_height = 640;
    int window_width = min_window_width;
    int window_height = min_window_height;
    
    os_set_window_min_size (main_widget, min_window_width, min_window_height,
                            min_window_width, min_window_height);
    if (snap_to_default) {
      window_width = min_window_width;
      window_height = min_window_height;
    } else {
    os_get_window_metrics (main_widget, &metrics);
      window_width = std::max (metrics.width, min_window_width);
      window_height = std::max (metrics.height, min_window_height);
    }
    if (metrics.width < min_window_width ||
        metrics.height < min_window_height || snap_to_default) {
      debug ("requesting window size:");
      request_window_size (window_width, window_height);
    }
    
    int lane_width = window_width - 24;
    //int lane_height = 130;
    int lane_height = window_height / 5;
    
    debug ("window w/h %d,%d", window_width, window_height);
    //main_widget->func.configure_callback (main_widget, NULL);
    
    
    cont_checkboxes.move_resize (16, window_height - 44, b ? 500 : 450, 40);
    
    btn_enable.move_resize (16, 12, 120, 40);
    btn_muteall.move_resize (window_width - 136, 12, 120, 40);
    btn_about.move_resize (btn_muteall.x () + 20, window_height - 50, 100, 40);
    label_exclmode.set_label (b ? "Exclusive mode" : "Excl. mode");
    label_big.move_resize (150, 0, window_width - 300, 48);
    
    size_t i;
    for (i = 0; i < NB_UI_MAX_LANES; i++) {
      lanes [i].move_resize (12, 60 + i * (lane_height + 5), lane_width, lane_height);
    }
    meter_in.move_resize (6, 64, 5, (lane_height + 5) * i - 12);
    
    ui_resize_lock = false;
  }
}

void c_neuralblender_ui::show_advanced_settings (bool b) {
  //show_advanced = b;
  state.showadvanced = b;
  reposition_widgets (true);
}

void c_neuralblender_ui::hide_advanced_settings () {
  show_advanced_settings (false);
}

void c_neuralblender_ui::vu_on (bool b) { CP
  if (!b) {
    vu_off ();
    return;
  }

  state.do_vu = true;

  meter_in.show ();
  for (size_t i = 0; i < NB_UI_MAX_LANES; i++) {
    lanes [i].meter_out.show ();
  }
  on_vu (&btn_vu, b);
}

void c_neuralblender_ui::vu_off () { CP
  state.do_vu = false;

  meter_in.hide ();
  for (size_t i = 0; i < NB_UI_MAX_LANES; i++) {
    lanes [i].meter_out.hide ();
  }
  on_vu (&btn_vu, false);
}

size_t c_neuralblender_ui::choose_exclusive_lane () const {
  if (state.exclusive_lane > 0 &&
      state.exclusive_lane <= (int) NB_UI_MAX_LANES)
    return (size_t) state.exclusive_lane;

  if (last_exclusive_lane > 0 && last_exclusive_lane <= NB_UI_MAX_LANES)
    return last_exclusive_lane;

  for (size_t i = 0; i < NB_UI_MAX_LANES; ++i) {
    if (!state.lanes [i].filename.empty () &&
        !state.lanes [i].lane_mute)
      return i + 1;
  }

  for (size_t i = 0; i < NB_UI_MAX_LANES; ++i) {
    //if (!filepickers [i].selected_file.empty ())
    if (!state.lanes [i].filename.empty ())
      return i + 1;
  }

  for (size_t i = 0; i < NB_UI_MAX_LANES; ++i) {
    if (!state.lanes [i].lane_mute)
      return i + 1;
  }

  return 1;
}

void c_neuralblender_ui::on_window_resize (int w, int h) {
  if (ui_ready && !ui_resize_lock)
    reposition_widgets ();
}

bool c_neuralblender_ui::request_window_size (int w, int h) {
  if (!main_widget || !display)
    return false;

  os_resize_window (display, main_widget, w, h);
  return true;
}

void c_neuralblender_ui::on_excl (c_widget *w, int n) {
  debug ("n=%d", n);
  state.exclusive_lane = n;
  if (n > 0 && n <= (int) NB_UI_MAX_LANES)
    last_exclusive_lane = (size_t) n;
  if (!w)
    return;

  btn_exclmode.set_value (state.exclusive_lane != 0);
  apply_effective_controls();
  sync_widgets_from_state (state);
}

void c_neuralblender_ui::on_excl_use (c_widget *w, bool b) {
  (void) b;
  if (!w)
    return;

  debug ("lane %d, value=%d", (int) w->lane + 1, (int) b);

  on_excl (w, (int) w->lane + 1);
}

void c_neuralblender_ui::on_advanced (c_widget *w, bool b) {
  (void) w;
  debug ("b=%d", (int) b);
  show_advanced_settings (b);
}

int c_neuralblender_ui::idle () {
  if (!ui_ready) {
    CP
    return 0;
  }

  if (state.do_vu) {
    meter_in.on_ui_timer ();
    for (int i = 0; i < NB_MAX_MODELS; i++) {
      lanes [i].meter_out.on_ui_timer ();
    }
  }
  run_embedded (&app);
  return 0;
}

void c_neuralblender_ui::draw () {
  if (!main_widget)
    return;

  widget_draw (main_widget, NULL);
}

void c_neuralblender_ui::clear_lane_model_ui (size_t which) {
  if (which >= NB_UI_MAX_LANES)
    return;

  //filepickers [which].selected_file.clear ();
  state.lanes [which].filename.clear ();
  lanes [which].menu_list.clear ();
}

static std::string path_dirname (const std::string &path) {
  const size_t pos = path.find_last_of ('/');
  if (pos == std::string::npos)
    return "";

  if (pos == 0)
    return "/";

  return path.substr (0, pos);
}

static std::string path_basename (const std::string &path) {
  const size_t pos = path.find_last_of ('/');
  if (pos == std::string::npos)
    return path;

  return path.substr (pos + 1);
}

void c_neuralblender_ui::set_lane_mute (size_t which, bool b) {
  debug ("which=%d, b=%d", (int) which, (int) b);
  if (which >= NB_UI_MAX_LANES)
    return;

  const bool old_updating = updating_from_state;
  updating_from_state = true;

  lanes [which].btn_mute.value = b;
  lanes [which].btn_mute.set_value (b);

  updating_from_state = old_updating;
}

void c_neuralblender_ui::apply_effective_controls () {
}

void c_neuralblender_ui::sync_widgets_from_state (const c_neuralblender_state &state_) {
  if (!ui_ready)
    return;
  this->state = state_;
  if (state.exclusive_lane > 0 &&
      state.exclusive_lane <= (int) NB_UI_MAX_LANES)
    last_exclusive_lane = (size_t) state.exclusive_lane;

  updating_from_state = true;

  const size_t nlanes = NB_UI_MAX_LANES < NB_MAX_MODELS ? NB_UI_MAX_LANES : NB_MAX_MODELS;
  for (size_t i = 0; i < nlanes; ++i) {
    const c_neuralblender_lane_state &lane = state.lanes [i];

    lanes [i].gain_in.set_value (gain_to_db (lane.gain_in));
    lanes [i].gain_out.set_value (gain_to_db (lane.gain_out));
    lanes [i].delay.set_value (lane.delay_ms);

    //filepickers [i].selected_file = lane.filename;
    if (state.lanes [i].filename.empty ()) {
      lanes [i].menu_list.clear ();
    } else {
      filepickers [i].current_dir = path_dirname (state.lanes [i].filename);
      filepickers [i].scan_current_dir ();
      filepickers [i].add_files_from_dir (&lanes [i].menu_list);
    }
  }

  const bool enabled = !state.bypass;
  btn_enable.set_value (enabled);
  btn_enable.set_label (enabled ? "Enabled" : "Bypass");

  btn_vu.set_value (state.do_vu);
  if (state.do_vu) {
    meter_in.show ();
    for (size_t i = 0; i < NB_UI_MAX_LANES; ++i)
      lanes [i].meter_out.show ();
  } else {
    meter_in.hide ();
    for (size_t i = 0; i < NB_UI_MAX_LANES; ++i)
      lanes [i].meter_out.hide ();
  }

  btn_exclmode.set_value (state.exclusive_lane > 0);

  const bool exclusive_on = state.exclusive_lane > 0;
  for (size_t i = 0; i < NB_UI_MAX_LANES; ++i) {
    const bool selected =
      exclusive_on && state.exclusive_lane == i + 1;

    lanes [i].btn_mute.set_value (state.lanes [i].lane_mute);
    lanes [i].btn_excl.set_value (selected);

    for (size_t i = 0; i < NB_UI_MAX_LANES; i++) {
      lanes [i].lane_widget.set_fg_color (0, 0, 0);
    }
    if (exclusive_on) { CP
      widget_hide (lanes [i].btn_mute.widget);
      widget_show (lanes [i].btn_excl.widget);
    } else { CP
      widget_show (lanes [i].btn_mute.widget);
      widget_hide (lanes [i].btn_excl.widget);
    }
  }
  if (exclusive_on)
    lanes [state.exclusive_lane - 1].lane_widget.set_fg_color (0.1, 0.4, 0.4);

  updating_from_state = false;
}
