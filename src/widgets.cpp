#include <string.h>
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>

#include "neuralblender.h"
#include "ui.h"

#include "xdrawing_area.h"
#include "xtooltip.h"
#include "widgets/xknob_private.h"
#include "xpngloader.h"
#include "widgets/xcombobox_private.h"

#include "data/data.h"

#define CMDLINE_DEBUG_COLOR ANSI_DARK_CYAN
#include "cmdline_debug.h"

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

static float knob_angle_from_value (float value, float min, float max) {
  if (max <= min)
    return 0.0f;

  float t = (value - min) / (max - min);
  t = std::clamp (t, 0.0f, 1.0f);

  const float start = 3.0f * M_PI / 4.0f; // 135 deg, lower-left
  const float sweep = 3.0f * M_PI / 2.0f; // 270 deg

  return start + t * sweep;
}


// Floats galore. Bring a row boat.
// (Codex generated most of this table)

static t_colortheme g_default_colors = {
  // window_bg
  solid  (0.08f, 0.085f, 0.09f, 1.0f),

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
    case WSTYLE_IMAGE_BUTTON_NOFRAME:
    case WSTYLE_IMAGE_TOGGLE:
    case WSTYLE_IMAGE_TOGGLE_NOFRAME:
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

  cairo_new_path (cr);

  r = std::clamp (r, 0.0, std::min (w, h) * 0.5);
  if (r <= 0.0) {
    cairo_rectangle (cr, x, y, w, h);
    return;
  }

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

static void draw_check_mark (
    Widget_t *w,
    int x,
    int y,
    int width,
    int height,
    const t_gradientcolors &colors) {

  if (!w || !w->crb || width <= 0 || height <= 0)
    return;

  cairo_save (w->crb);
  cairo_set_source_rgba (w->crb, colors.r1, colors.g1, colors.b1, colors.a1);
  cairo_set_line_width (w->crb, std::max (2.0, width * 0.16));
  cairo_set_line_cap (w->crb, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_join (w->crb, CAIRO_LINE_JOIN_ROUND);

  cairo_move_to (
      w->crb,
      x + width * 0.23,
      y + height * 0.52);
  cairo_line_to (
      w->crb,
      x + width * 0.43,
      y + height * 0.72);
  cairo_line_to (
      w->crb,
      x + width * 0.78,
      y + height * 0.28);
  cairo_stroke (w->crb);
  cairo_restore (w->crb);
}

static void draw_checkbox (
    Widget_t *w,
    const t_statecolors &colors,
    bool checked) {

  int x = 0, y = 0, width = 0, height = 0;
  if (!get_widget_size (w, &x, &y, &width, &height))
    return;

  const int size = std::max (4, height - 6);
  const int cx = x + 3;
  const int cy = y + (height - size) / 2;

  //fill_rounded_rect (w, x, y, width, height, 0.0f, g_colors->window_bg);
  fill_rounded_rect (w, cx, cy, size, size, UI_CHECKBOX_RADIUS, colors.bg);
  draw_rounded_rect (w, cx, cy, size, size, UI_CHECKBOX_RADIUS, colors.fg, 2.0f);

  if (checked)
    draw_check_mark (w, cx, cy, size, size, colors.fg);

	  const char *label = w->label ? w->label : "";
	  if (label [0] && width > size + 8) {
	    cairo_text_extents_t ext;
	    use_text_color_scheme (w, NORMAL_);
	    cairo_set_font_size (w->crb, w->app->normal_font / w->scale.ascale);
	    cairo_text_extents (w->crb, label, &ext);
    cairo_move_to (
        w->crb,
        cx + size + 8 - ext.x_bearing,
        y + (height - ext.height) * 0.5 - ext.y_bearing);
    cairo_show_text (w->crb, label);
  }
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

static c_toplevelwindow *toplevel_from_widget (void *w_) {
  Widget_t *w = (Widget_t *) w_;
  if (!w || !w->parent_struct)
    return NULL;

  return (c_toplevelwindow *) w->parent_struct;
}

static c_toplevelwindow *toplevel_from_child_widget (Widget_t *w) {
  while (w && w->parent) {
    Widget_t *parent_widget = (Widget_t *) w->parent;
    if ((parent_widget->flags & IS_WINDOW) && parent_widget->parent_struct)
      return (c_toplevelwindow *) parent_widget->parent_struct;

    w = parent_widget;
  }

  return NULL;
}

uint64_t get_unique_id () {
  static uint64_t current = 1;
  return current++;
}

void c_toplevelwindow::cb_expose (void *w_, void *user_data) {
  (void) user_data;

  c_toplevelwindow *window = toplevel_from_widget (w_);
  if (!window)
    return;

  window->on_expose ();
}

void c_toplevelwindow::cb_resize (void *w_, void *user_data) {
  (void) user_data;

  c_toplevelwindow *window = toplevel_from_widget (w_);
  if (!window)
    return;

  window->on_resize ();
}

void c_toplevelwindow::cb_key_press (void *w_, void *event, void *user_data) {
  (void) user_data;

  c_toplevelwindow *window = toplevel_from_widget (w_);
  if (!window)
    return;

  window->on_keydown ((XKeyEvent *) event);
}

void c_toplevelwindow::cb_close (void *w_, void *user_data) { CP
  (void) user_data;

  c_toplevelwindow *window = toplevel_from_widget (w_);
  if (!window)
    return;

  window->on_close ();
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
  add_tooltip (widget, ""); // creates initial tooltip
  set_tooltip ("");         // our function below hides it
  set_x11_window_background (widget, g_colors->window_bg);
  widget->flags |= USE_TRANSPARENCY;

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

bool c_widget::set_tooltip (const char *tt) {
  tooltip = tt ? tt : "";
  
  if (!widget)
    return false;
  
  if (tooltip.empty ()) {
    widget->flags &= ~HAS_TOOLTIP;
    return false;
  } else {
    tooltip_set_text (widget, tooltip.c_str ());
    widget->flags |= HAS_TOOLTIP;
    return true;
  }
}

bool c_widget::on_keydown (XKeyEvent *key) { CP
  (void) key;
  return false;
}

bool c_widget::on_keyup (XKeyEvent *key) { CP
  (void) key;
  return false;
}

void c_widget::focus () {
  c_toplevelwindow *top = toplevel_from_child_widget (widget);
  if (top)
    top->set_focused_widget (this);
}

void c_widget::clear_focus () {
  c_toplevelwindow *top = toplevel_from_child_widget (widget);
  if (top)
    top->clear_focus ();
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

bool c_widget::get_label_size (int *rw, int *rh, const char *text) {
  if (rw) *rw = 0;
  if (rh) *rh = 0;

  if (!widget || !widget->crb || !widget->app)
    return false;

  const char *s = text ? text : label.c_str ();
  if (!s)
    s = "";

  float fontsize = widget->app->normal_font / widget->scale.ascale;
  fontsize *= text_size;

  cairo_save (widget->crb);
  cairo_set_font_size (widget->crb, fontsize);

  cairo_text_extents_t ext;
  cairo_text_extents (widget->crb, s, &ext);
  cairo_restore (widget->crb);

  if (rw)
    *rw = (int) std::ceil (ext.width);
  if (rh)
    *rh = (int) std::ceil (ext.height);

  return true;
}

// -1 for padding x/y means stay at that size
void c_widget::resize_to_label (int padding_x, int padding_y) {
  int lw, lh;
  if (get_label_size (&lw, &lh)) {
    if (padding_x < 0)
      lw = w ();
    if (padding_y < 0)
      lh = h ();
    
    resize (lw + padding_x + 1, lh + padding_y + 1);
  }
}

int c_widget::x () {
  if (!widget || !widget->app)
    return 0;

  Metrics_t metrics;
  os_get_window_metrics (widget, &metrics);
  if (metrics.visible)
    return (int) (metrics.x / widget->app->hdpi);

  return (int) (widget->scale.init_x / widget->app->hdpi);
}

int c_widget::y () {
  if (!widget || !widget->app)
    return 0;

  Metrics_t metrics;
  os_get_window_metrics (widget, &metrics);
  if (metrics.visible)
    return (int) (metrics.y / widget->app->hdpi);

  return (int) (widget->scale.init_y / widget->app->hdpi);
}

int c_widget::w () {
  if (!widget || !widget->app)
    return 0;

  Metrics_t metrics;
  os_get_window_metrics (widget, &metrics);
  if (metrics.visible)
    return (int) (metrics.width / widget->app->hdpi);

  return (int) (widget->scale.init_width / widget->app->hdpi);
}

int c_widget::h () {
  if (!widget || !widget->app)
    return 0;

  Metrics_t metrics;
  os_get_window_metrics (widget, &metrics);
  if (metrics.visible)
    return (int) (metrics.height / widget->app->hdpi);

  return (int) (widget->scale.init_height / widget->app->hdpi);
}

bool c_toplevelwindow::create (
    c_neuralblender_ui *ui_,
    Window parent_,
    const char *title_,
    int x, int y, int w, int h,
    Widget_t *owner) {

  id = get_unique_id ();
  label = title_ ? title_ : "";
  ui = ui_;
  parent = parent_;

  if (!ui || !ui->display)
    return false;

  if (!parent)
    parent = DefaultRootWindow (ui->display);

  widget = create_window (&ui->app, parent, x, y, w, h);
  if (!widget)
    return false;

  window = widget->widget;
  widget->parent_struct = this;
  widget->label = label.c_str ();
  widget->scale.gravity = NONE;
  widget->func.resize_notify_callback = c_toplevelwindow::cb_resize;
  widget->func.expose_callback = c_toplevelwindow::cb_expose;
  widget->func.key_press_callback = c_toplevelwindow::cb_key_press;
  widget->func.unmap_notify_callback = c_toplevelwindow::cb_close;

  os_register_wm_delete_window (widget);
  auto_close (true);
  if (owner)
    os_set_transient_for_hint (owner, widget);
  set_title (title_);
  set_x11_window_background (widget, get_colortheme ()->window_bg);

  return true;
}

bool c_mainwindow::create (
    c_neuralblender_ui *ui_,
    Window parent_,
    const char *title_,
    int x, int y, int w, int h,
    Widget_t *owner) {

  if (!c_toplevelwindow::create (ui_, parent_, title_, x, y, w, h, owner))
    return false;

  auto_close (false);
  ui_->window = window;
  return true;
}

void c_toplevelwindow::set_min_size_to_current () {
  if (!widget)
    return;

  os_set_window_min_size (
      widget,
      widget->scale.init_width,
      widget->scale.init_height,
      widget->scale.init_width,
      widget->scale.init_height);
}

void c_toplevelwindow::set_min_size (int w, int h) {
  if (!widget)
    return;

  os_set_window_min_size (widget, w, h, w, h);
}

void c_toplevelwindow::show () {
  if (!widget)
    return;

  widget_show_all (widget);
  widget_draw (widget, NULL);
  if (widget->app && widget->app->dpy)
    XFlush (widget->app->dpy);
}

void c_toplevelwindow::hide () {
  if (!widget)
    return;

  clear_focus ();
  widget_hide (widget);
}

void c_toplevelwindow::auto_close (bool b) {
  if (!widget)
    return;

  if (b)
    widget->flags |= HIDE_ON_DELETE;
  else
    widget->flags &= ~HIDE_ON_DELETE;
}

void c_toplevelwindow::set_title (const char *title_) {
  label = title_ ? title_ : "";
  if (!widget)
    return;

  widget->label = label.c_str ();
  widget_set_title (widget, label.c_str ());
}

void c_toplevelwindow::set_icon_from_png (const unsigned char *png) {
  if (!widget || !png)
    return;

  widget_set_icon_from_png (widget, png);
}

bool c_toplevelwindow::request_size (int w, int h) {
  if (!widget || !widget->app || !widget->app->dpy)
    return false;

  const int sw = std::max (1, (int) (w * widget->app->hdpi));
  const int sh = std::max (1, (int) (h * widget->app->hdpi));

  os_resize_window (widget->app->dpy, widget, sw, sh);
  return true;
}

void c_toplevelwindow::set_focused_widget (c_widget *w) {
  if (focused_widget == w)
    return;

  c_widget *old = focused_widget;
  focused_widget = w;

  if (widget && widget->app) {
    if (focused_widget)
      widget->app->key_snooper = widget;
    else if (widget->app->key_snooper == widget)
      widget->app->key_snooper = NULL;
  }

  if (old)
    old->set_state (WSTATE_NORMAL);
  if (focused_widget)
    focused_widget->set_state (WSTATE_SELECTED);
}

void c_toplevelwindow::clear_focus () {
  set_focused_widget (NULL);
}

void c_toplevelwindow::on_expose () {
  if (!widget)
    return;

  Metrics_t metrics;
  os_get_window_metrics (widget, &metrics);
  if (!metrics.visible)
    return;

  fill_rounded_rect (widget, 0, 0, metrics.width, metrics.height,
                     0.0f, g_colors->window_bg);
}

void c_toplevelwindow::on_resize () { CP
}

bool c_toplevelwindow::on_keydown (XKeyEvent *key) { CP
  if (!widget || !key)
    return false;

  if (!focused_widget)
    return false;

  return focused_widget->on_keydown (key);
}

void c_mainwindow::on_resize () { CP
  if (!widget || !ui)
    return;

  Metrics_t metrics;
  os_get_window_metrics (widget, &metrics);
  if (!metrics.visible)
    return;

  ui->on_window_resize (
      metrics.width / widget->app->hdpi,
      metrics.height / widget->app->hdpi);
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


void c_container::create (
    c_neuralblender_ui *ui_,
    Widget_t *parent,
    const char *label_,
    int x, int y, int w, int h) {

  widget = create_widget (parent->app, parent, x, y, w, h);
  c_widget::create (ui_, parent, label_, x, y, w, h);
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

void c_label::create (
    c_neuralblender_ui *ui_,
    Widget_t *parent,
    const char *label_,
    int x, int y, int w, int h) {

  role = ROLE_UNKNOWN;
  widget = add_label (parent, label.c_str (), x, y, w, h);
  widget->func.expose_callback = c_label::cb_draw;
  c_widget::create (ui_, parent, label_, x, y, w, h);
}

void c_label::cb_draw (void *w_, void *ptr) {
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
  //cairo_rectangle (w->crb, 0, 0, metrics.width, metrics.height);
  //cairo_fill (w->crb);

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

////////////////////////////////////////////////////////////////////////////////
// c_textbox

static size_t utf8_prev_pos (const std::string &s, size_t pos) {
  pos = std::min (pos, s.size ());
  if (pos == 0)
    return 0;

  --pos;
  while (pos > 0 && ((unsigned char) s [pos] & 0xC0) == 0x80)
    --pos;

  return pos;
}

void c_textbox::create (
    c_neuralblender_ui *ui_,
    Widget_t *parent,
    const char *label_,
    int x, int y, int w, int h) {

  role = ROLE_UNKNOWN;
  widget = create_widget (parent->app, parent, x, y, w, h);
  if (!widget)
    return;

  widget->func.expose_callback = c_textbox::cb_draw;
  widget->func.button_press_callback = c_textbox::cb_button_press;
  //widget->flags |= USE_TRANSPARENCY;
  widget->scale.gravity = NONE;
  os_set_input_mask (widget);

  wstyle = WSTYLE_BUTTON;
  wstate = WSTATE_NORMAL;
  c_widget::create (ui_, parent, label_, x, y, w, h);
  set_text (label_);
}

bool c_textbox::set_text (const char *s) {
  value = s ? s : "";
  cursor = value.size ();
  label = value;

  if (widget) {
    widget->label = label.c_str ();
    expose ();
  }

  return true;
}

const std::string &c_textbox::text () const {
  return value;
}

void c_textbox::on_change (const std::string &s) { CP
  (void) s;
  if (!widget)
    return;
  expose_widget (widget);
}

void c_textbox::cb_draw (void *w_, void *user_data) {
  (void) user_data;

  Widget_t *w = (Widget_t *) w_;
  if (!w || !w->crb)
    return;

  c_textbox *self = (c_textbox *) w->parent_struct;
  if (!self)
    return;

  Metrics_t metrics;
  os_get_window_metrics (w, &metrics);
  if (!metrics.visible)
    return;
    
  // background and outline
  const bool selected = self->wstate == WSTATE_SELECTED;
  const t_statecolors &normal_colors = colors_for (WSTYLE_BUTTON, WSTATE_NORMAL);
  const t_statecolors &textbox_colors = colors_for (WSTYLE_FRAME, WSTATE_NORMAL);
  const t_statecolors &focus_colors = colors_for (WSTYLE_FRAME, WSTATE_SELECTED);

  fill_rounded_rect (w, UI_BUTTON_RADIUS, textbox_colors.bg);
  draw_rounded_rect (w, UI_BUTTON_RADIUS,
                     selected ? focus_colors.fg : normal_colors.fg,
                     selected ? 2.5f : 2.0f);

  cairo_save (w->crb);
  
  const double pad = 8.0 * w->app->hdpi;
  cairo_rectangle (
      w->crb,
      pad,
      2.0 * w->app->hdpi,
      std::max (0.0, metrics.width - pad * 2.0),
      std::max (0.0, metrics.height - 4.0 * w->app->hdpi));
  cairo_clip (w->crb);

  cairo_set_source_rgba (
      w->crb,
      normal_colors.fg.r1,
      normal_colors.fg.g1,
      normal_colors.fg.b1,
      normal_colors.fg.a1);
  cairo_set_font_size (w->crb, w->app->normal_font / w->scale.ascale);

  cairo_text_extents_t extents;
  cairo_text_extents (w->crb, self->value.c_str (), &extents);

  const double x = pad;
  const double y = (metrics.height - extents.height) * 0.5 - extents.y_bearing;

  cairo_move_to (w->crb, x, y);
  cairo_show_text (w->crb, self->value.c_str ());
  
  // cursor/caret
  if (selected) {
    const std::string before_cursor = self->value.substr (
        0, std::min (self->cursor, self->value.size ()));
    cairo_text_extents_t cursor_extents;
    cairo_text_extents (w->crb, before_cursor.c_str (), &cursor_extents);

    const double cx = x + cursor_extents.x_advance + 1.0;
    cairo_set_line_width (w->crb, 1.0);
    //cairo_move_to (w->crb, cx, y + extents.y_bearing - 4);
    //cairo_line_to (w->crb, cx, y + extents.y_bearing + extents.height + 5);
    cairo_move_to (w->crb, cx, 10);
    cairo_line_to (w->crb, cx, metrics.height - 10);
    cairo_stroke (w->crb);
  }

  cairo_restore (w->crb);
}

void c_textbox::cb_button_press (void *w_, void *event, void *user_data) {
  (void) event;
  (void) user_data;

  Widget_t *w = (Widget_t *) w_;
  if (!w || !w->parent_struct)
    return;

  c_textbox *self = (c_textbox *) w->parent_struct;
  self->focus ();
}

void c_textbox::cb_key_press (void *w_, void *event, void *user_data) {
  (void) user_data;

  Widget_t *w = (Widget_t *) w_;
  XKeyEvent *key = (XKeyEvent *) event;
  if (!w || !key || !w->parent_struct)
    return;

  c_textbox *self = (c_textbox *) w->parent_struct;
  self->on_keydown (key);
}

bool c_textbox::on_keydown (XKeyEvent *key) { CP
  if (!widget || !key)
    return false;

  bool changed = false;

  switch (key_mapping (widget->app->dpy, key)) {
    case 10: // return
      on_change (value);
      expose_widget (widget);
      return true;
    
    case 11: { // backspace
      const size_t prev = utf8_prev_pos (value, cursor);
      if (prev < cursor) {
        value.erase (prev, cursor - prev);
        cursor = prev;
        changed = true;
      }
    } break;

    default: {
      
      char buf [32] = {};
      if (os_get_keyboard_input (widget, key, buf, sizeof (buf) - 1) && buf [0]) {
        //debug ("buf [0]=%d", (int) buf [0]);
        if (buf [0] >= 32) {
          cursor = std::min (cursor, value.size ());
          value.insert (cursor, buf);
          cursor += strlen (buf);
          changed = true;
        }
      }
    } break;
  }

  if (changed) {
    label = value;
    widget->label = label.c_str ();
    on_change (value);
  }

  expose_widget (widget);
  return changed;
}

////////////////////////////////////////////////////////////////////////////////
// c_image

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

static cairo_surface_t *button_image_for_state (c_button *b, Widget_t *w) {
  const bool on = w->adj && adj_get_value (w->adj) >= 0.5f;
  const bool hover = (w->flags & HAS_FOCUS) || w->state == 1;
  const bool down = w->state == 2;
  
  //if (b->img_default) return b->img_default;
  if (down && hover && b->img_down_hover) return b->img_down_hover;
  else if (down && b->img_down) return b->img_down;
  else if (!on && hover && b->img_off_hover) return b->img_off_hover;
  else if (hover && b->img_hover) return b->img_hover;
  else if (on && b->img_on) return b->img_on;
  
  if (b->img_default) return b->img_default;
  return b->img_off;
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
  b->clear_focus ();
  b->on_mouseup ();
}

static void button_mouse_down (void *w_, void *event, void *user_data) {
  (void) event;
  (void) user_data;

  auto *w = (Widget_t *) w_;
  if (!w || !w->parent_struct)
    return;

  auto *b = (c_button *) w->parent_struct;
  b->clear_focus ();
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
  b->clear_focus ();
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

c_button::c_button () { }

c_button::~c_button () {
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
  widget->func.button_press_callback = button_mouse_down;
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
    return true; // not shown

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

static bool draw_button_image_and_text (Widget_t *w, c_button *b) {
  if (!w || !b || !w->crb)
    return false;

  cairo_surface_t *img = button_image_for_state (b, w);
  const bool have_img = img && cairo_surface_status (img) == CAIRO_STATUS_SUCCESS;
  const bool have_text = !b->label.empty ();

  if (!have_img && !have_text)
    return false;

  Metrics_t m;
  os_get_window_metrics (w, &m);
  if (!m.visible)
    return true;

  if (!have_img) {
    c_widget::draw_text_centered (
        w,
        b->label.c_str (),
        b->text_r,
        b->text_g,
        b->text_b);
    return true;
  }

  const double iw = cairo_image_surface_get_width (img);
  const double ih = cairo_image_surface_get_height (img);
  if (iw <= 0 || ih <= 0)
    return false;
  
  const double pad = b->padding;
  /*if (b->padding < 0)
    pad = std::max (4.0, (double) m.height * 0.2);*/
  
  const double gap = have_text ? std::max (4.0, (double) m.height * 0.14) : 0.0;
  const double max_img_h = std::max (1.0, (double) m.height - pad * 2.0);
  double img_h = max_img_h;
  double img_w = img_h * iw / ih;

  const double max_img_w = std::max (1.0, (double) m.width - pad * 2.0);
  if (!have_text && img_w > max_img_w) {
    img_w = max_img_w;
    img_h = img_w * ih / iw;
  }
  
  cairo_save (w->crb);
  
  float fontsize = w->app->normal_font / w->scale.ascale;
  fontsize *= b->text_size;
  cairo_set_font_size (w->crb, fontsize);

  cairo_text_extents_t ext {};
  if (have_text)
    cairo_text_extents (w->crb, b->label.c_str (), &ext);

  double text_w = have_text ? ext.width : 0.0;
  double total_w = img_w + gap + text_w;
  const double max_total_w = std::max (1.0, (double) m.width - pad * 2.0);
  if (have_text && total_w > max_total_w) {
    const double excess = total_w - max_total_w;
    const double min_img_h = std::max (1.0, (double) m.height * 0.35);
    img_h = std::max (min_img_h, img_h - excess);
    img_w = img_h * iw / ih;
    total_w = img_w + gap + text_w;
  }

  double x = ((double) m.width - total_w) * 0.5;
  if (x < pad)
    x = pad;

  const double y = ((double) m.height - img_h) * 0.5;

  cairo_save (w->crb);
  cairo_translate (w->crb, x, y);
  cairo_scale (w->crb, img_w / iw, img_h / ih);
  cairo_set_source_surface (w->crb, img, 0, 0);
  cairo_rectangle (w->crb, 0, 0, iw, ih);
  cairo_fill (w->crb);
  cairo_restore (w->crb);

  if (have_text) {
    cairo_set_source_rgba (w->crb, b->text_r, b->text_g, b->text_b, 1.0);
    cairo_move_to (
        w->crb,
        x + img_w + gap - ext.x_bearing,
        ((double) m.height - ext.height) * 0.5 - ext.y_bearing);
    cairo_show_text (w->crb, b->label.c_str ());
  }

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
      draw_checkbox (widget, colors, self->value);
    break;
    
    case WSTYLE_IMAGE_TOGGLE_NOFRAME:
    case WSTYLE_IMAGE_BUTTON_NOFRAME:
      // xputty takes care of images for us, if we set them
      // using set_image_* methods.
      draw_button_image (widget, self);
    break;

    case WSTYLE_IMAGE_TOGGLE:
    case WSTYLE_IMAGE_BUTTON:
      fill_rounded_rect (widget, UI_BUTTON_RADIUS, colors.bg);
      draw_rounded_rect (widget, UI_BUTTON_RADIUS, colors.fg, 2.0f);
      if (draw_button_image_and_text (widget, self))
        return;

      // didn't draw? fall back to normal frame/text
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
    case WSTATE_DEFAULT:    csp = &img_default; break;
    case WSTATE_ALL:
      set_image (pngdata, WSTATE_OFF);
      set_image (pngdata, WSTATE_ON);
      set_image (pngdata, WSTATE_HOVER);
      set_image (pngdata, WSTATE_DOWN);
      set_image (pngdata, WSTATE_DOWN_HOVER);
      set_image (pngdata, WSTATE_OFF_HOVER);
      set_image (pngdata, WSTATE_DEFAULT);
      return;
    break;
      return;
    break;
    default: return;
  }
  CP
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

void c_button::on_mouseup () {
  ui->on_button (this, value);
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
  expose ();
  widget->func.value_changed_callback = oldvaluechanged;
  widget->func.adj_callback = oldadj;

  return true;
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

Widget_t *g_knob_image = NULL;

void c_knob::create (
    c_neuralblender_ui *ui_,
    Widget_t *parent,
    const char *label_,
    int x, int y, int w, int h) {
  
  if (!g_knob_image) {
    g_knob_image = create_widget (parent->app, parent, 0, 0, 1, 1);
    widget_get_png (g_knob_image, data_knob_png);
    widget_hide (g_knob_image);
  }
  label = label_;
  widget = add_knob (parent, label.c_str (), x, y, w, h);
  widget->func.expose_callback = c_knob::cb_draw;
  widget->func.value_changed_callback = knob_value_changed;
  widget->func.double_click_callback = knob_double_click;

  c_widget::create (ui_, parent, label_, x, y, w, h);
}

void c_knob::cb_draw (void *w, void *user_data) {
  Widget_t *widget = (Widget_t *) w;
  if (!widget || !widget->parent_struct)
    return;
    
  c_knob *knob = (c_knob *) widget->parent_struct;

  if (g_knob_image && g_knob_image->image && widget->image != g_knob_image->image)
    widget_get_surface_ptr (widget, g_knob_image);

  _draw_knob (widget, user_data);
  
  if (widget->crb) {
    // arc around knob - thanks to codex for help with the math!
    int w, h;
    float a0 = 3.0f * M_PI / 4.0f;
    float a1 = knob_angle_from_value (knob->value, knob->min, knob->max);
    get_widget_size (widget, NULL, NULL, &w, &h);
    int cx = w / 2 - 2;
    int cy = cx;//h / 2;
    cairo_arc (widget->crb, cx, cy, cx - cx / 16, a0, a1);
    cairo_stroke (widget->crb);
    
    // little dot indicating value
    float dot_r = std::max (2.0f, (float) (cx - 2) / 12);
    float dot_dist = (cx / 2) * 0.95f;
    float dot_x = cx + cosf (a1) * dot_dist;
    float dot_y = cy + sinf (a1) * dot_dist;
    cairo_arc (widget->crb, dot_x, dot_y, dot_r, 0.0, 2.0 * M_PI);
    cairo_fill (widget->crb);
  }
 
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
  expose ();
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
  expose ();
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
  expose ();
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
    int show_items = 16;
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
    combobox_set_menu_size (widget, 16);
    widget->label = label.c_str ();
  }
  updating_widget = false;

  expose ();
}

void combobox_selected_callback (void *w_, void *user_data) { CP
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
  //os_set_transient_for_hint (ui->mainwindow.widget, widget);
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
