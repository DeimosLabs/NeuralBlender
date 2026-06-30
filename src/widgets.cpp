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

#define CMDLINE_DEBUG_COLOR ANSI_DARK_CYAN
#include "cmdline/cmdline_debug.h"

static constexpr float DEFAULT_BG_R = 0.125f;
static constexpr float DEFAULT_BG_G = 0.125f;
static constexpr float DEFAULT_BG_B = 0.125f;

static constexpr t_gradientcolors grad (
    float r1, float g1, float b1, float a1,
    float r2, float g2, float b2, float a2) {
  return { r1, g1, b1, a1, r2, g2, b2, a2 };
}

static constexpr t_gradientcolors solid (
    float r, float g, float b, float a = 1.0f) {
  return grad (r, g, b, a, r, g, b, a);
}

static constexpr t_statecolors sc (
    t_gradientcolors bg,
    t_gradientcolors fg) {
  return { bg, fg };
}

// Floats galore. Bring a row boat.

static t_colortheme g_default_colors = {
  // window_bg
  grad  (0.090f, 0.090f, 0.095f, 1.0f, 0.130f, 0.130f, 0.140f, 1.0f),

  // button
  { // normal, hover, on, on_hover, down, disabled
    sc (solid (0.0f,  0.0f,  0.0f,  0.0f), solid (0.58f, 0.59f, 0.60f, 1.0f)),
    sc (grad  (0.16f, 0.16f, 0.17f, 0.45f, 0.23f, 0.23f, 0.24f, 0.45f), solid (0.76f, 0.78f, 0.80f, 1.0f)),
    sc (grad  (0.04f, 0.22f, 0.22f, 0.90f, 0.08f, 0.38f, 0.37f, 0.90f), solid (0.70f, 0.95f, 0.93f, 1.0f)),
    sc (grad  (0.06f, 0.29f, 0.29f, 0.95f, 0.10f, 0.48f, 0.46f, 0.95f), solid (0.78f, 1.00f, 0.98f, 1.0f)),
    sc (grad  (0.07f, 0.07f, 0.08f, 0.85f, 0.12f, 0.12f, 0.13f, 0.85f), solid (0.42f, 0.43f, 0.44f, 1.0f)),
    sc (solid (0.10f, 0.10f, 0.10f, 0.45f), solid (0.30f, 0.30f, 0.31f, 0.75f))
  },

  // checkbox
  { // normal, hover, on, on_hover, down, disabled
    sc (grad  (0.11f, 0.11f, 0.12f, 0.85f, 0.16f, 0.16f, 0.17f, 0.85f), solid (0.54f, 0.55f, 0.56f, 1.0f)),
    sc (grad  (0.16f, 0.16f, 0.17f, 0.90f, 0.22f, 0.22f, 0.23f, 0.90f), solid (0.76f, 0.78f, 0.80f, 1.0f)),
    sc (grad  (0.04f, 0.24f, 0.24f, 0.95f, 0.08f, 0.42f, 0.40f, 0.95f), solid (0.72f, 0.96f, 0.94f, 1.0f)),
    sc (grad  (0.05f, 0.30f, 0.30f, 1.00f, 0.10f, 0.50f, 0.48f, 1.00f), solid (0.80f, 1.00f, 0.98f, 1.0f)),
    sc (grad  (0.07f, 0.07f, 0.08f, 0.95f, 0.11f, 0.11f, 0.12f, 0.95f), solid (0.38f, 0.39f, 0.40f, 1.0f)),
    sc (solid (0.10f, 0.10f, 0.10f, 0.45f), solid (0.30f, 0.30f, 0.31f, 0.75f))
  },

  // radio
  { // normal, hover, on, on_hover, down, disabled
    sc (solid (0.0f,  0.0f,  0.0f,  0.0f), solid (0.54f, 0.55f, 0.56f, 1.0f)),
    sc (grad  (0.16f, 0.16f, 0.17f, 0.50f, 0.22f, 0.22f, 0.23f, 0.50f), solid (0.76f, 0.78f, 0.80f, 1.0f)),
    sc (grad  (0.04f, 0.22f, 0.22f, 0.90f, 0.08f, 0.38f, 0.37f, 0.90f), solid (0.72f, 0.96f, 0.94f, 1.0f)),
    sc (grad  (0.06f, 0.29f, 0.29f, 0.95f, 0.10f, 0.48f, 0.46f, 0.95f), solid (0.80f, 1.00f, 0.98f, 1.0f)),
    sc (grad  (0.07f, 0.07f, 0.08f, 0.85f, 0.11f, 0.11f, 0.12f, 0.85f), solid (0.38f, 0.39f, 0.40f, 1.0f)),
    sc (solid (0.10f, 0.10f, 0.10f, 0.45f), solid (0.30f, 0.30f, 0.31f, 0.75f))
  },

  // frame_normal, frame_selected, frame_disabled
  sc (grad  (0.115f, 0.118f, 0.122f, 1.0f, 0.145f, 0.150f, 0.155f, 1.0f), grad  (0.42f, 0.43f, 0.44f, 1.0f, 0.00f, 0.00f, 0.00f, 1.0f)),
  sc (grad  (0.125f, 0.128f, 0.132f, 1.0f, 0.165f, 0.170f, 0.175f, 1.0f), solid (0.28f, 0.65f, 0.64f, 1.0f)),
  sc (solid (0.10f,  0.10f,  0.10f,  0.55f), solid (0.20f, 0.20f, 0.20f, 0.75f))
};

static t_colortheme *g_colors = &g_default_colors;

const t_colortheme *get_colortheme () {
  return g_colors;
}

static const t_statecolors &control_colors_for_state (
    const t_controlcolors &colors,
    _widget_state state) {

  switch (state) {
    case WSTATE_HOVER:
    case WSTATE_OFF_HOVER:
      return colors.hover;

    case WSTATE_ON:
    case WSTATE_SELECTED:
      return colors.on;

    case WSTATE_ON_HOVER:
      return colors.on_hover;

    case WSTATE_DOWN:
    case WSTATE_DOWN_HOVER:
      return colors.down;

    case WSTATE_DISABLED:
      return colors.disabled;

    case WSTATE_OFF:
    case WSTATE_NORMAL:
    default:
      return colors.normal;
  }
}

static const t_statecolors &frame_colors_for_state (_widget_state state) {
  switch (state) {
    case WSTATE_SELECTED:
    case WSTATE_HOVER:
      return g_colors->frame_selected;

    case WSTATE_DISABLED:
    case WSTATE_OFF:
      return g_colors->frame_disabled;

    case WSTATE_NORMAL:
    case WSTATE_ON:
    default:
      return g_colors->frame_normal;
  }
}

static const t_statecolors &colors_for (
    _widget_style style,
    _widget_state state) {

  switch (style) {
    case WSTYLE_FRAME:
    case WSTYLE_FRAME_HIGHLIGHT:
    case WSTYLE_FRAME_DISABLED:
      return frame_colors_for_state (state);

    case WSTYLE_CHECKBOX:
      return control_colors_for_state (g_colors->checkbox, state);

    case WSTYLE_RADIO:
      return control_colors_for_state (g_colors->radio, state);

    case WSTYLE_BUTTON:
    case WSTYLE_TOGGLE:
    case WSTYLE_IMAGE:
    case WSTYLE_IMAGE_BUTTON:
    case WSTYLE_IMAGE_TOGGLE:
    case WSTYLE_DISABLED:
    default:
      return control_colors_for_state (g_colors->button, state);
  }
}


// we need to keep this for now
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

static bool get_widget_size  (Widget_t *w, int *rx, int *ry, int *rw, int *rh) {
  if (!w)
    return false;

  Metrics_t m;
  os_get_window_metrics (w, &m);
  if (!m.visible || m.width <= 0 || m.height <= 0)
    return false;

  if (rx) *rx = 0;
  if (ry) *ry = 0;
  if (rw) *rw = m.width;
  if (rh) *rh = m.height;
  return true;
}

static cairo_pattern_t *create_vertical_gradient (
    int x,
    int y,
    int h,
    const t_gradientcolors &colors) {

  cairo_pattern_t *pat = cairo_pattern_create_linear (x, y, x, y + h);
  cairo_pattern_add_color_stop_rgba (
      pat, 0.0, colors.r1, colors.g1, colors.b1, colors.a1);
  cairo_pattern_add_color_stop_rgba (
      pat, 1.0, colors.r2, colors.g2, colors.b2, colors.a2);
  return pat;
}

static void path_rounded_rect (
    cairo_t *cr,
    double x,
    double y,
    double w,
    double h,
    double r) {

  if (!cr || w <= 0.0 || h <= 0.0)
    return;

  r = std::clamp (r, 0.0, std::min (w, h) * 0.5);
  if (r <= 0.0) {
    cairo_rectangle (cr, x, y, w, h);
    return;
  }

  cairo_new_path (cr);
  cairo_arc (cr, x + r,     y + r,     r, M_PI, 1.5 * M_PI);
  cairo_arc (cr, x + w - r, y + r,     r, 1.5 * M_PI, 0.0);
  cairo_arc (cr, x + w - r, y + h - r, r, 0.0, 0.5 * M_PI);
  cairo_arc (cr, x + r,     y + h - r, r, 0.5 * M_PI, M_PI);
  cairo_close_path (cr);
}

static void fill_rounded_rect (
    Widget_t *w,
    int x,
    int y,
    int width,
    int height,
    float radius,
    const t_gradientcolors &colors) {

  if (!w || !w->crb || width <= 0 || height <= 0)
    return;

  cairo_pattern_t *pat = create_vertical_gradient (x, y, height, colors);
  path_rounded_rect (w->crb, x, y, width, height, radius);
  cairo_set_source (w->crb, pat);
  cairo_fill (w->crb);
  cairo_pattern_destroy (pat);
}

static void fill_rounded_rect (
    Widget_t *w,
    float radius,
    const t_gradientcolors &colors) {
  int x = 0, y = 0, width = 0, height = 0;
  if (!get_widget_size (w, &x, &y, &width, &height))
    return;

  fill_rounded_rect (w, x, y, width, height, radius, colors);
}

static void draw_rounded_rect (
    Widget_t *w,
    int x,
    int y,
    int width,
    int height,
    float radius,
    const t_gradientcolors &colors,
    float line_width) {

  if (!w || !w->crb || width <= 0 || height <= 0)
    return;

  const double half = line_width * 0.5;

  cairo_pattern_t *pat = create_vertical_gradient (x, y, height, colors);
  path_rounded_rect (
      w->crb,
      x + half,
      y + half,
      width - line_width,
      height - line_width,
      radius);

  cairo_set_source (w->crb, pat);
  cairo_set_line_width (w->crb, line_width);
  cairo_stroke (w->crb);
  cairo_pattern_destroy (pat);
}

static void draw_rounded_rect (
    Widget_t *w,
    float radius,
    const t_gradientcolors &colors,
    float line_width) {
  int x = 0, y = 0, width = 0, height = 0;
  if (!get_widget_size (w, &x, &y, &width, &height))
    return;

  draw_rounded_rect (w, x, y, width, height, radius, colors, line_width);
}

void inherit_parent_bg_color (Widget_t *w, Widget_t *parent) {
  return;
  if (!w || !parent || !parent->color_scheme)
    return;

  Colors *c = get_color_scheme (parent, NORMAL_);
  if (!c)
    return;

  set_widget_color_all_states (
      w, BACKGROUND_, c->bg [0], c->bg [1], c->bg [2], c->bg [3]);
}

unsigned long x11_color_pixel (
    Display *display,
    const t_gradientcolors &colors) {

  if (!display)
    return 0;

  const int screen = DefaultScreen (display);
  XColor color;
  color.red   = (unsigned short) (std::clamp (colors.r1, 0.0f, 1.0f) * 65535.0f);
  color.green = (unsigned short) (std::clamp (colors.g1, 0.0f, 1.0f) * 65535.0f);
  color.blue  = (unsigned short) (std::clamp (colors.b1, 0.0f, 1.0f) * 65535.0f);
  color.flags = DoRed | DoGreen | DoBlue;

  if (XAllocColor (
          display,
          DefaultColormap (display, screen),
          &color))
    return color.pixel;

  return BlackPixel (display, screen);
}

void set_x11_window_background (
    Widget_t *w,
    const t_gradientcolors &colors) {

  if (!w || !w->app || !w->app->dpy || !w->widget)
    return;

  Display *display = w->app->dpy;
  XSetWindowBackground (display, w->widget, x11_color_pixel (display, colors));
  XClearWindow (display, w->widget);
}

void cb_dummy (void *w_, void* user_data) {}

cairo_surface_t *button_image_for_state (c_button *b, Widget_t *w) {
  const bool on = w->adj && adj_get_value (w->adj) >= 0.5f;
  const bool hover = (w->flags & HAS_FOCUS) || w->state == 1;
  const bool down = w->state == 2;

  if (down && hover && b->img_down_hover) return b->img_down_hover;
  if (down && b->img_down) return b->img_down;
  if (!on && hover && b->img_off_hover) return b->img_off_hover;
  if (hover && b->img_hover) return b->img_hover;
  if (on && b->img_on) return b->img_on;
  return b->img_off;
}

void main_notify_callback (void *w_, void *user_data) {
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

uint64_t get_unique_id () {
  static uint64_t current = 1;
  return current++;
}

void cb_draw_main_window (void *w_, void *user_data) { CP
  (void) user_data;
  Widget_t *w = (Widget_t *) w_;
  if (!w)
    return;

  Metrics_t metrics;
  os_get_window_metrics (w, &metrics);
  if (!metrics.visible)
    return;

  fill_rounded_rect (w, 0, 0, metrics.width, metrics.height,
                     0.0f, g_colors->window_bg);
}

void button_double_click (void *w_, void *event, void *user_data) {
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

void knob_double_click (void *w_, void *event, void *user_data) {
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

static _widget_state button_state_from_xputty (Widget_t *w) {
  if (!w)
    return WSTATE_UNKNOWN;

  const bool on = w->adj && adj_get_value (w->adj) >= 0.5f;
  const bool hover = (w->flags & HAS_FOCUS) || w->state == 1;
  const bool down = w->state == 2;

  if (down && hover) return WSTATE_DOWN_HOVER;
  if (down)          return WSTATE_DOWN;
  if (!on && hover)  return WSTATE_OFF_HOVER;
  if (on && hover)   return WSTATE_ON_HOVER;
  if (hover)         return WSTATE_HOVER;
  if (on)            return WSTATE_ON;
  return WSTATE_OFF;
}

void knob_value_changed (void *w_, void *value_) {
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
  set_x11_window_background (widget, g_colors->window_bg);

  // no callback func because it would overwrite the ones from
  // child widget classes if they call this func. after their own

  widget->scale.gravity = NONE;
  widget->parent_struct = this;
  widget->label = label.c_str ();
}

bool c_widget::set_label (const char *label_) {
  label = label_ ? label_ : "";
  if (!widget)
    return false;

  widget->label = label.c_str ();
  expose ();
  return true;
}

void c_widget::set_state (_widget_state state_) {
  wstate = state_;
  expose ();
}

void c_widget::draw_text_centered (Widget_t *w,
    const char *text,
    float r, float g, float b) {

  if (!w || !w->crb || !text)
    return;

  Metrics_t m;
  os_get_window_metrics (w, &m);
  if (!m.visible)
    return;

  cairo_set_source_rgba (w->crb, r, g, b, 1.0);
  
  float fontsize = w->app->normal_font / w->scale.ascale;
  c_widget *cw = (c_widget *) w->parent_struct;
  if (cw)
    fontsize *= cw->text_size;
  cairo_set_font_size (w->crb, fontsize);

  cairo_text_extents_t ext;
  cairo_text_extents (w->crb, text, &ext);

  cairo_move_to (
      w->crb,
      (m.width - ext.width) * 0.5 - ext.x_bearing,
      (m.height - ext.height) * 0.5 - ext.y_bearing);

  cairo_show_text (w->crb, text);
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
  expose ();
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
  expose ();
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
  expose ();
}

void c_frame::create (
    c_neuralblender_ui *ui_,
    Widget_t *parent,
    const char *label_,
    int x, int y, int w, int h) {
  
  wstyle = WSTYLE_FRAME;
  wstate = WSTATE_NORMAL;
  widget = add_frame (parent, label_ ? label_ : "", x, y, w, h);
  widget->func.expose_callback = c_frame::cb_draw;
  c_widget::create (ui_, parent, label_, x, y, w, h);
  inherit_parent_bg_color (widget, parent);
}

void c_frame::cb_draw (void *w_, void *user_data) {
  (void) user_data;

  Widget_t *w = (Widget_t *) w_;
  if (!w || !w->parent_struct)
    return;

  c_frame *self = (c_frame *) w->parent_struct;

  _widget_state state = self->wstate;
  if (state == WSTATE_UNKNOWN)
    state = WSTATE_NORMAL;

  const t_statecolors &c = colors_for (self->wstyle, state);
  
  //debug ("c.bg: %f,%f,%f,%f to %f,%f,%f,%f", c.bg.r1, c.bg.g1, c.bg.b1, c.bg.a1, c.bg.r2, c.bg.g2, c.bg.b2, c.bg.a2);
  //debug ("c.fg: %f,%f,%f,%f to %f,%f,%f,%f", c.fg.r1, c.fg.g1, c.fg.b1, c.fg.a1, c.fg.r2, c.fg.g2, c.fg.b2, c.fg.a2);

  fill_rounded_rect (w, UI_FRAME_RADIUS, c.bg);
  draw_rounded_rect (w, UI_FRAME_RADIUS, c.fg, 2.0f);
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
    expose ();
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

void c_label::create (
    c_neuralblender_ui *ui_,
    Widget_t *parent,
    const char *label_,
    int x, int y, int w, int h) {

  role = ROLE_UNKNOWN;
  widget = add_label (parent, label.c_str (), x, y, w, h);
  widget->func.expose_callback = c_label::cb_draw;
  c_widget::create (ui_, parent, label_, x, y, w, h);
  inherit_parent_bg_color (widget, parent);
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

  widget_get_png (widget, png);
  expose_widget (widget);
}

c_button::c_button () { CP }

c_button::~c_button () { CP
  if (img_off)        cairo_surface_destroy (img_off);
  if (img_on)         cairo_surface_destroy (img_on);
  if (img_hover)      cairo_surface_destroy (img_hover);
  if (img_down)       cairo_surface_destroy (img_down);
  if (img_down_hover) cairo_surface_destroy (img_down_hover);
  if (img_off_hover)  cairo_surface_destroy (img_off_hover);
}

void c_button::create (
    c_neuralblender_ui *ui_,
    Widget_t *parent,
    const char *label_,
    int x, int y, int w, int h,
    _widget_style style) {

  switch (style) {
    case WSTYLE_CHECKBOX:
      is_toggle = true;
      widget = add_check_button (parent, label_ ? label_ : "", x, y, w, h);
      widget->func.value_changed_callback = button_value_changed;
    break;

    case WSTYLE_TOGGLE:
      is_toggle = true;
      widget = add_toggle_button (parent, label_ ? label_ : "", x, y, w, h);
      widget->func.value_changed_callback = button_value_changed;
    break;

    case WSTYLE_IMAGE_TOGGLE:
      is_toggle = true;
      widget = add_image_toggle_button (parent, label_ ? label_ : "", x, y, w, h);
      widget->func.value_changed_callback = button_value_changed;
    break;

    case WSTYLE_IMAGE:
      is_toggle = false;
      widget = add_image_button (parent, label_ ? label_ : "", x, y, w, h);
      widget->func.double_click_callback = button_double_click;
      widget->func.button_release_callback = button_mouse_up;
    break;

    default:
      is_toggle = false;
      widget = add_button (parent, label_ ? label_ : "", x, y, w, h);
      widget->func.double_click_callback = button_double_click;
      widget->func.button_release_callback = button_mouse_up;
    break;
  }

  widget->func.expose_callback = c_button::cb_draw;
  wstyle = style;
  
  c_widget::create (ui_, parent, label_, x, y, w, h);
  /*if (style == WSTYLE_IMAGE || style == WSTYLE_IMAGE_TOGGLE)
    widget->func.expose_callback = cb_draw_button;*/
}

bool c_button::draw_button_image (Widget_t *w, c_button *b) {
  if (!w || !b || !w->crb)
    return false;

  cairo_surface_t *img = button_image_for_state (b, w);
  if (!img || cairo_surface_status (img) != CAIRO_STATUS_SUCCESS)
    return false;

  Metrics_t m;
  os_get_window_metrics (w, &m);
  if (!m.visible)
    return true; // no fallback needed, just invisible

  const double iw = cairo_image_surface_get_width (img);
  const double ih = cairo_image_surface_get_height (img);
  if (iw <= 0 || ih <= 0)
    return false;

  cairo_save (w->crb);
  cairo_scale (w->crb, (double) m.width / iw, (double) m.height / ih);
  cairo_set_source_surface (w->crb, img, 0, 0);
  cairo_rectangle (w->crb, 0, 0, iw, ih);
  cairo_fill (w->crb);
  cairo_restore (w->crb);

  return true;
}

void c_button::cb_draw (void *w, void *user_data) {
  (void) user_data;

  Widget_t *widget = (Widget_t *) w;
  if (!widget || !widget->parent_struct)
    return;

  c_button *self = (c_button *) widget->parent_struct;
  self->wstate = button_state_from_xputty (widget);
  const t_statecolors &colors = colors_for (self->wstyle, self->wstate);

  switch (self->wstyle) {
    case WSTYLE_CHECKBOX:
      fill_rounded_rect (widget, UI_BUTTON_RADIUS, colors.bg);
      draw_rounded_rect (widget, UI_BUTTON_RADIUS, colors.fg, 2.0f);
    break;

    case WSTYLE_IMAGE_TOGGLE:
    case WSTYLE_IMAGE_BUTTON:
      // xputty takes care of images for us, if we set them
      // using set_image_* methods.
      if (draw_button_image (widget, self))
        return;

      // didn't draw? fall back to normal frame/text
      fill_rounded_rect (widget, UI_BUTTON_RADIUS, colors.bg);
      draw_rounded_rect (widget, UI_BUTTON_RADIUS, colors.fg, 2.0f);
      draw_text_centered (widget, self->label.c_str (),
                          self->text_r, self->text_g, self->text_b);
    break;

    default:
      fill_rounded_rect (widget, UI_BUTTON_RADIUS, colors.bg);
      draw_rounded_rect (widget, UI_BUTTON_RADIUS, colors.fg, 2.0f);
      draw_text_centered (widget, self->label.c_str (),
                          self->text_r, self->text_g, self->text_b);
    break;
  }
}

void c_button::set_image (const unsigned char *pngdata, _widget_state which) {
  cairo_surface_t **csp = nullptr;

  switch (which) {
    case WSTATE_OFF:        csp = &img_off; break;
    case WSTATE_ON:         csp = &img_on; break;
    case WSTATE_HOVER:      csp = &img_hover; break;
    case WSTATE_DOWN:       csp = &img_down; break;
    case WSTATE_DOWN_HOVER: csp = &img_down_hover; break;
    case WSTATE_OFF_HOVER:  csp = &img_off_hover; break;
    default: return;
  }

  if (*csp) {
    cairo_surface_destroy(*csp);
    *csp = nullptr;
  }

  if (!pngdata)
    return;

  *csp = cairo_image_surface_create_from_stream(pngdata);

  if (cairo_surface_status(*csp) != CAIRO_STATUS_SUCCESS) {
    cairo_surface_destroy(*csp);
    *csp = nullptr;
  }
}

static void filepicker_response (void *w_, void *user_data) { CP
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

