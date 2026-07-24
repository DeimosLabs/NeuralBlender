
/* NeuralBlender - RTNeural / NAM based amp modeler
 *
 * nbtk widget and native-window implementation.
 */

#include <string.h>
#include <algorithm>
#include <cstdlib>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "neuralblender.h"
#include "ui.h"
#include "widgets.h"

#include "data/data.h"

#define CMDLINE_DEBUG_COLOR ANSI_DARK_CYAN
#include "cmdline_debug.h"

static constexpr float DEFAULT_BG_R = 0.125f;
static constexpr float DEFAULT_BG_G = 0.125f;
static constexpr float DEFAULT_BG_B = 0.125f;

//uint64_t now_ms ();

uint64_t get_unique_id () {
  static uint64_t current = 1;
  return current++;
}

static const t_statecolors &colors_for (_widget_style style, _widget_state state);
static cairo_surface_t *get_knob_image_surface ();
static bool draw_nbtk_knob_image (
    cairo_t *cr,
    double x,
    double y,
    double size);
static void set_x11_window_background (
    Widget_t *w,
    const t_gradientcolors &colors);
static void disable_x11_window_background (Widget_t *w);

static cairo_surface_t *g_knob_image_surface = NULL;

static cairo_surface_t *get_knob_image_surface () {
  if (!g_knob_image_surface) {
    g_knob_image_surface = cairo_image_surface_create_from_stream (data_knob_png);

    if (cairo_surface_status (g_knob_image_surface) != CAIRO_STATUS_SUCCESS) {
      cairo_surface_destroy (g_knob_image_surface);
      g_knob_image_surface = NULL;
    }
  }

  return g_knob_image_surface;
}

static bool draw_nbtk_knob_image (
    cairo_t *cr,
    double x,
    double y,
    double size) {

  if (!cr || size <= 0.0)
    return false;

  cairo_surface_t *surface = get_knob_image_surface ();
  if (!surface)
    return false;

  const double iw = cairo_image_surface_get_width (surface);
  const double ih = cairo_image_surface_get_height (surface);
  if (iw <= 0.0 || ih <= 0.0)
    return false;

  cairo_save (cr);
  cairo_translate (cr, x, y);
  cairo_scale (cr, size / iw, size / ih);
  cairo_set_source_surface (cr, surface, 0.0, 0.0);
  cairo_rectangle (cr, 0.0, 0.0, iw, ih);
  cairo_fill (cr);
  cairo_restore (cr);
  return true;
}

namespace nbtk {

static Widget_t *as_xputty_widget (t_native_handle handle) {
  return (Widget_t *) handle;
}

class c_x11_native_backend : public c_native_backend {
public:
  void init_app (t_native_app *app) override {
    if (app)
      main_init (app);
  }

  void shutdown_app (t_native_app *app) override {
    if (app)
      main_quit (app);
  }

  void run_events (t_native_app *app) override {
    if (app)
      run_embedded (app);
  }

  void flush_dirty (t_native_app *app) override {
    if (app)
      draw_dirty_widgets (app);
  }

  bool is_running (const t_native_app *app) const override {
    return app && app->run;
  }

  t_native_display display (const t_native_app *app) const override {
    return app ? app->dpy : nullptr;
  }

  t_native_window default_root_window (t_native_display display) const override {
    return display ? DefaultRootWindow (display) : 0;
  }

  bool window_size (
      t_native_display display,
      t_native_window window,
      double hdpi,
      int *w,
      int *h) const override {

    if (!display || !window || !w || !h)
      return false;

    XWindowAttributes attr;
    if (!XGetWindowAttributes (display, window, &attr))
      return false;

    if (attr.width <= 0 || attr.height <= 0)
      return false;

    const double scale = hdpi > 0.0 ? hdpi : 1.0;
    *w = (int) (attr.width / scale);
    *h = (int) (attr.height / scale);
    return *w > 0 && *h > 0;
  }

  void invalidate (t_native_handle handle) override {
    Widget_t *widget = as_xputty_widget (handle);
    if (widget)
      expose_widget (widget);
  }

  void flush (t_native_handle handle) override {
    Widget_t *widget = as_xputty_widget (handle);
    if (widget && widget->app && widget->app->dpy)
      XFlush (widget->app->dpy);
  }

  bool grab_pointer (t_native_handle handle) override {
    Widget_t *widget = as_xputty_widget (handle);
    if (!widget || !widget->app || !widget->app->dpy)
      return false;

    const int grab = XGrabPointer (
        widget->app->dpy,
        widget->widget,
        False,
        ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
        GrabModeAsync,
        GrabModeAsync,
        None,
        None,
        CurrentTime);
    return grab == GrabSuccess;
  }

  void ungrab_pointer (t_native_handle handle) override {
    Widget_t *widget = as_xputty_widget (handle);
    if (widget && widget->app && widget->app->dpy)
      XUngrabPointer (widget->app->dpy, CurrentTime);
  }

  bool query_pointer (t_native_handle handle, t_point *local) const override {
    Widget_t *widget = as_xputty_widget (handle);
    if (!widget || !widget->app || !widget->app->dpy || !local)
      return false;

    Window root_return = 0;
    Window child_return = 0;
    int root_x = 0;
    int root_y = 0;
    int win_x = 0;
    int win_y = 0;
    unsigned int mask_return = 0;
    if (!XQueryPointer (
        widget->app->dpy,
        widget->widget,
        &root_return,
        &child_return,
        &root_x,
        &root_y,
        &win_x,
        &win_y,
        &mask_return))
      return false;

    const float hdpi = widget->app->hdpi;
    local->x = (int) (win_x / hdpi);
    local->y = (int) (win_y / hdpi);
    return true;
  }

  void set_window_background (
      t_native_handle handle,
      const t_gradientcolors &colors) override {
    Widget_t *widget = as_xputty_widget (handle);
    set_x11_window_background (widget, colors);
  }

  void disable_window_background (t_native_handle handle) override {
    Widget_t *widget = as_xputty_widget (handle);
    disable_x11_window_background (widget);
  }

  void set_min_size (t_native_handle handle, int w, int h) override {
    Widget_t *widget = as_xputty_widget (handle);
    if (!widget || !widget->app || !widget->app->dpy)
      return;

    Display *display = widget->app->dpy;
    set_window_hints (display, widget->widget, w, h);

    Window root = 0;
    Window parent = 0;
    Window *children = NULL;
    unsigned int nchildren = 0;
    Window current = widget->widget;

    for (int depth = 0; depth < 8; ++depth) {
      if (!XQueryTree (
          display,
          current,
          &root,
          &parent,
          &children,
          &nchildren))
        break;

      if (children)
        XFree (children);

      if (!parent || parent == root)
        break;

      set_window_hints (display, parent, w, h);
      current = parent;
    }
  }

  bool request_size (t_native_handle handle, int w, int h) override {
    Widget_t *widget = as_xputty_widget (handle);
    if (!widget || !widget->app || !widget->app->dpy)
      return false;

    const float hdpi = widget->app->hdpi;
    os_resize_window (
        widget->app->dpy,
        widget,
        std::max (1, (int) (w * hdpi)),
        std::max (1, (int) (h * hdpi)));
    return true;
  }

  void move_resize (t_native_handle handle, int x, int y, int w, int h) override {
    Widget_t *widget = as_xputty_widget (handle);
    if (!widget || !widget->app)
      return;

    const float hdpi = widget->app->hdpi;
    os_move_window (
        widget->app->dpy,
        widget,
        (int) (x * hdpi),
        (int) (y * hdpi));
    os_resize_window (
        widget->app->dpy,
        widget,
        std::max (1, (int) (w * hdpi)),
        std::max (1, (int) (h * hdpi)));
  }

  void set_mouse_cursor (t_native_handle handle, _mouse_cursor cursor) override {
    Widget_t *widget = as_xputty_widget (handle);
    if (!widget || !widget->app || !widget->app->dpy)
      return;

    if (cursor == MOUSE_CURSOR_HAND) {
      Cursor xcursor = XCreateFontCursor (widget->app->dpy, XC_hand2);
      XDefineCursor (widget->app->dpy, widget->widget, xcursor);
      XFreeCursor (widget->app->dpy, xcursor);
    } else {
      XUndefineCursor (widget->app->dpy, widget->widget);
    }
  }

  t_native_window root_window (t_native_handle handle, bool is_widget) const override {
    Widget_t *widget = as_xputty_widget (handle);
    if (!widget || !widget->app)
      return 0;

    return os_get_root_window (widget->app, is_widget ? IS_WIDGET : IS_WINDOW);
  }

  t_point root_to_screen (t_native_handle handle, t_point p) const override {
    Widget_t *widget = as_xputty_widget (handle);
    if (!widget || !widget->app)
      return p;

    int sx = p.x;
    int sy = p.y;
    const float hdpi = widget->app->hdpi;
    os_translate_coords (
        widget,
        widget->widget,
        root_window (handle, true),
        (int) (p.x * hdpi),
        (int) (p.y * hdpi),
        &sx,
        &sy);

    return {
      (int) (sx / hdpi),
      (int) (sy / hdpi)
    };
  }

  t_point screen_to_root (t_native_handle handle, t_point p) const override {
    Widget_t *widget = as_xputty_widget (handle);
    if (!widget || !widget->app)
      return p;

    int rx = p.x;
    int ry = p.y;
    const float hdpi = widget->app->hdpi;
    os_translate_coords (
        widget,
        root_window (handle, true),
        widget->widget,
        (int) (p.x * hdpi),
        (int) (p.y * hdpi),
        &rx,
        &ry);

    return {
      (int) (rx / hdpi),
      (int) (ry / hdpi)
    };
  }

private:
  static void set_window_hints (Display *display, Window window, int w, int h) {
    if (!display || !window)
      return;

    XSizeHints *hints = XAllocSizeHints ();
    if (!hints)
      return;

    hints->flags = PMinSize | PBaseSize | PWinGravity;
    hints->min_width = w;
    hints->min_height = h;
    hints->base_width = w;
    hints->base_height = h;
    hints->win_gravity = CenterGravity;
    XSetWMNormalHints (display, window, hints);
    XFree (hints);
  }
};

std::unique_ptr<c_native_backend> create_native_backend () {
  return std::make_unique<c_x11_native_backend> ();
}

bool t_rect::contains (int px, int py) const {
  return px >= x && py >= y && px < x + w && py < y + h;
}

t_action_event::t_action_event () {
  type = e_event_type::action;
}

t_command_event::t_command_event () {
  type = e_event_type::command;
}

void c_widget::create (
    c_widget *parent_,
    const char *label_,
    int x_,
    int y_,
    int w_,
    int h_) {
  
  id = get_unique_id ();
  parent = parent_;
  app = parent ? parent->app : nullptr;
  label = label_ ? label_ : "";
  x = x_;
  y = y_;
  w = w_;
  h = h_;

  if (parent)
    parent->children.push_back (this);
}

void c_widget::draw (cairo_t *cr) {
  (void) cr;
}

bool c_widget::on_mouse_down (int x_, int y_, int button) {
  (void) x_;
  (void) y_;
  last_mouse_button = button;

  if (!wants_mouse)
    return false;

  const bool changed = !pressed || !hovered;
  pressed = true;
  hovered = true;
  if (changed)
    invalidate ();

  return true;
}

bool c_widget::on_mouse_up (int x_, int y_, int button) {
  (void) x_;
  (void) y_;
  last_mouse_button = button;

  if (!wants_mouse)
    return false;

  if (pressed) {
    pressed = false;
    invalidate ();
  }

  return true;
}

bool c_widget::on_mouse_move (int x_, int y_) {
  (void) x_;
  (void) y_;
  return false;
}

void c_widget::on_mouse_enter () {
  if (!wants_hover)
    return;

  if (!hovered) {
    hovered = true;
    invalidate ();
  }
}

void c_widget::on_mouse_leave () {
  if (!wants_hover && !pressed)
    return;

  if (hovered || pressed) {
    hovered = false;
    pressed = false;
    invalidate ();
  }
}

bool c_widget::on_key_down (int key) {
  (void) key;
  return false;
}

bool c_widget::on_key_up (int key) {
  (void) key;
  return false;
}

bool c_widget::on_text_input (const char *text) {
  (void) text;
  return false;
}

void c_widget::clear_hover () {
  on_mouse_leave ();
}

void c_widget::on_event (t_event &event) {
  if (parent && !event.handled)
    parent->on_event (event);
  else if (app && !event.handled)
    app->on_event (event);
}

void c_widget::on_action (t_action_event &event) {
  if (action_parent && !event.handled)
    action_parent->on_action (event);
  else if (parent && !event.handled)
    parent->on_action (event);
  else if (app && !event.handled)
    app->on_action (event);
}

void c_widget::on_command (t_command_event &event) {
  if (parent && !event.handled)
    parent->on_command (event);
  else if (app && !event.handled)
    app->on_command (event);
}

void c_widget::draw_tree (cairo_t *cr) {
  if (!visible || !cr || w <= 0 || h <= 0)
    return;

  cairo_save (cr);
  cairo_translate (cr, x, y);
  cairo_rectangle (cr, 0, 0, w, h);
  cairo_clip (cr);

  draw (cr);

  for (c_widget *child : children) {
    if (child)
      child->draw_tree (cr);
  }

  cairo_restore (cr);
}

bool c_widget::mouse_down_tree (int px, int py, int button) { CP
  if (!visible || !enabled || !contains_local (px, py))
    return false;

  const int lx = px - x;
  const int ly = py - y;

  for (auto it = children.rbegin (); it != children.rend (); ++it) {
    c_widget *child = *it;
    if (child && child->mouse_down_tree (lx, ly, button))
      return true;
  }

  return on_mouse_down (lx, ly, button);
}

bool c_widget::mouse_up_tree (int px, int py, int button) { CP
  if (!visible || !enabled || !contains_local (px, py))
    return false;

  const int lx = px - x;
  const int ly = py - y;

  for (auto it = children.rbegin (); it != children.rend (); ++it) {
    c_widget *child = *it;
    if (child && child->mouse_up_tree (lx, ly, button))
      return true;
  }

  return on_mouse_up (lx, ly, button);
}

bool c_widget::mouse_move_tree (int px, int py) {
  if (!visible || !enabled || !contains_local (px, py))
    return false;

  const int lx = px - x;
  const int ly = py - y;

  for (auto it = children.rbegin (); it != children.rend (); ++it) {
    c_widget *child = *it;
    if (child && child->mouse_move_tree (lx, ly))
      return true;
  }

  return on_mouse_move (lx, ly);
}

bool c_widget::update_hover_tree (int px, int py) {
  if (!visible || !enabled || !contains_local (px, py)) {
    clear_hover_tree ();
    return false;
  }

  const int lx = px - x;
  const int ly = py - y;
  bool handled = false;

  if (!mouse_inside) {
    mouse_inside = true;
    on_mouse_enter ();
  }
  if (app && wants_hover)
    app->hovered_widget = this;

  for (c_widget *child : children) {
    if (child && child->update_hover_tree (lx, ly))
      handled = true;
  }

  if (!handled)
    handled = on_mouse_move (lx, ly);

  return handled;
}

void c_widget::clear_hover_tree () {
  for (c_widget *child : children) {
    if (child)
      child->clear_hover_tree ();
  }

  if (mouse_inside) {
    mouse_inside = false;
    if (app && app->hovered_widget == this)
      app->hovered_widget = nullptr;
    on_mouse_leave ();
  }

  clear_hover ();
}

void c_frame::draw (cairo_t *cr) {
  if (!cr)
    return;

  const t_statecolors &colors = colors_for (WSTYLE_FRAME, state);
  tk_path_rounded_rect (cr, 1, 1, w - 2, h - 2, UI_FRAME_RADIUS);
  tk_set_gradient (cr, h, colors.bg);
  cairo_fill_preserve (cr);

  tk_set_gradient (cr, h, colors.fg);
  cairo_set_line_width (cr, 2.0);
  cairo_stroke (cr);
}

static double tk_aligned_text_x (
    int w,
    const cairo_text_extents_t &ext,
    double pad,
    _textalign align) {

  switch (align) {
    case TEXT_LEFT:
      return pad - ext.x_bearing;

    case TEXT_RIGHT:
      return (double) w - pad - ext.width - ext.x_bearing;

    case TEXT_CENTER:
    default:
      return ((double) w - ext.width) * 0.5 - ext.x_bearing;
  }
}

void c_label::draw (cairo_t *cr) {
  if (!cr)
    return;

  cairo_select_font_face (
    cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
  const float font_size = size * font_multiplier ();
  cairo_set_font_size (cr, font_size);
  if (link)
    cairo_set_source_rgb (cr, 0.45, 0.72, 1.0);
  else
    cairo_set_source_rgb (cr, 0.92, 0.92, 0.92);

  cairo_text_extents_t ext;
  cairo_text_extents (cr, label.c_str (), &ext);
  const double text_x = tk_aligned_text_x (
      w, ext, 4.0 * font_multiplier (), align);
  cairo_move_to (
    cr,
    text_x,
    (h - ext.height) * 0.5 - ext.y_bearing);
  cairo_show_text (cr, label.c_str ());

  if (link) {
    const double underline_y = (h + ext.height) * 0.5 + 2.0 * font_multiplier ();
    cairo_move_to (cr, text_x + ext.x_bearing, underline_y);
    cairo_line_to (cr, text_x + ext.x_bearing + ext.width, underline_y);
    cairo_set_line_width (cr, 1.0);
    cairo_stroke (cr);
  }
}

static void tk_open_link (const std::string &url) {
  if (url.empty ())
    return;

  pid_t pid = fork ();
  if (pid < 0)
    return;

  if (pid == 0) {
    execlp ("xdg-open", "xdg-open", url.c_str (), (char *) NULL);
    _exit (127);
  }

  int dummy = 0;
  waitpid (pid, &dummy, 0);
}

bool c_label::on_mouse_down (int x_, int y_, int button) {
  if (!link)
    return c_widget::on_mouse_down (x_, y_, button);

  c_widget::on_mouse_down (x_, y_, button);
  if (button == Button1)
    tk_open_link (label);

  return true;
}

bool c_label::on_mouse_move (int x_, int y_) {
  (void) x_;
  (void) y_;
  return link;
}

void c_label::on_mouse_enter () {
  if (!link) {
    c_widget::on_mouse_enter ();
    return;
  }

  wants_hover = true;
  c_widget::on_mouse_enter ();
  if (app)
    app->set_mouse_cursor (MOUSE_CURSOR_HAND);
}

void c_label::on_mouse_leave () {
  if (link && app)
    app->set_mouse_cursor (MOUSE_CURSOR_DEFAULT);
  c_widget::on_mouse_leave ();
}

void c_label::clear_hover () {
  if (link)
    on_mouse_leave ();
  else
    c_widget::clear_hover ();
}

static void tk_draw_surface_scaled (
    cairo_t *cr,
    cairo_surface_t *surface,
    double x,
    double y,
    double w,
    double h) {

  if (!cr || !surface)
    return;

  const double iw = cairo_image_surface_get_width (surface);
  const double ih = cairo_image_surface_get_height (surface);
  if (iw <= 0 || ih <= 0)
    return;

  cairo_save (cr);
  cairo_translate (cr, x, y);
  cairo_scale (cr, w / iw, h / ih);
  cairo_set_source_surface (cr, surface, 0, 0);
  cairo_rectangle (cr, 0, 0, iw, ih);
  cairo_fill (cr);
  cairo_restore (cr);
}

c_button::c_button () {
  wants_mouse = true;
  wants_hover = true;
}

c_button::~c_button () {
  destroy_images ();
}

cairo_surface_t *c_button::image_for_state () const {
  if (pressed && hovered && img_down_hover) return img_down_hover;
  if (pressed && img_down)                  return img_down;
  if (!value && hovered && img_off_hover)   return img_off_hover;
  if (hovered && img_hover)                 return img_hover;
  if (value && img_on)                      return img_on;
  if (img_default)                          return img_default;
  return img_off;
}

static _widget_state tk_button_state (const c_button &button) {
  if (button.pressed && button.hovered) return WSTATE_DOWN_HOVER;
  if (button.pressed)                   return WSTATE_DOWN;
  if (!button.value && button.hovered)  return WSTATE_OFF_HOVER;
  if (button.value && button.hovered)   return WSTATE_ON_HOVER;
  if (button.hovered)                   return WSTATE_HOVER;
  if (button.value)                     return WSTATE_ON;
  return WSTATE_OFF;
}

void c_button::destroy_images () {
  if (img_off)        cairo_surface_destroy (img_off);
  if (img_on)         cairo_surface_destroy (img_on);
  if (img_hover)      cairo_surface_destroy (img_hover);
  if (img_down)       cairo_surface_destroy (img_down);
  if (img_down_hover) cairo_surface_destroy (img_down_hover);
  if (img_off_hover)  cairo_surface_destroy (img_off_hover);
  if (img_default)    cairo_surface_destroy (img_default);

  img_off = nullptr;
  img_on = nullptr;
  img_hover = nullptr;
  img_down = nullptr;
  img_down_hover = nullptr;
  img_off_hover = nullptr;
  img_default = nullptr;
  img_default_source = nullptr;
}

void c_button::draw (cairo_t *cr) {
  if (!cr)
    return;

  const t_statecolors &colors = colors_for (WSTYLE_BUTTON, tk_button_state (*this));
  tk_path_rounded_rect (cr, 1, 1, w - 2, h - 2, UI_BUTTON_RADIUS);
  tk_set_gradient (cr, h, colors.bg);
  cairo_fill_preserve (cr);
  tk_set_gradient (cr, h, colors.fg);
  cairo_set_line_width (cr, 2.0);
  cairo_stroke (cr);

  cairo_surface_t *img = image_for_state ();
  const bool have_img = img && cairo_surface_status (img) == CAIRO_STATUS_SUCCESS;
  const bool have_text = !label.empty ();
  if (have_img) {
    const double pad = std::max (0.0f, padding * font_multiplier ());
    const double gap = have_text ? std::max (4.0, (double) h * 0.14) : 0.0;
    const double iw = cairo_image_surface_get_width (img);
    const double ih = cairo_image_surface_get_height (img);
    if (iw > 0 && ih > 0) {
      double img_h = std::max (1.0, (double) h - pad * 2.0);
      double img_w = img_h * iw / ih;
      const double max_img_w = std::max (1.0, (double) w - pad * 2.0);
      if (!have_text && img_w > max_img_w) {
        img_w = max_img_w;
        img_h = img_w * ih / iw;
      }

      cairo_save (cr);
      cairo_set_font_size (cr, 14.0 * font_multiplier ());
      cairo_text_extents_t ext {};
      if (have_text)
        cairo_text_extents (cr, label.c_str (), &ext);

      double total_w = img_w + gap + (have_text ? ext.width : 0.0);
      const double max_total_w = std::max (1.0, (double) w - pad * 2.0);
      if (have_text && total_w > max_total_w) {
        const double excess = total_w - max_total_w;
        const double min_img_h = std::max (1.0, (double) h * 0.35);
        img_h = std::max (min_img_h, img_h - excess);
        img_w = img_h * iw / ih;
        total_w = img_w + gap + ext.width;
      }

      cairo_text_extents_t total_ext {};
      total_ext.width = total_w;
      double x = tk_aligned_text_x (w, total_ext, pad, align);
      if (align == TEXT_LEFT && x < pad)
        x = pad;
      else if (align == TEXT_RIGHT && x + total_w > (double) w - pad)
        x = (double) w - pad - total_w;
      else if (x < pad)
        x = pad;
      const double y = ((double) h - img_h) * 0.5;
      tk_draw_surface_scaled (cr, img, x, y, img_w, img_h);

      if (have_text) {
        cairo_set_source_rgb (cr, 0.95, 0.95, 0.95);
        cairo_move_to (
          cr,
          x + img_w + gap - ext.x_bearing,
          ((double) h - ext.height) * 0.5 - ext.y_bearing);
        cairo_show_text (cr, label.c_str ());
      }
      cairo_restore (cr);
      return;
    }
  }

  cairo_select_font_face (
    cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size (cr, 14.0 * font_multiplier ());
  cairo_set_source_rgb (cr, 0.95, 0.95, 0.95);
  cairo_text_extents_t ext;
  cairo_text_extents (cr, label.c_str (), &ext);
  const double text_x = tk_aligned_text_x (
      w, ext, 8.0 * font_multiplier (), align);
  cairo_move_to (
    cr,
    text_x,
    (h - ext.height) * 0.5 - ext.y_bearing);
  cairo_show_text (cr, label.c_str ());
}

bool c_button::on_mouse_down (int x_, int y_, int button) {
  c_widget::on_mouse_down (x_, y_, button);
  return true;
}

bool c_button::on_mouse_up (int x_, int y_, int button) {
  c_widget::on_mouse_up (x_, y_, button);
  if (is_toggle)
    value = !value;

  t_action_event event;
  event.source = this;
  event.source_id = id;
  event.mouse_button = button;
  event.value = value;
  on_action (event);

  return true;
}

bool c_button::on_mouse_move (int x_, int y_) {
  (void) x_;
  (void) y_;
  return true;
}

void c_button::on_mouse_enter () {
  c_widget::on_mouse_enter ();
}

void c_button::on_mouse_leave () {
  c_widget::on_mouse_leave ();
}

void c_button::clear_hover () {
  on_mouse_leave ();
}

bool c_button::set_value (bool value_) {
  const bool changed = value != value_;
  value = value_;
  if (changed)
    invalidate ();
  return changed;
}

c_checkbox::c_checkbox () {
  is_toggle = true;
  align = TEXT_LEFT;
}

void c_checkbox::draw (cairo_t *cr) {
  if (!cr)
    return;

  const t_statecolors &colors = colors_for (
      WSTYLE_CHECKBOX,
      enabled ? (value ? WSTATE_ON : WSTATE_OFF) : WSTATE_DISABLED);
  const int box = std::max (24, std::min (h - 4, 24));
  const int bx = 2;
  const int by = (h - box) / 2;

  tk_path_rounded_rect (cr, bx, by, box, box, UI_CHECKBOX_RADIUS);
  tk_set_gradient (cr, box, colors.bg);
  cairo_fill_preserve (cr);
  tk_set_gradient (cr, box, colors.fg);
  cairo_set_line_width (cr, hovered ? 2.5 : 2.0);
  cairo_stroke (cr);

  if (value) {
    cairo_save (cr);
    cairo_set_source_rgba (
        cr, colors.fg.r1, colors.fg.g1, colors.fg.b1, colors.fg.a1);
    cairo_set_line_width (cr, std::max (2.0, box * 0.14));
    cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
    cairo_move_to (cr, bx + box * 0.23, by + box * 0.55);
    cairo_line_to (cr, bx + box * 0.43, by + box * 0.74);
    cairo_line_to (cr, bx + box * 0.78, by + box * 0.28);
    cairo_stroke (cr);
    cairo_restore (cr);
  }

  cairo_select_font_face (
    cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size (cr, 13.0 * font_multiplier ());
  cairo_set_source_rgba (
      cr, colors.fg.r1, colors.fg.g1, colors.fg.b1, colors.fg.a1);
  cairo_text_extents_t ext {};
  cairo_text_extents (cr, label.c_str (), &ext);
  const double text_x = tk_aligned_text_x (
      w - (bx + box + 8),
      ext,
      0.0,
      align);
  cairo_move_to (
      cr,
      bx + box + 8 + text_x,
      ((double) h - ext.height) * 0.5 - ext.y_bearing);
  cairo_show_text (cr, label.c_str ());
}

c_scrollbar::c_scrollbar () {
  wants_mouse = true;
  wants_hover = true;
}

bool c_scrollbar::set_value (float value_, bool notify) {
  const float old = value;
  value = std::clamp (value_, 0.0f, 1.0f);
  if (fabsf (value - old) < 0.000001f)
    return false;

  invalidate ();
  if (notify)
    emit_action ();

  return true;
}

void c_scrollbar::set_page_size (float value_) {
  page_size = std::clamp (value_, 0.02f, 1.0f);
  invalidate ();
}

void c_scrollbar::set_step (float value_) {
  step = std::max (0.0f, value_);
}

void c_scrollbar::set_container (c_container *container_) {
  container = container_;
}

void c_scrollbar::set_orientation (_scrollbar_orientation orientation_) {
  orientation = orientation_;
  invalidate ();
}

t_rect c_scrollbar::thumb_rect () const {
  if (orientation == SCROLLBAR_HORIZONTAL) {
    const int track_w = std::max (1, w - 4);
    const int thumb_w = std::clamp (
      (int) std::round (track_w * page_size),
      std::min (track_w, 16),
      track_w);
    const int travel = std::max (0, track_w - thumb_w);
    const int tx = 2 + (int) std::round (travel * value);
    return { tx, 2, thumb_w, std::max (1, h - 4) };
  } else {
    const int track_h = std::max (1, h - 4);
    const int thumb_h = std::clamp (
      (int) std::round (track_h * page_size),
      std::min (track_h, 16),
      track_h);
    const int travel = std::max (0, track_h - thumb_h);
    const int ty = 2 + (int) std::round (travel * value);
    return { 2, ty, std::max (1, w - 4), thumb_h };
  }
}

void c_scrollbar::emit_action () {
  t_action_event event;
  event.source = this;
  event.source_id = id;
  event.mouse_button = last_mouse_button;

  if (container)
    container->on_scrollbar_action (event);
  if (!event.handled)
    on_action (event);
}

void c_scrollbar::draw (cairo_t *cr) {
  if (!cr)
    return;

  const t_statecolors &frame = colors_for (WSTYLE_FRAME, WSTATE_NORMAL);
  const t_statecolors &thumb = colors_for (
    WSTYLE_BUTTON,
    pressed ? WSTATE_DOWN : hovered ? WSTATE_HOVER : WSTATE_NORMAL);
  const t_gradientcolors &bg = get_colortheme ()->window_bg;

  tk_path_rounded_rect (cr, 1, 1, w - 2, h - 2, UI_SCROLLBAR_RADIUS);
  cairo_set_source_rgba (cr, bg.r1, bg.g1, bg.b1, bg.a1);
  cairo_fill_preserve (cr);
  cairo_set_source_rgba (
      cr, frame.fg.r1, frame.fg.g1, frame.fg.b1, frame.fg.a1);
  cairo_set_line_width (cr, 1.0);
  cairo_stroke (cr);

  const t_rect r = thumb_rect ();
  tk_path_rounded_rect (cr, r.x, r.y, r.w, r.h, std::max (0, (int) UI_SCROLLBAR_RADIUS - (r.x - 2)));
  tk_set_gradient (cr, r.h, thumb.bg);
  cairo_fill_preserve (cr);
  tk_set_gradient (cr, r.h, thumb.fg);
  cairo_set_line_width (cr, hovered ? 1.8 : 1.2);
  cairo_stroke (cr);
}

bool c_scrollbar::on_mouse_down (int x_, int y_, int button) {
  c_widget::on_mouse_down (x_, y_, button);
  last_mouse_button = button;

  if (button == Button4)
    return set_value (value - step, true) || true;
  if (button == Button5)
    return set_value (value + step, true) || true;
  if (button != Button1)
    return true;

  if (app)
    app->set_focus (this);

  const t_rect thumb = thumb_rect ();
  if (thumb.contains (x_, y_)) {
    dragging = true;
    drag_start_x = x_;
    drag_start_y = y_;
    drag_start_value = value;
  } else {
    const bool before = orientation == SCROLLBAR_HORIZONTAL ?
      x_ < thumb.x : y_ < thumb.y;
    set_value (value + (before ? -page_size : page_size), true);
  }

  return true;
}

bool c_scrollbar::on_mouse_up (int x_, int y_, int button) {
  if (button == Button4)
    return set_value (value - step, true) || true;
  if (button == Button5)
    return set_value (value + step, true) || true;

  c_widget::on_mouse_up (x_, y_, button);
  (void) x_;
  (void) y_;
  if (button == Button1)
    dragging = false;
  return true;
}

bool c_scrollbar::on_mouse_move (int x_, int y_) {
  if (!dragging)
    return true;

  const t_rect thumb = thumb_rect ();
  if (orientation == SCROLLBAR_HORIZONTAL) {
    const int track_w = std::max (1, w - 4);
    const int travel = std::max (1, track_w - thumb.w);
    set_value (
      drag_start_value + (float) (x_ - drag_start_x) / (float) travel,
      true);
  } else {
    const int track_h = std::max (1, h - 4);
    const int travel = std::max (1, track_h - thumb.h);
    set_value (
      drag_start_value + (float) (y_ - drag_start_y) / (float) travel,
      true);
  }
  return true;
}

void c_scrollbar::on_mouse_leave () {
  if (dragging)
    return;

  c_widget::on_mouse_leave ();
}

void c_container::draw (cairo_t *cr) {
  if (!cr)
    return;

  const t_gradientcolors &bg = get_colortheme ()->window_bg;
  cairo_set_source_rgba (cr, bg.r1, bg.g1, bg.b1, bg.a1);
  cairo_rectangle (cr, 0, 0, w, h);
  cairo_fill (cr);
}

void c_container::set_scrollbar (c_scrollbar *scrollbar_) {
  set_vscrollbar (scrollbar_);
}

void c_container::set_vscrollbar (c_scrollbar *scrollbar_) {
  if (vscrollbar && vscrollbar->container == this)
    vscrollbar->set_container (nullptr);

  vscrollbar = scrollbar_;
  if (vscrollbar) {
    vscrollbar->set_orientation (SCROLLBAR_VERTICAL);
    vscrollbar->set_container (this);
  }

  sync_scrollbar ();
}

void c_container::set_hscrollbar (c_scrollbar *scrollbar_) {
  if (hscrollbar && hscrollbar->container == this)
    hscrollbar->set_container (nullptr);

  hscrollbar = scrollbar_;
  if (hscrollbar) {
    hscrollbar->set_orientation (SCROLLBAR_HORIZONTAL);
    hscrollbar->set_container (this);
  }

  sync_scrollbar ();
}

void c_container::sync_scrollbar () {
  if (vscrollbar) {
    vscrollbar->set_page_size (vscroll_page_size ());
    vscrollbar->set_step (vscroll_step ());
    vscrollbar->set_value (vscroll_value ());
  }

  if (hscrollbar) {
    hscrollbar->set_page_size (hscroll_page_size ());
    hscrollbar->set_step (hscroll_step ());
    hscrollbar->set_value (hscroll_value ());
  }
}

void c_container::on_scrollbar_action (t_action_event &event) {
  if (vscrollbar && event.source_id == vscrollbar->id) {
    set_vscroll_value (vscrollbar->value);
    event.handled = true;
    return;
  }

  if (hscrollbar && event.source_id == hscrollbar->id) {
    set_hscroll_value (hscrollbar->value);
    event.handled = true;
    return;
  }
}

void c_container::set_vscroll_value (float value_) {
  (void) value_;
}

void c_container::set_hscroll_value (float value_) {
  (void) value_;
}

float c_container::vscroll_value () const {
  return 0.0f;
}

float c_container::hscroll_value () const {
  return 0.0f;
}

float c_container::vscroll_page_size () const {
  return 1.0f;
}

float c_container::hscroll_page_size () const {
  return 1.0f;
}

float c_container::vscroll_step () const {
  return 1.0f;
}

float c_container::hscroll_step () const {
  return 1.0f;
}

void c_container::on_action (t_action_event &event) {
  if ((vscrollbar && event.source_id == vscrollbar->id) ||
      (hscrollbar && event.source_id == hscrollbar->id)) {
    on_scrollbar_action (event);
    if (event.handled)
      return;
  }

  c_widget::on_action (event);
}

c_listbox::c_listbox () {
  wants_mouse = true;
  wants_hover = true;
}

void c_listbox::clear () {
  rows.clear ();
  selected = -1;
  first_visible = 0;
  sync_scrollbar ();
  invalidate ();
}

void c_listbox::add (const std::string &text) {
  rows.push_back ({ text, "", false, false });
  sync_scrollbar ();
  invalidate ();
}

void c_listbox::set_items (const std::vector<std::string> &items_) {
  rows.clear ();
  rows.reserve (items_.size ());
  for (const std::string &item : items_)
    rows.push_back ({ item, "", false, false });
  if (selected >= (int) rows.size ())
    selected = -1;
  scroll_to (first_visible);
  sync_scrollbar ();
  invalidate ();
}

void c_listbox::set_rows (const std::vector<t_listrow> &rows_) {
  rows = rows_;
  if (selected >= (int) rows.size ())
    selected = -1;
  scroll_to (first_visible);
  sync_scrollbar ();
  invalidate ();
}

void c_listbox::set_item_flags (
    const std::vector<bool> &directories,
    const std::vector<bool> &symlinks) {

  for (size_t i = 0; i < rows.size (); i++) {
    rows [i].directory = i < directories.size () ? directories [i] : false;
    rows [i].symlink = i < symlinks.size () ? symlinks [i] : false;
  }

  invalidate ();
}

int c_listbox::visible_rows () const {
  return std::max (1, h / std::max (1, row_height));
}

bool c_listbox::scroll_to (int first_row) {
  const int max_first = std::max (0, (int) rows.size () - visible_rows ());
  const int next = std::clamp (first_row, 0, max_first);
  if (first_visible == next)
    return false;

  first_visible = next;
  sync_scrollbar ();
  invalidate ();
  return true;
}

void c_listbox::sync_scrollbar () {
  if (!vscrollbar)
    return;

  vscrollbar->set_page_size (vscroll_page_size ());
  vscrollbar->set_step (vscroll_step ());
  vscrollbar->set_value (vscroll_value ());
}

void c_listbox::set_vscroll_value (float value_) {
  const int max_first = std::max (0, (int) rows.size () - visible_rows ());
  scroll_to ((int) std::round (std::clamp (value_, 0.0f, 1.0f) * max_first));
}

float c_listbox::vscroll_value () const {
  const int max_first = std::max (0, (int) rows.size () - visible_rows ());
  return max_first > 0 ? (float) first_visible / (float) max_first : 0.0f;
}

float c_listbox::vscroll_page_size () const {
  const int total = std::max (1, (int) rows.size ());
  return (float) visible_rows () / (float) total;
}

float c_listbox::vscroll_step () const {
  const int max_first = std::max (0, (int) rows.size () - visible_rows ());
  return max_first > 0 ? 1.0f / (float) max_first : 1.0f;
}

int c_listbox::row_at (int y_) const {
  if (row_height <= 0)
    return -1;

  const int index = first_visible + y_ / row_height;
  if (index < 0 || index >= (int) rows.size ())
    return -1;

  return index;
}

bool c_listbox::set_selected (int index, bool notify) {
  if (index < 0 || index >= (int) rows.size ())
    index = -1;

  if (selected == index)
    return false;

  selected = index;
  bool scrolled = false;
  if (selected >= 0) {
    if (selected < first_visible) {
      first_visible = selected;
      scrolled = true;
    } else if (selected >= first_visible + visible_rows ()) {
      first_visible = selected - visible_rows () + 1;
      scrolled = true;
    }
  }

  if (scrolled)
    sync_scrollbar ();
  invalidate ();
  if (notify)
    on_select (selected);

  return true;
}

void c_listbox::emit_action (bool activated) {
  debug ("activated=%d", (int) activated);
  emit_action (activated, selected);
}

void c_listbox::emit_action (bool activated, int index) {
  debug ("activated=%d, index=%d", (int) activated, index);
  t_action_event event;
  event.source = this;
  event.source_id = id;
  event.source_index = index;
  event.value = activated;
  event.mouse_button = last_mouse_button;
  on_action (event);
}

void c_listbox::on_select (int index) {
  emit_action (false, index);
}

void c_listbox::on_activate (int index) {
  emit_action (true, index);
}

void c_listbox::draw (cairo_t *cr) {
  if (!cr)
    return;

  static cairo_surface_t *dir_icon =
    cairo_image_surface_create_from_stream (data_xputty_directory_png);
  static cairo_surface_t *file_icon =
    cairo_image_surface_create_from_stream (data_xputty_file_png);

  const t_statecolors &frame = colors_for (WSTYLE_FRAME, WSTATE_NORMAL);
  const t_statecolors &sel = colors_for (WSTYLE_BUTTON, WSTATE_ON);
  const t_statecolors &hover = colors_for (WSTYLE_BUTTON, WSTATE_HOVER);
  const t_gradientcolors &bg = get_colortheme ()->window_bg;
  const t_gradientcolors outline = {
    frame.fg.r2, frame.fg.g2, frame.fg.b2, frame.fg.a2,
    frame.fg.r1, frame.fg.g1, frame.fg.b1, frame.fg.a1
  };

  tk_path_rounded_rect (cr, 1, 1, w - 2, h - 2, UI_LIST_RADIUS);
  cairo_set_source_rgba (cr, bg.r1, bg.g1, bg.b1, bg.a1);
  cairo_fill_preserve (cr);
  tk_set_gradient (cr, h, outline);
  cairo_set_line_width (cr, 1.5);
  cairo_stroke (cr);

  cairo_save (cr);
  cairo_rectangle (cr, 2, 2, std::max (0, w - 4), std::max (0, h - 4));
  cairo_clip (cr);

  cairo_select_font_face (
    cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size (cr, size * font_multiplier ());

  const int visible_count = visible_rows ();
  for (int row = 0; row < visible_count; row++) {
    const int index = first_visible + row;
    if (index >= (int) rows.size ())
      break;

    const int y0 = row * row_height;
    if (index == selected) {
      tk_path_rounded_rect (cr, 4, y0 + 2, w - 8, row_height - 4, 4.0);
      tk_set_gradient (cr, row_height, sel.bg);
      cairo_fill (cr);
    } else if (hovered) {
      // Keep hover subtle; row hover can be added later once motion tracks it.
      (void) hover;
    }

    const bool have_icon = rows [index].directory || !rows [index].path.empty ();
    const int icon_size = have_icon ?
      std::max (1, std::min (16, row_height - 6)) : 0;
    const int icon_x = 8;
    const int icon_y = y0 + (row_height - icon_size) / 2;
    const int text_x = have_icon ? icon_x + icon_size + 6 : 8;
    const int text_w = std::max (1, w - text_x - 8);

    if (have_icon) {
      cairo_surface_t *icon =
        rows [index].directory ? dir_icon : file_icon;
      if (icon && cairo_surface_status (icon) == CAIRO_STATUS_SUCCESS)
        tk_draw_surface_scaled (
            cr, icon, icon_x, icon_y, icon_size, icon_size);
    }

    cairo_text_extents_t ext {};
    cairo_text_extents (cr, rows [index].label.c_str (), &ext);
    cairo_set_source_rgba (
      cr, frame.fg.r1, frame.fg.g1, frame.fg.b1, frame.fg.a1);
    cairo_save (cr);
    cairo_rectangle (cr, text_x, y0, text_w, row_height);
    cairo_clip (cr);
    cairo_move_to (
      cr,
      text_x,
      y0 + (row_height - ext.height) * 0.5 - ext.y_bearing);
    cairo_show_text (cr, rows [index].label.c_str ());
    cairo_restore (cr);
  }

  cairo_restore (cr);
}

bool c_listbox::on_mouse_down (int x_, int y_, int button) { CP
  last_mouse_button = button;

  if (button == Button4)
    return scroll_to (first_visible - 1) || true;
  if (button == Button5)
    return scroll_to (first_visible + 1) || true;

  c_widget::on_mouse_down (x_, y_, button);
  if (button != Button1)
    return true;

  if (app)
    app->set_focus (this);

  const int index = row_at (y_);
  const uint64_t now = now_ms ();
  const bool was_selected = index >= 0 && index == selected;
  mouse_down_row = index;
  mouse_activate_pending = false;
  if (index >= 0 && activate_on_single_click) {
    mouse_activate_pending = true;
  } else if (was_selected) {
    const bool double_click = activate_on_doubleclick &&
      now - last_click_ms < UI_DOUBLECLICK_MS;
    mouse_activate_pending = activate_on_click_again || double_click;
  }
  set_selected (index, true);
  last_click_ms = now;

  (void) x_;
  return true;
}

bool c_listbox::on_mouse_up (int x_, int y_, int button) { CP
  last_mouse_button = button;

  if (button == Button4)
    return scroll_to (first_visible - 1) || true;
  if (button == Button5)
    return scroll_to (first_visible + 1) || true;

  c_widget::on_mouse_up (x_, y_, button);
  if (button != Button1)
    return true;

  const bool released_inside =
    x_ >= 0 && y_ >= 0 && x_ < w && y_ < h;
  const bool activate =
    released_inside &&
    mouse_down_row >= 0 &&
    (activate_on_single_click || mouse_activate_pending);
  const int activate_row = mouse_down_row;
  mouse_down_row = -1;
  mouse_activate_pending = false;

  if (activate)
    on_activate (activate_row);

  return true;
}

bool c_listbox::on_key_down (int key) {
  switch (key) {
    case KEY_UP:
      set_selected (selected < 0 ? 0 : selected - 1, true);
      return true;

    case KEY_DOWN:
      set_selected (selected < 0 ? 0 : selected + 1, true);
      return true;

    case KEY_RETURN:
      if (selected >= 0)
        on_activate (selected);
      return true;

    default:
      return false;
  }
}

c_combobox::c_combobox () {
  wants_mouse = true;
  wants_hover = true;
}

c_combobox::~c_combobox () = default;

void c_combobox::create (
    c_widget *parent_,
    const char *label_,
    int x_,
    int y_,
    int w_,
    int h_) {

  c_widget::create (parent_, label_, x_, y_, w_, h_);
  listbox.visible = false;
  listbox.row_height = dropdown_row_height;
  sync_list_geometry ();
}

void c_combobox::draw (cairo_t *cr) {
  if (!cr)
    return;

  const t_statecolors &colors = colors_for (
      WSTYLE_BUTTON,
      pressed ? WSTATE_DOWN : hovered ? WSTATE_HOVER : WSTATE_NORMAL);

  tk_path_rounded_rect (cr, 1, 1, w - 2, h - 2, UI_COMBOBOX_RADIUS);
  tk_set_gradient (cr, h, colors.bg);
  cairo_fill_preserve (cr);
  tk_set_gradient (cr, h, colors.fg);
  cairo_set_line_width (cr, 1.5);
  cairo_stroke (cr);

  const int arrow_w = std::min (24, std::max (16, h));
  const int text_x = 8;
  const int text_w = std::max (1, w - arrow_w - text_x - 6);
  const std::string text = selected_text ().empty () ? label : selected_text ();

  cairo_select_font_face (
    cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size (cr, 13.0 * font_multiplier ());
  cairo_text_extents_t ext {};
  cairo_text_extents (cr, text.c_str (), &ext);
  cairo_set_source_rgba (
      cr, colors.fg.r1, colors.fg.g1, colors.fg.b1, colors.fg.a1);
  cairo_save (cr);
  cairo_rectangle (cr, text_x, 0, text_w, h);
  cairo_clip (cr);
  cairo_move_to (
      cr,
      text_x,
      ((double) h - ext.height) * 0.5 - ext.y_bearing);
  cairo_show_text (cr, text.c_str ());
  cairo_restore (cr);

  const double cx = w - arrow_w * 0.5;
  const double cy = h * 0.5;
  const double s = std::max (4.0, h * 0.18);
  cairo_move_to (cr, cx - s, cy - s * 0.5);
  cairo_line_to (cr, cx + s, cy - s * 0.5);
  cairo_line_to (cr, cx, cy + s * 0.65);
  cairo_close_path (cr);
  cairo_fill (cr);
}

bool c_combobox::on_mouse_down (int x_, int y_, int button) {
  c_widget::on_mouse_down (x_, y_, button);
  toggle_on_mouse_up = button == Button1;

  return true;
}

bool c_combobox::on_mouse_up (int x_, int y_, int button) {
  c_widget::on_mouse_up (x_, y_, button);
  const bool inside = x_ >= 0 && y_ >= 0 && x_ < w && y_ < h;
  if (button == Button1 && toggle_on_mouse_up && inside)
    toggle_list ();

  toggle_on_mouse_up = false;
  return true;
}

bool c_combobox::on_key_down (int key) {
  switch (key) {
    case KEY_RETURN:
      toggle_list ();
      return true;

    case KEY_ESCAPE:
      hide_list ();
      return true;

    case KEY_UP:
      set_selected (selected < 0 ? 0 : selected - 1, true);
      return true;

    case KEY_DOWN:
      if (!list_visible) {
        show_list ();
        return true;
      }
      set_selected (selected < 0 ? 0 : selected + 1, true);
      return true;

    default:
      return false;
  }
}

void c_combobox::on_action (t_action_event &event) {
  if (event.source_id == listbox.id) {
    event.handled = true;
    bool changed = false;
    if (event.source_index >= 0)
      changed = set_selected (event.source_index, event.value);
    if (event.value) {
      if (!changed && selected >= 0)
        on_change (selected);
      hide_list ();
    }
    return;
  }

  c_widget::on_action (event);
}

void c_combobox::clear () {
  items.clear ();
  selected = -1;
  listbox.clear ();
  invalidate ();
}

void c_combobox::add (const std::string &text) {
  items.push_back (text);
  listbox.set_items (items);
  sync_list_geometry ();
  invalidate ();
}

void c_combobox::set_items (const std::vector<std::string> &items_) {
  items = items_;
  if (selected >= (int) items.size ())
    selected = -1;
  listbox.set_items (items);
  listbox.set_selected (selected);
  sync_list_geometry ();
  invalidate ();
}

bool c_combobox::set_selected (int index, bool notify) {
  if (index < 0 || index >= (int) items.size ())
    index = -1;

  if (selected == index)
    return false;

  selected = index;
  listbox.set_selected (selected);
  invalidate ();

  if (notify)
    on_change (selected);

  return true;
}

int c_combobox::get_selection () const {
  return selected;
}

std::string c_combobox::selected_text () const {
  return selected >= 0 && selected < (int) items.size () ? items [selected] : "";
}

void c_combobox::show_list () {
  if (items.empty ())
    return;

  if (!popup && app) {
    popup = app->create_popup (this);
    if (popup) {
      listbox.create (&popup->root, "", 0, 0, w, dropdown_row_height);
      listbox.action_parent = this;
      listbox.row_height = dropdown_row_height;
      listbox.activate_on_single_click = true;
      listbox.activate_on_doubleclick = false;
      listbox.set_vscrollbar (&vscrollbar);

      vscrollbar.create (
          &popup->root, "", w, 0, UI_SCROLLBAR_WIDTH, dropdown_row_height);
      vscrollbar.set_container (&listbox);
      vscrollbar.set_orientation (SCROLLBAR_VERTICAL);
    }
  }
  if (!popup)
    return;

  sync_list_geometry ();
  list_visible = true;
  listbox.visible = true;
  if (selected >= 0)
    listbox.set_selected (selected);

  t_point p = local_to_screen ({ 0, h });
  popup->show_at_screen_pos (p.x, p.y);
  invalidate ();
}

void c_combobox::hide_list () {
  list_visible = false;
  listbox.visible = false;
  if (popup)
    popup->close ();
  invalidate ();
}

void c_combobox::toggle_list () {
  if (list_visible && popup && !popup->visible) {
    list_visible = false;
    listbox.visible = false;
  }

  if (list_visible)
    hide_list ();
  else
    show_list ();
}

void c_combobox::sync_list_geometry () {
  const int rows = std::clamp (
      (int) items.size (),
      1,
      std::max (1, visible_rows_max));
  listbox.row_height = dropdown_row_height;
  const int popup_h = rows * dropdown_row_height;
  const bool needs_scroll = (int) items.size () > rows;
  const int scroll_w = needs_scroll ? UI_SCROLLBAR_WIDTH : 0;
  listbox.move_resize (0, 0, std::max (1, w - scroll_w), popup_h);
  vscrollbar.move_resize (
      std::max (1, w - scroll_w), 0, std::max (1, scroll_w), popup_h);
  vscrollbar.visible = needs_scroll;
  if (popup)
    popup->move_resize (popup->x, popup->y, w, popup_h);
  listbox.sync_scrollbar ();
}

void c_combobox::emit_action () {
  t_action_event event;
  event.source = this;
  event.source_id = id;
  event.source_index = selected;
  event.value = selected >= 0;
  event.mouse_button = last_mouse_button;
  c_widget::on_action (event);
}

void c_combobox::on_change (int index) {
  (void) index;
  emit_action ();
}

void c_combobox::update_widget () {
  listbox.set_items (items);
  listbox.set_selected (selected);
  sync_list_geometry ();
  invalidate ();
}

c_knob::c_knob () {
  wants_mouse = true;
  wants_hover = true;
}

float c_knob::quantize (float value_) const {
  if (step <= 0.0f)
    return value_;

  return min + std::round ((value_ - min) / step) * step;
}

bool c_knob::set_value (float value_, bool notify) {
  const float old = value;
  value = std::clamp (quantize (value_), min, max);
  if (fabsf (value - old) < 0.000001f)
    return false;

  invalidate ();
  if (notify)
    emit_action ();

  return true;
}

void c_knob::set_range (float min_, float max_) {
  min = min_;
  max = max_;
  if (max < min)
    std::swap (min, max);
  set_value (value, false);
}

void c_knob::set_min (float min_) {
  set_range (min_, max);
}

void c_knob::set_max (float max_) {
  set_range (min, max_);
}

void c_knob::set_default (float value_) {
  default_value = value_;
}

void c_knob::set_step (float step_) {
  step = step_;
}

float c_knob::normalized_value () const {
  if (max <= min)
    return 0.0f;

  return std::clamp ((value - min) / (max - min), 0.0f, 1.0f);
}

float c_knob::angle_from_value () const {
  const float start = 3.0f * M_PI / 4.0f;
  const float sweep = 3.0f * M_PI / 2.0f;
  return start + normalized_value () * sweep;
}

void c_knob::emit_action () {
  t_action_event event;
  event.source = this;
  event.source_id = id;
  event.mouse_button = last_mouse_button;
  on_action (event);
}

void c_knob::draw (cairo_t *cr) {
  if (!cr)
    return;

  const t_statecolors &colors = colors_for (
    WSTYLE_BUTTON,
    pressed ? WSTATE_DOWN : hovered ? WSTATE_HOVER : WSTATE_NORMAL);

  const int label_h = label.empty () ? 0 :
    std::max (14, (int) (size * font_multiplier () + 4));
  const int knob_h = std::max (1, h - label_h);
  const double cx = w * 0.5;
  const double cy = knob_h * 0.5;
  const double knob_size = std::max (1.0, (double) std::min (w, knob_h) - 2.0);
  const double knob_x = cx - knob_size * 0.5;
  const double knob_y = cy - knob_size * 0.5;
  const double radius = std::max (4.0, knob_size * 0.5 - 5.0);
  const double indicator_radius = std::max (3.0, knob_size * 0.45);

  cairo_save (cr);
  if (!draw_nbtk_knob_image (cr, knob_x, knob_y, knob_size)) {
    cairo_arc (cr, cx, cy, radius, 0.0, 2.0 * M_PI);
    tk_set_gradient (cr, radius * 2.0, colors.bg);
    cairo_fill_preserve (cr);

    tk_set_gradient (cr, radius * 2.0, colors.fg);
    cairo_set_line_width (cr, hovered ? 2.4 : 1.8);
    cairo_stroke (cr);
  }

  const double a0 = 3.0 * M_PI / 4.0;
  const double a1 = angle_from_value ();
  if (hovered || dragging)
    cairo_set_source_rgba (cr, 0.80, 0.96, 0.94, 0.95);
  else
    cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.95);
  cairo_set_line_width (cr, std::max (2.0, indicator_radius * 0.08));
  cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
  cairo_arc (cr, cx, cy, indicator_radius, a0, a1);
  cairo_stroke (cr);

  const double dot_dist = indicator_radius * 0.68;
  const double dot_r = std::max (2.0, indicator_radius * 0.10);
  cairo_arc (
    cr,
    cx + cos (a1) * dot_dist,
    cy + sin (a1) * dot_dist,
    dot_r,
    0.0,
    2.0 * M_PI);
  cairo_fill (cr);

  cairo_select_font_face (
    cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size (cr, size * font_multiplier ());
  cairo_set_source_rgb (cr, 0.92, 0.92, 0.92);

  char text [64] = {};
  if (show_value) {
    if (fabsf (step) >= 0.99f)
      snprintf (text, sizeof (text), "%d", (int) std::round (value));
    else if (fabsf (step) >= 0.09f)
      snprintf (text, sizeof (text), "%.1f", value);
    else
      snprintf (text, sizeof (text), "%.2f", value);

    cairo_text_extents_t ext {};
    cairo_text_extents (cr, text, &ext);
    cairo_move_to (
      cr,
      cx - ext.width * 0.5 - ext.x_bearing,
      cy - ext.height * 0.5 - ext.y_bearing);
    cairo_show_text (cr, text);
  }

  if (!label.empty ()) {
    cairo_text_extents_t ext {};
    cairo_text_extents (cr, label.c_str (), &ext);
    cairo_move_to (
      cr,
      cx - ext.width * 0.5 - ext.x_bearing,
      h - label_h * 0.5 + ext.height * 0.5);
    cairo_show_text (cr, label.c_str ());
  }

  cairo_restore (cr);
}

bool c_knob::on_mouse_down (int x_, int y_, int button) {
  if (button == Button4 || button == Button5) {
    last_mouse_button = button;
    return true;
  }

  c_widget::on_mouse_down (x_, y_, button);
  last_mouse_button = button;

  if (button != Button1)
    return true;

  const uint64_t now = now_ms ();
  if (reset_on_doubleclick && now - last_click_ms < 350) {
    last_click_ms = 0;
    set_value (default_value, true);
    return true;
  }
  last_click_ms = now;

  if (app)
    app->set_focus (this);

  dragging = true;
  drag_start_y = y_;
  drag_start_value = value;
  return true;
}

bool c_knob::on_mouse_up (int x_, int y_, int button) {
  if (button == Button4 || button == Button5) {
    last_mouse_button = button;
    const float dir = button == Button4 ? 1.0f : -1.0f;
    set_value (value + step * dir, true);
    return true;
  }

  c_widget::on_mouse_up (x_, y_, button);
  (void) x_;
  (void) y_;
  if (button == Button1)
    dragging = false;
  return true;
}

bool c_knob::on_mouse_move (int x_, int y_) {
  (void) x_;
  if (!dragging)
    return true;

  const float range = max - min;
  if (range <= 0.0f)
    return true;

  const float delta = (float) (drag_start_y - y_) * drag_sensitivity * range;
  set_value (drag_start_value + delta, true);
  return true;
}

void c_knob::on_mouse_leave () {
  if (dragging)
    return;

  c_widget::on_mouse_leave ();
}

static size_t tk_utf8_prev_pos (const std::string &s, size_t pos) {
  pos = std::min (pos, s.size ());
  if (pos == 0)
    return 0;

  --pos;
  while (pos > 0 && ((unsigned char) s [pos] & 0xC0) == 0x80)
    --pos;

  return pos;
}

static size_t tk_utf8_next_pos (const std::string &s, size_t pos) {
  pos = std::min (pos, s.size ());
  if (pos >= s.size ())
    return s.size ();

  ++pos;
  while (pos < s.size () && ((unsigned char) s [pos] & 0xC0) == 0x80)
    ++pos;

  return pos;
}

c_textbox::c_textbox () {
  wants_mouse = true;
  wants_hover = true;
  align = TEXT_LEFT;
}

void c_textbox::draw (cairo_t *cr) {
  if (!cr)
    return;

  const bool focused = app && app->focused_widget == this;
  const t_statecolors &normal_colors = colors_for (WSTYLE_BUTTON, WSTATE_NORMAL);
  const t_statecolors &textbox_colors = colors_for (WSTYLE_FRAME, WSTATE_NORMAL);
  const t_statecolors &focus_colors = colors_for (WSTYLE_FRAME, WSTATE_SELECTED);

  tk_path_rounded_rect (cr, 1, 1, w - 2, h - 2, UI_TEXTBOX_RADIUS);
  tk_set_gradient (cr, h, textbox_colors.bg);
  cairo_fill_preserve (cr);

  tk_set_gradient (cr, h, focused ? focus_colors.fg : normal_colors.fg);
  cairo_set_line_width (cr, focused ? 2.5 : 2.0);
  cairo_stroke (cr);

  cairo_save (cr);
  const double pad = 8.0 * font_multiplier ();
  cairo_rectangle (
      cr,
      pad,
      2.0,
      std::max (0.0, (double) w - pad * 2.0),
      std::max (0.0, (double) h - 4.0));
  cairo_clip (cr);

  cairo_select_font_face (
    cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size (cr, size * font_multiplier ());
  cairo_set_source_rgba (
      cr,
      normal_colors.fg.r1,
      normal_colors.fg.g1,
      normal_colors.fg.b1,
      normal_colors.fg.a1);

  cairo_text_extents_t extents {};
  cairo_text_extents (cr, value.c_str (), &extents);
  const double tx = tk_aligned_text_x (
      w, extents, pad, focused ? TEXT_LEFT : align);
  const double ty = ((double) h - extents.height) * 0.5 - extents.y_bearing;

  cairo_move_to (cr, tx, ty);
  cairo_show_text (cr, value.c_str ());

  if (focused) {
    cursor = std::min (cursor, value.size ());
    const std::string before_cursor = value.substr (0, cursor);
    cairo_text_extents_t cursor_extents {};
    cairo_text_extents (cr, before_cursor.c_str (), &cursor_extents);
    const double cx = tx + cursor_extents.x_advance + 1.0;

    cairo_set_line_width (cr, 1.0);
    cairo_move_to (cr, cx, 8.0);
    cairo_line_to (cr, cx, h - 8.0);
    cairo_stroke (cr);
  }

  cairo_restore (cr);
}

bool c_textbox::on_mouse_down (int x_, int y_, int button) {
  c_widget::on_mouse_down (x_, y_, button);
  if (button != Button1)
    return true;

  if (app)
    app->set_focus (this);

  cursor = value.size ();
  invalidate ();
  return true;
}

bool c_textbox::on_key_down (int key) {
  bool changed = false;

  switch (key) {
    case KEY_RETURN:
      emit_action ();
      if (app)
        app->clear_focus (this);
      return true;

    case KEY_ESCAPE:
      emit_action ();
      if (app)
        app->clear_focus (this);
      return true;

    case KEY_BACKSPACE: {
      const size_t prev = tk_utf8_prev_pos (value, cursor);
      if (prev < cursor) {
        value.erase (prev, cursor - prev);
        cursor = prev;
        changed = true;
      }
    } break;

    case KEY_DELETE: {
      const size_t next = tk_utf8_next_pos (value, cursor);
      if (next > cursor) {
        value.erase (cursor, next - cursor);
        changed = true;
      }
    } break;

    case KEY_LEFT:
      cursor = tk_utf8_prev_pos (value, cursor);
      invalidate ();
      return true;

    case KEY_RIGHT:
      cursor = tk_utf8_next_pos (value, cursor);
      invalidate ();
      return true;

    case KEY_HOME:
      cursor = 0;
      invalidate ();
      return true;

    case KEY_END:
      cursor = value.size ();
      invalidate ();
      return true;

    default:
      return false;
  }

  if (changed) {
    label = value;
    invalidate ();
  }

  return true;
}

bool c_textbox::on_text_input (const char *text) {
  if (!text || !text [0])
    return false;

  cursor = std::min (cursor, value.size ());
  value.insert (cursor, text);
  cursor += strlen (text);
  label = value;
  invalidate ();
  return true;
}

bool c_textbox::set_text (const char *text_) {
  const std::string next = text_ ? text_ : "";
  if (value == next)
    return false;

  value = next;
  label = value;
  cursor = value.size ();
  invalidate ();
  return true;
}

const std::string &c_textbox::text () const {
  return value;
}

void c_textbox::emit_action () {
  t_action_event event;
  event.source = this;
  event.source_id = id;
  event.mouse_button = last_mouse_button;
  on_action (event);
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
    case WSTATE_DEFAULT:
      if (img_default_source == pngdata)
        return;
      csp = &img_default;
    break;
    case WSTATE_ALL:
      set_image (pngdata, WSTATE_OFF);
      set_image (pngdata, WSTATE_ON);
      set_image (pngdata, WSTATE_HOVER);
      set_image (pngdata, WSTATE_DOWN);
      set_image (pngdata, WSTATE_DOWN_HOVER);
      set_image (pngdata, WSTATE_OFF_HOVER);
      set_image (pngdata, WSTATE_DEFAULT);
      return;
    default:
      return;
  }

  if (*csp) {
    cairo_surface_destroy (*csp);
    *csp = nullptr;
  }

  if (!pngdata) {
    if (which == WSTATE_DEFAULT)
      img_default_source = nullptr;
    invalidate ();
    return;
  }

  *csp = cairo_image_surface_create_from_stream (pngdata);
  if (cairo_surface_status (*csp) != CAIRO_STATUS_SUCCESS) {
    cairo_surface_destroy (*csp);
    *csp = nullptr;
    if (which == WSTATE_DEFAULT)
      img_default_source = nullptr;
    invalidate ();
    return;
  }

  if (which == WSTATE_DEFAULT)
    img_default_source = pngdata;
  invalidate ();
}

c_staticimage::~c_staticimage () {
  if (surface)
    cairo_surface_destroy (surface);
}

void c_staticimage::set_png (const unsigned char *png) {
  if (surface) {
    cairo_surface_destroy (surface);
    surface = nullptr;
  }

  if (png)
    surface = cairo_image_surface_create_from_stream (png);
}

void c_staticimage::draw (cairo_t *cr) {
  if (!cr || !surface)
    return;

  const int iw = cairo_image_surface_get_width (surface);
  const int ih = cairo_image_surface_get_height (surface);
  if (iw <= 0 || ih <= 0)
    return;

  tk_draw_surface_scaled (cr, surface, 0, 0, w, h);
}

static int tk_canvas_button_index (int button) {
  switch (button) {
    case Button1:
      return 0;
    case Button2:
      return 1;
    case Button3:
      return 2;
    default:
      return -1;
  }
}

c_canvas::c_canvas () {
  wants_mouse = true;
  wants_hover = true;
}

void c_canvas::draw (cairo_t *cr) {
  if (!cr)
    return;

  render_base (cr);
  on_paint (cr);
  render_overlay (cr);
  base_image_valid = true;
}

bool c_canvas::on_mouse_down (int x_, int y_, int button) {
  mouse_x = x_;
  mouse_y = y_;
  last_mouse_button = button;

  if (button == Button4) {
    on_mousewheel_v (1);
    return true;
  }
  if (button == Button5) {
    on_mousewheel_v (-1);
    return true;
  }
  if (button == 6) {
    on_mousewheel_h (1);
    return true;
  }
  if (button == 7) {
    on_mousewheel_h (-1);
    return true;
  }

  const int idx = tk_canvas_button_index (button);
  if (idx >= 0) {
    mousedown_x [idx] = mouse_x;
    mousedown_y [idx] = mouse_y;
    mouse_buttons |= (1 << idx);
    on_mousedown (idx);
  }

  c_widget::on_mouse_down (x_, y_, button);

  if (button == Button1)
    on_mousedown_left ();
  else if (button == Button2)
    on_mousedown_middle ();
  else if (button == Button3)
    on_mousedown_right ();

  return true;
}

bool c_canvas::on_mouse_up (int x_, int y_, int button) {
  mouse_x = x_;
  mouse_y = y_;
  last_mouse_button = button;

  const int idx = tk_canvas_button_index (button);
  if (idx >= 0) {
    mouse_buttons &= ~(1 << idx);
    on_mouseup (idx);
  }

  c_widget::on_mouse_up (x_, y_, button);

  if (button == Button1)
    on_mouseup_left ();
  else if (button == Button2)
    on_mouseup_middle ();
  else if (button == Button3)
    on_mouseup_right ();

  return true;
}

bool c_canvas::on_mouse_move (int x_, int y_) {
  mouse_x = x_;
  mouse_y = y_;
  on_mousemove (mouse_x, mouse_y);
  return wants_mouse || wants_hover;
}

void c_canvas::on_mouse_leave () {
  mouse_x = -1;
  mouse_y = -1;
  on_mouseleave ();
  c_widget::on_mouse_leave ();
}

bool c_canvas::on_key_down (int key) {
  on_keydown (key, false);
  return true;
}

bool c_canvas::on_key_up (int key) {
  on_keyup (key);
  return true;
}

void c_canvas::move_resize (int x_, int y_, int w_, int h_) {
  const int old_w = w;
  const int old_h = h;
  c_widget::move_resize (x_, y_, w_, h_);
  if (w != old_w || h != old_h) {
    base_image_valid = false;
    on_resize (w, h);
  }
}

void c_canvas::resize (int w_, int h_) {
  const int old_w = w;
  const int old_h = h;
  c_widget::resize (w_, h_);
  if (w != old_w || h != old_h) {
    base_image_valid = false;
    on_resize (w, h);
  }
}

void c_canvas::invalidate_base () {
  base_image_valid = false;
  invalidate ();
}

void c_canvas::invalidate_overlay () {
  invalidate ();
}

void c_canvas::invalidate_overlay_rect (int x_, int y_, int w_, int h_) {
  invalidate_rect (x_, y_, w_, h_);
}

void c_canvas::expose () {
  invalidate ();
}

bool c_canvas::button_left_down () const {
  return mouse_buttons & 0x01;
}

bool c_canvas::button_middle_down () const {
  return mouse_buttons & 0x02;
}

bool c_canvas::button_right_down () const {
  return mouse_buttons & 0x04;
}

bool c_canvas::check_click_distance (int which) const {
  if (which < 0 || which >= 8)
    return false;

  return std::abs (mouse_x - mousedown_x [which]) <= click_distance &&
         std::abs (mouse_y - mousedown_y [which]) <= click_distance;
}

void c_canvas::render_base (cairo_t *cr) {
  (void) cr;
}

void c_canvas::render_overlay (cairo_t *cr) {
  (void) cr;
}

void c_canvas::on_paint (cairo_t *cr) {
  (void) cr;
}

void c_canvas::on_resize (int w_, int h_) {
  (void) w_;
  (void) h_;
}

void c_canvas::on_mousemove (int x_, int y_) {
  (void) x_;
  (void) y_;
}

void c_canvas::on_mousedown (int which) {
  (void) which;
}

void c_canvas::on_mouseup (int which) {
  (void) which;
}

void c_canvas::on_mousedown_left () {
}

void c_canvas::on_mouseup_left () {
}

void c_canvas::on_mousedown_middle () {
}

void c_canvas::on_mouseup_middle () {
}

void c_canvas::on_mousedown_right () {
}

void c_canvas::on_mouseup_right () {
}

void c_canvas::on_mouseleave () {
}

void c_canvas::on_mousewheel_v (int howmuch) {
  (void) howmuch;
}

void c_canvas::on_mousewheel_h (int howmuch) {
  (void) howmuch;
}

void c_canvas::on_keydown (int keycode, bool is_repeat) {
  (void) keycode;
  (void) is_repeat;
}

void c_canvas::on_keyup (int keycode) {
  (void) keycode;
}

void c_canvas::on_visible () {
}

t_point c_widget::local_to_root (t_point p) const {
  p.x += x;
  p.y += y;

  const c_widget *node = parent;
  while (node) {
    p.x += node->x;
    p.y += node->y;
    node = node->parent;
  }

  return p;
}

t_point c_widget::root_to_local (t_point p) const {
  p.x -= x;
  p.y -= y;

  const c_widget *node = parent;
  while (node) {
    p.x -= node->x;
    p.y -= node->y;
    node = node->parent;
  }

  return p;
}

t_point c_widget::local_to_screen (t_point p) const {
  p = local_to_root (p);
  return app ? app->root_to_screen (p) : p;
}

t_rect c_widget::rect () const {
  return { x, y, w, h };
}

bool c_widget::contains_local (int px, int py) const {
  return px >= x && py >= y && px < x + w && py < y + h;
}

float c_widget::font_multiplier () const {
  return text_size * (app ? app->font_scale : 1.0f);
}

void c_widget::show () {
  if (visible)
    return;

  visible = true;
  invalidate ();
}

void c_widget::hide () {
  if (!visible)
    return;

  visible = false;
  invalidate ();
}

void c_widget::move (int x_, int y_) {
  if (x == x_ && y == y_)
    return;

  invalidate ();
  x = x_;
  y = y_;
  invalidate ();
}

void c_widget::resize (int w_, int h_) {
  w_ = std::max (1, w_);
  h_ = std::max (1, h_);
  if (w == w_ && h == h_)
    return;

  invalidate ();
  w = w_;
  h = h_;
  invalidate ();
}

void c_widget::move_resize (int x_, int y_, int w_, int h_) {
  w_ = std::max (1, w_);
  h_ = std::max (1, h_);
  if (x == x_ && y == y_ && w == w_ && h == h_)
    return;

  invalidate ();
  x = x_;
  y = y_;
  w = w_;
  h = h_;
  invalidate ();
}

void c_widget::invalidate () {
  invalidate_rect (0, 0, w, h);
}

void c_widget::invalidate_rect (int x_, int y_, int w_, int h_) {
  if (!app)
    return;

  t_point p = local_to_root ({ x_, y_ });
  app->invalidate_rect (p.x, p.y, w_, h_);
}

void c_app::create (int w, int h) {
  create_root<c_widget> (w, h);
}

void c_app::draw () {
  if (!cr || !root)
    return;

  cairo_save (cr);
  root->draw_tree (cr);
  cairo_restore (cr);
}

void c_app::dispatch_mouse_down (int x, int y, int button) {
  for (auto it = popups.rbegin (); it != popups.rend (); ++it) {
    c_popupwindow *popup = it->get ();
    if (!popup || !popup->visible)
      continue;

    t_point local = popup->screen_to_local (root_to_screen ({ x, y }));
    if (popup->root.mouse_down_tree (local.x, local.y, button))
      return;

    if (popup->close_on_outside_click ())
      popup->close ();
  }

  if (root)
    root->mouse_down_tree (x, y, button);
}

void c_app::dispatch_mouse_up (int x, int y, int button) {
  if (root)
    root->mouse_up_tree (x, y, button);
}

void c_app::dispatch_mouse_move (int x, int y) {
  if (root)
    root->mouse_move_tree (x, y);
}

void c_app::dispatch_key_down (int key) {
  if (focused_widget)
    focused_widget->on_key_down (key);
}

void c_app::dispatch_key_up (int key) {
  if (focused_widget)
    focused_widget->on_key_up (key);
}

void c_app::dispatch_text_input (const char *text) {
  if (focused_widget)
    focused_widget->on_text_input (text);
}

void c_app::invalidate_rect (int x, int y, int w, int h) {
  if (main_window)
    main_window->invalidate_rect (x, y, w, h);
}

void c_app::set_mouse_cursor (_mouse_cursor cursor) {
  (void) cursor;
}

void c_app::set_focus (c_widget *widget) {
  if (focused_widget == widget)
    return;

  c_widget *old = focused_widget;
  focused_widget = widget;

  if (old)
    old->invalidate ();
  if (focused_widget)
    focused_widget->invalidate ();
}

void c_app::clear_focus (c_widget *widget) {
  if (widget && focused_widget != widget)
    return;

  c_widget *old = focused_widget;
  focused_widget = nullptr;
  if (old)
    old->invalidate ();
}

void c_app::on_event (t_event &event) {
  (void) event;
}

void c_app::on_action (t_action_event &event) {
  on_event (event);
}

void c_app::on_command (t_command_event &event) {
  on_action (event);
}

std::unique_ptr<c_popupwindow> c_app::create_popup (c_widget *owner) {
  std::unique_ptr<c_popupwindow> popup = std::make_unique<c_popupwindow> ();
  popup->create_for_owner (this, owner, 1, 1);
  return popup;
}

t_point c_app::root_to_screen (t_point p) const {
  return main_window ? main_window->local_to_screen (p) : p;
}

t_point c_app::screen_to_root (t_point p) const {
  return main_window ? main_window->screen_to_local (p) : p;
}

void c_nativewindow::create (c_app *app_, int x_, int y_, int w_, int h_) {
  app = app_;
  x = x_;
  y = y_;
  w = w_;
  h = h_;

  root.app = app;
  root.parent = nullptr;
  root.x = 0;
  root.y = 0;
  root.w = w;
  root.h = h;
}

void c_nativewindow::show () {
  visible = true;
}

void c_nativewindow::hide () {
  visible = false;
}

void c_nativewindow::move_resize (int x_, int y_, int w_, int h_) {
  x = x_;
  y = y_;
  w = w_;
  h = h_;
  root.w = w;
  root.h = h;
}

void c_nativewindow::invalidate_rect (int x_, int y_, int w_, int h_) {
  (void) x_;
  (void) y_;
  (void) w_;
  (void) h_;
}

cairo_t *c_nativewindow::begin_paint () {
  return app ? app->cr : nullptr;
}

void c_nativewindow::end_paint () {
}

void c_nativewindow::on_paint (cairo_t *cr_) {
  root.draw_tree (cr_);
}

void c_nativewindow::on_close () {
  hide ();
}

bool c_nativewindow::on_key_down (int key) {
  return root.on_key_down (key);
}

bool c_nativewindow::on_key_up (int key) {
  return root.on_key_up (key);
}

bool c_nativewindow::on_mouse_down (int x_, int y_, int button) {
  return root.mouse_down_tree (x_, y_, button);
}

bool c_nativewindow::on_mouse_up (int x_, int y_, int button) {
  return root.mouse_up_tree (x_, y_, button);
}

bool c_nativewindow::on_mouse_move (int x_, int y_) {
  return root.mouse_move_tree (x_, y_);
}

void c_nativewindow::on_action (t_action_event &event) {
  if (app && !event.handled)
    app->on_action (event);
}

t_point c_nativewindow::local_to_screen (t_point p) const {
  p.x += x;
  p.y += y;
  return p;
}

t_point c_nativewindow::screen_to_local (t_point p) const {
  p.x -= x;
  p.y -= y;
  return p;
}

void c_toplevelwindow::on_close () {
  hide ();
}

void c_embeddedwindow::create_for_parent (
    c_app *app_,
    void *native_parent_,
    int w_,
    int h_) {

  native_parent = native_parent_;
  create (app_, 0, 0, w_, h_);
}

void c_popupwindow::create_for_owner (
    c_app *app_,
    c_widget *owner_,
    int w_,
    int h_) {

  owner = owner_;
  create (app_, 0, 0, w_, h_);
}

void c_popupwindow::create_native_for_owner (
    c_app *app_,
    c_widget *owner_,
    t_native_handle native_owner_,
    int w_,
    int h_) {

  create_for_owner (app_, owner_, w_, h_);
  Widget_t *native_owner = as_xputty_widget (native_owner_);
  if (!native_owner || !native_owner->app)
    return;

  widget = create_window (
      native_owner->app,
      os_get_root_window (native_owner->app, IS_WIDGET),
      0,
      0,
      w_,
      h_);
  if (!widget)
    return;

  Widget_t *w = as_xputty_widget (widget);
  w->parent_struct = this;
  w->scale.gravity = NONE;
  os_set_window_attrb (w);
  os_set_transient_for_hint (native_owner, w);
  if (app && app->backend)
    app->backend->set_window_background (widget, get_colortheme ()->window_bg);
  w->func.expose_callback = c_popupwindow::cb_expose;
  w->func.button_press_callback = c_popupwindow::cb_button_press;
  w->func.button_release_callback = c_popupwindow::cb_button_release;
  w->func.double_click_callback = c_popupwindow::cb_button_release;
  w->func.motion_callback = c_popupwindow::cb_motion;
  os_set_input_mask (w);
  childlist_add_child (native_owner->childlist, w);
}

void c_popupwindow::show_at_screen_pos (int x_, int y_) {
  move_resize (x_, y_, w, h);
  show ();
}

void c_popupwindow::close () {
  hide ();
}

bool c_popupwindow::close_on_outside_click () const {
  return true;
}

bool c_popupwindow::takes_focus () const {
  return true;
}

void c_popupwindow::on_action (t_action_event &event) {
  if (owner && !event.handled)
    owner->on_action (event);

  if (!event.handled)
    c_nativewindow::on_action (event);
}

void c_popupwindow::invalidate_rect (int x_, int y_, int w_, int h_) {
  (void) x_;
  (void) y_;
  (void) w_;
  (void) h_;
  if (app && app->backend)
    app->backend->invalidate (widget);
}

void c_popupwindow::move_resize (int x_, int y_, int w_, int h_) {
  c_nativewindow::move_resize (x_, y_, w_, h_);
  if (app && app->backend)
    app->backend->move_resize (widget, x_, y_, w_, h_);
}

void c_popupwindow::show () {
  c_nativewindow::show ();
  if (widget) {
    Widget_t *w = as_xputty_widget (widget);
    widget_show_all (w);
    if (app && app->backend)
      pointer_grabbed = app->backend->grab_pointer (widget);
    close_on_release = false;
    if (app && app->backend)
      app->backend->invalidate (widget);
  }
}

void c_popupwindow::hide () {
  c_nativewindow::hide ();
  if (widget) {
    Widget_t *w = as_xputty_widget (widget);
    if (pointer_grabbed && app && app->backend) {
      app->backend->ungrab_pointer (widget);
      pointer_grabbed = false;
    }
    close_on_release = false;
    widget_hide (w);
  }
}

void c_popupwindow::cb_expose (void *w_, void *user_data) {
  (void) user_data;
  Widget_t *w = (Widget_t *) w_;
  if (!w || !w->parent_struct)
    return;

  c_popupwindow *self = (c_popupwindow *) w->parent_struct;
  if (!self->app || !w->crb)
    return;

  self->app->cr = w->crb;
  self->root.draw_tree (w->crb);
  self->app->cr = nullptr;
}

void c_popupwindow::cb_button_press (
    void *w_,
    void *event,
    void *user_data) {

  (void) user_data;
  Widget_t *w = (Widget_t *) w_;
  XButtonEvent *button = (XButtonEvent *) event;
  if (!w || !w->parent_struct || !button)
    return;

  c_popupwindow *self = (c_popupwindow *) w->parent_struct;
  const int x = button->x / w->app->hdpi;
  const int y = button->y / w->app->hdpi;
  if (self->close_on_outside_click () &&
      (x < 0 || y < 0 || x >= self->w || y >= self->h)) {
    self->close_on_release = true;
    return;
  }

  const bool handled = self->on_mouse_down (
      x,
      y,
      button->button);
  self->mouse_captured = false;
  if (handled && self->app && self->app->focused_widget) {
    t_point local = self->app->focused_widget->root_to_local ({ x, y });
    self->mouse_captured =
      local.x >= 0 &&
      local.y >= 0 &&
      local.x < self->app->focused_widget->w &&
      local.y < self->app->focused_widget->h;
  }
  expose_widget (w);
}

void c_popupwindow::cb_button_release (
    void *w_,
    void *event,
    void *user_data) {

  (void) user_data;
  Widget_t *w = (Widget_t *) w_;
  XButtonEvent *button = (XButtonEvent *) event;
  if (!w || !w->parent_struct || !button)
    return;

  c_popupwindow *self = (c_popupwindow *) w->parent_struct;
  const int x = button->x / w->app->hdpi;
  const int y = button->y / w->app->hdpi;

  if (self->mouse_captured && self->app && self->app->focused_widget) {
    t_point local = self->app->focused_widget->root_to_local ({ x, y });
    self->app->focused_widget->on_mouse_up (
        local.x, local.y, button->button);
    self->mouse_captured = false;
    expose_widget (w);
    return;
  }

  if (self->close_on_release ||
      (self->close_on_outside_click () &&
       (x < 0 || y < 0 || x >= self->w || y >= self->h))) {
    self->close ();
    return;
  }

  self->on_mouse_up (
      x,
      y,
      button->button);
  self->mouse_captured = false;
  expose_widget (w);
}

void c_popupwindow::cb_motion (
    void *w_,
    void *event,
    void *user_data) {

  (void) user_data;
  Widget_t *w = (Widget_t *) w_;
  XMotionEvent *motion = (XMotionEvent *) event;
  if (!w || !w->parent_struct || !motion)
    return;

  c_popupwindow *self = (c_popupwindow *) w->parent_struct;
  const int x = motion->x / w->app->hdpi;
  const int y = motion->y / w->app->hdpi;
  if (!self->mouse_captured &&
      (x < 0 || y < 0 || x >= self->w || y >= self->h))
    return;

  if (self->mouse_captured && self->app && self->app->focused_widget) {
    t_point local = self->app->focused_widget->root_to_local ({ x, y });
    self->app->focused_widget->on_mouse_move (local.x, local.y);
  } else {
    self->on_mouse_move (
        x,
        y);
  }
  expose_widget (w);
}

bool c_menu::close_on_outside_click () const {
  return true;
}

bool c_menu::takes_focus () const {
  return true;
}

bool c_menu::on_key_down (int key) {
  constexpr int key_escape = 27;
  if (key == key_escape) {
    close ();
    return true;
  }

  return c_popupwindow::on_key_down (key);
}

bool c_menu::on_mouse_down (int x_, int y_, int button) {
  if (!root.rect ().contains (x_, y_)) {
    close ();
    return true;
  }

  return c_popupwindow::on_mouse_down (x_, y_, button);
}

bool c_tooltip::close_on_outside_click () const {
  return false;
}

bool c_tooltip::takes_focus () const {
  return false;
}

void c_tooltip::set_text (const char *text) {
  root.label = text ? text : "";
  root.invalidate ();
}

} // namespace nbtk

static const char *cwd_config_key_for_bank (uint64_t bank) {
  return bank == BANK_CAB ? CONFIG_KEY_NAME_IR_CWD : CONFIG_KEY_NAME_MODEL_CWD;
}

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

static bool key_is_decimal_point (XKeyEvent *key) {
  if (!key)
    return false;

  const KeySym sym0 = XLookupKeysym (key, 0);
  const KeySym sym1 = XLookupKeysym (key, 1);

  return sym0 == XK_period || sym1 == XK_period ||
         sym0 == XK_KP_Decimal || sym1 == XK_KP_Decimal;
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

void tk_path_rounded_rect (
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

void tk_set_gradient (
    cairo_t *cr,
    double h,
    const t_gradientcolors &colors) {

  if (!cr)
    return;

  const bool solid =
    fabsf (colors.r1 - colors.r2) < 0.000001f &&
    fabsf (colors.g1 - colors.g2) < 0.000001f &&
    fabsf (colors.b1 - colors.b2) < 0.000001f &&
    fabsf (colors.a1 - colors.a2) < 0.000001f;
  if (solid) {
    cairo_set_source_rgba (cr, colors.r1, colors.g1, colors.b1, colors.a1);
    return;
  }

  cairo_pattern_t *pat = cairo_pattern_create_linear (0, 0, 0, h);
  cairo_pattern_add_color_stop_rgba (
    pat, 0.0, colors.r1, colors.g1, colors.b1, colors.a1);
  cairo_pattern_add_color_stop_rgba (
    pat, 1.0, colors.r2, colors.g2, colors.b2, colors.a2);
  cairo_set_source (cr, pat);
  cairo_pattern_destroy (pat);
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
  tk_path_rounded_rect (w->crb, x, y, width, height, radius);
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
  tk_path_rounded_rect (
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

void disable_x11_window_background (Widget_t *w) {
  if (!w || !w->app || !w->app->dpy || !w->widget)
    return;

  XSetWindowBackgroundPixmap (w->app->dpy, w->widget, None);
}

void cb_dummy (void *w_, void* user_data) {}

////////////////////////////////////////////////////////////////////////////////
// c_widget

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
  set_x11_window_background (widget, bg_colors ? *bg_colors : g_colors->window_bg);
  widget->flags |= USE_TRANSPARENCY /*| DONT_PROPAGATE*/;

  // no callback func because it would overwrite the ones from
  // child widget classes if they call this func. after their own

  widget->scale.gravity = NONE;
  widget->parent_struct = this;
  widget->label = label.c_str ();
  
  created = true;
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

void c_widget::show () {
  if (widget) widget_show (widget);
}

void c_widget::hide () {
  if (widget) widget_hide (widget);
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

void c_widget::on_focus_lost () {
}

void c_widget::set_state (_widget_state state_) {
  wstate = state_;
  expose ();
}

void c_widget::expose () {
  if (widget)
    expose_widget (widget);
}

const t_gradientcolors *c_widget::bg_override_for_state (
    _widget_state state) const {

  switch (state) {
    case WSTATE_SELECTED:
    case WSTATE_HOVER:
      return highlight_bg_colors;

    case WSTATE_DISABLED:
    case WSTATE_OFF:
      return disabled_bg_colors;

    case WSTATE_NORMAL:
    case WSTATE_ON:
    default:
      return active_bg_colors;
  }
}

const t_gradientcolors *c_widget::fg_override_for_state (
    _widget_state state) const {

  switch (state) {
    case WSTATE_SELECTED:
    case WSTATE_HOVER:
      return highlight_fg_colors;

    case WSTATE_DISABLED:
    case WSTATE_OFF:
      return disabled_fg_colors;

    case WSTATE_NORMAL:
    case WSTATE_ON:
    default:
      return active_fg_colors;
  }
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
void c_widget::shrinkwrap (int padding_x, int padding_y) {
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

  return (int) (widget->scale.init_x / widget->app->hdpi);
}

int c_widget::y () {
  if (!widget || !widget->app)
    return 0;

  return (int) (widget->scale.init_y / widget->app->hdpi);
}

int c_widget::w () {
  if (!widget || !widget->app)
    return 0;

  return (int) (widget->scale.init_width / widget->app->hdpi);
}

int c_widget::h () {
  if (!widget || !widget->app)
    return 0;

  return (int) (widget->scale.init_height / widget->app->hdpi);
}

void c_widget::move_resize (int x, int y, int w, int h) {
  move (x, y);
  resize (w, h);
}

void c_widget::move (int x, int y) {
  if (!widget)
    return;

  const int sx = x * widget->app->hdpi;
  const int sy = y * widget->app->hdpi;

  if (widget->scale.init_x == sx && widget->scale.init_y == sy) {
    //expose ();
    return;
  }

  widget->x = sx;
  widget->y = sy;
  widget->scale.init_x = sx;
  widget->scale.init_y = sy;

  os_move_window (widget->app->dpy, widget, sx, sy);
  //expose ();
}

void c_widget::resize (int w, int h) {
  if (!widget)
    return;

  const int sw = std::max (1, (int) (w * widget->app->hdpi));
  const int sh = std::max (1, (int) (h * widget->app->hdpi));

  if (widget->scale.init_width == sw && widget->scale.init_height == sh) {
    //expose ();
    return;
  }

  widget->scale.init_width = sw;
  widget->scale.init_height = sh;

  os_resize_window (widget->app->dpy, widget, sw, sh);
  //widget->func.configure_callback (widget, NULL);
  //expose ();
}

////////////////////////////////////////////////////////////////////////////////
// c_toplevelwindow / c_mainwindow

bool c_toplevelwindow::create (
    c_neuralblender_ui *ui_,
    nbtk::t_native_window parent_,
    const char *title_,
    int x, int y, int w, int h,
    nbtk::t_native_handle owner_) {

  id = get_unique_id ();
  label = title_ ? title_ : "";
  ui = ui_;
  parent = parent_;

  if (!ui || !ui->display)
    return false;

  if (!parent)
    parent = ui->tk_app.backend
      ? ui->tk_app.backend->default_root_window (ui->display)
      : 0;

  widget = create_window (&ui->app, parent, x, y, w, h);
  if (!widget)
    return false;

  window = widget->widget;
  widget->parent_struct = this;
  widget->label = label.c_str ();
  widget->scale.gravity = NONE;
  widget->func.resize_notify_callback = c_toplevelwindow::cb_resize;
  widget->func.configure_notify_callback =
      c_toplevelwindow::cb_configure_notify;
  widget->func.expose_callback = c_toplevelwindow::cb_expose;
  widget->func.key_press_callback = c_toplevelwindow::cb_key_press;
  widget->func.unmap_notify_callback = c_toplevelwindow::cb_close;

  os_register_wm_delete_window (widget);
  auto_close (true);
  Widget_t *owner = nbtk::as_xputty_widget (owner_);
  if (owner)
    os_set_transient_for_hint (owner, widget);
  set_title (title_);
  if (ui && ui->tk_app.backend)
    ui->tk_app.backend->set_window_background (widget, get_colortheme ()->window_bg);
  else
    set_x11_window_background (widget, get_colortheme ()->window_bg);

  return true;
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

void c_toplevelwindow::cb_configure_notify (void *w_, void *user_data) {
  (void) user_data;

  c_toplevelwindow *window = toplevel_from_widget (w_);
  if (!window)
    return;

  window->on_configure_notify ();
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

  Widget_t *widget = (Widget_t *) w_;
  if (widget && widget->app && !(widget->flags & HIDE_ON_DELETE))
    widget->app->run = false;
}

bool c_mainwindow::create (
    c_neuralblender_ui *ui_,
    Window parent_,
    const char *title_,
    int x, int y, int w, int h,
    nbtk::t_native_handle owner) {

  if (!create_tk (ui_, &ui_->tk_app, parent_, title_, x, y, w, h, owner))
    return false;

  auto_close (false);
  auto_hide_on_close (false);
  auto_quit_on_close (true);
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
  /*if (widget->app && widget->app->dpy)
    XFlush (widget->app->dpy);*/
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

void c_toplevelwindow::set_mouse_cursor (_mouse_cursor cursor) {
  if (!widget)
    return;

  if (ui && ui->tk_app.backend) {
    ui->tk_app.backend->set_mouse_cursor (widget, cursor);
    return;
  }

  nbtk::c_x11_native_backend fallback;
  fallback.set_mouse_cursor (widget, cursor);
}

bool c_toplevelwindow::request_size (int w, int h) {
  if (!widget)
    return false;

  if (ui && ui->tk_app.backend)
    return ui->tk_app.backend->request_size (widget, w, h);

  nbtk::c_x11_native_backend fallback;
  return fallback.request_size (widget, w, h);
}

bool c_toplevelwindow::is_created () const {
  return widget != NULL;
}

nbtk::t_native_handle c_toplevelwindow::native_handle () const {
  return widget;
}

bool c_toplevelwindow::get_metrics (int *w, int *h, bool *visible) const {
  if (!widget || !widget->app)
    return false;

  Metrics_t metrics;
  os_get_window_metrics (widget, &metrics);
  if (w)
    *w = metrics.width / widget->app->hdpi;
  if (h)
    *h = metrics.height / widget->app->hdpi;
  if (visible)
    *visible = metrics.visible;
  return true;
}

void c_toplevelwindow::force_draw () {
  if (widget)
    widget_draw (widget, NULL);
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
    old->on_focus_lost ();
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

  debug ("toplevel expose: metrics=%d,%d init=%d,%d",
         (int) metrics.width, (int) metrics.height,
         widget->scale.init_width, widget->scale.init_height);
  fill_rounded_rect (widget, 0, 0, metrics.width, metrics.height,
                     0.0f, g_colors->window_bg);
  
  //static c_printfps fps("c_toplevelwindow::on_expose ");
  //fps.tick ();
}

void c_toplevelwindow::on_resize () { CP
}

void c_toplevelwindow::on_configure_notify () {
}

void c_toplevelwindow::on_tk_action (nbtk::t_action_event &event) {
  (void) event;
}

void c_tkappbridge::invalidate_rect (int x, int y, int w, int h) {
  (void) x;
  (void) y;
  (void) w;
  (void) h;

  if (backend && native_window)
    backend->invalidate (native_window->widget);
}

void c_tkappbridge::set_mouse_cursor (_mouse_cursor cursor) {
  if (!native_window)
    return;

  if (backend)
    backend->set_mouse_cursor (native_window->widget, cursor);
}

void c_tkappbridge::set_focus (nbtk::c_widget *widget) {
  if (!native_window) {
    nbtk::c_app::set_focus (widget);
    return;
  }

  if (native_window->focused_widget == widget)
    return;

  nbtk::c_widget *old = native_window->focused_widget;
  native_window->focused_widget = widget;
  focused_widget = widget;

  if (old)
    old->invalidate ();
  if (widget)
    widget->invalidate ();
}

void c_tkappbridge::clear_focus (nbtk::c_widget *widget) {
  if (!native_window) {
    nbtk::c_app::clear_focus (widget);
    return;
  }

  if (widget && native_window->focused_widget != widget)
    return;

  nbtk::c_widget *old = native_window->focused_widget;
  native_window->focused_widget = nullptr;
  focused_widget = nullptr;

  if (old)
    old->invalidate ();
}

std::unique_ptr<nbtk::c_popupwindow> c_tkappbridge::create_popup (
    nbtk::c_widget *owner) {

  std::unique_ptr<nbtk::c_popupwindow> popup =
    std::make_unique<nbtk::c_popupwindow> ();
  popup->create_native_for_owner (
      this,
      owner,
      native_window ? native_window->widget : NULL,
      1,
      1);
  return popup;
}

nbtk::t_point c_tkappbridge::root_to_screen (nbtk::t_point p) const {
  if (!backend || !native_window || !native_window->widget)
    return nbtk::c_app::root_to_screen (p);

  return backend->root_to_screen (native_window->widget, p);
}

nbtk::t_point c_tkappbridge::screen_to_root (nbtk::t_point p) const {
  if (!backend || !native_window || !native_window->widget)
    return nbtk::c_app::screen_to_root (p);

  return backend->screen_to_root (native_window->widget, p);
}

void c_tkappbridge::on_action (nbtk::t_action_event &event) {
  if (action_owner)
    action_owner->on_tk_action (event);

  if (!event.handled)
    nbtk::c_app::on_action (event);
}

c_tktoplevelwindow::~c_tktoplevelwindow () {
  clear_tk_buffer ();
}

void c_tktoplevelwindow::clear_tk_buffer () {
  if (tk_cr) {
    cairo_destroy (tk_cr);
    tk_cr = NULL;
  }

  if (tk_surface) {
    cairo_surface_destroy (tk_surface);
    tk_surface = NULL;
  }

  tk_surface_w = 0;
  tk_surface_h = 0;
}

bool c_tktoplevelwindow::ensure_tk_buffer (int w, int h) {
  w = std::max (1, w);
  h = std::max (1, h);

  if (tk_surface && tk_cr && tk_surface_w == w && tk_surface_h == h)
    return cairo_surface_status (tk_surface) == CAIRO_STATUS_SUCCESS &&
           cairo_status (tk_cr) == CAIRO_STATUS_SUCCESS;

  clear_tk_buffer ();

  tk_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
  if (!tk_surface || cairo_surface_status (tk_surface) != CAIRO_STATUS_SUCCESS) {
    clear_tk_buffer ();
    return false;
  }

  tk_cr = cairo_create (tk_surface);
  if (!tk_cr || cairo_status (tk_cr) != CAIRO_STATUS_SUCCESS) {
    clear_tk_buffer ();
    return false;
  }

  tk_surface_w = w;
  tk_surface_h = h;
  return true;
}

bool c_tktoplevelwindow::create_tk (
    c_neuralblender_ui *ui_,
    c_tkappbridge *tk_app_,
    nbtk::t_native_window parent_,
    const char *title_,
    int x, int y, int w_, int h_,
    nbtk::t_native_handle owner) {

  tk_app = tk_app_;
  if (!tk_app)
    return false;

  if (!c_toplevelwindow::create (ui_, parent_, title_, x, y, w_, h_, owner))
    return false;

  if (!tk_app->backend)
    tk_app->backend = nbtk::create_native_backend ();
  if (tk_app->backend)
    tk_app->backend->disable_window_background (widget);

  widget->func.button_press_callback = c_tktoplevelwindow::cb_button_press;
  widget->func.button_release_callback = c_tktoplevelwindow::cb_button_release;
  widget->func.double_click_callback = c_tktoplevelwindow::cb_button_release;
  widget->func.motion_callback = c_tktoplevelwindow::cb_motion;
  widget->func.enter_callback = c_tktoplevelwindow::cb_enter;
  widget->func.leave_callback = c_tktoplevelwindow::cb_leave;
  widget->func.key_press_callback = c_tktoplevelwindow::cb_key_press;
  widget->func.key_release_callback = c_tktoplevelwindow::cb_key_release;
  os_set_input_mask (widget);

  tk_app->focused_widget = nullptr;
  tk_app->hovered_widget = nullptr;
  tk_app->mouse_captured = false;
  tk_app->main_window = nullptr;
  tk_app->cr = nullptr;
  tk_app->font_scale = widget->app ? widget->app->hdpi : 1.0f;
  tk_app->font_scale *= 1.1f;

  tk_root.app = tk_app;
  tk_root.parent = nullptr;
  tk_root.children.clear ();
  tk_root.doublebuffer = true;
  tk_root.move_resize (0, 0, this->w (), this->h ());
  activate_tk ();

  return true;
}

void c_tktoplevelwindow::activate_tk () {
  if (!tk_app)
    return;

  tk_app->root = &tk_root;
  tk_app->native_window = this;
  tk_app->action_owner = this;
  tk_app->focused_widget = focused_widget;
  tk_app->hovered_widget = hovered_widget;
  tk_app->mouse_captured = mouse_captured;
}

void c_tktoplevelwindow::save_tk_state () {
  if (!tk_app)
    return;

  focused_widget = tk_app->focused_widget;
  hovered_widget = tk_app->hovered_widget;
  mouse_captured = tk_app->mouse_captured;
}

void c_tktoplevelwindow::on_resize () {
  if (!widget || !widget->app)
    return;

  Metrics_t metrics;
  os_get_window_metrics (widget, &metrics);
  if (!metrics.visible)
    return;

  activate_tk ();
  if (tk_root.w > 0 && tk_root.h > 0) {
    tk_root.x = 0;
    tk_root.y = 0;
    tk_root.w = std::max (1, (int) (metrics.width / widget->app->hdpi));
    tk_root.h = std::max (1, (int) (metrics.height / widget->app->hdpi));
    return;
  }

  tk_root.move_resize (
      0,
      0,
      std::max (1, (int) (metrics.width / widget->app->hdpi)),
      std::max (1, (int) (metrics.height / widget->app->hdpi)));
}

void c_tktoplevelwindow::on_configure_notify () {
  on_resize ();
}

void c_tktoplevelwindow::on_expose () {
  if (!widget || !widget->crb)
    return;

  if (!tk_app)
    return;

  Metrics_t metrics;
  os_get_window_metrics (widget, &metrics);
  if (!metrics.visible)
    return;

  activate_tk ();

  cairo_t *draw_cr = widget->crb;
  if (tk_root.doublebuffer &&
      ensure_tk_buffer (std::max (1, tk_root.w), std::max (1, tk_root.h)))
    draw_cr = tk_cr;

  tk_app->cr = draw_cr;

  const t_gradientcolors &bg = get_colortheme ()->window_bg;
  cairo_save (draw_cr);
  cairo_set_operator (draw_cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (draw_cr, bg.r1, bg.g1, bg.b1, bg.a1);
  cairo_paint (draw_cr);
  cairo_set_operator (draw_cr, CAIRO_OPERATOR_OVER);
  cairo_restore (draw_cr);

  tk_root.draw_tree (draw_cr);
  tk_app->cr = nullptr;

  if (draw_cr == tk_cr) {
    cairo_surface_flush (tk_surface);

    cairo_save (widget->crb);
    cairo_set_operator (widget->crb, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface (widget->crb, tk_surface, 0, 0);
    cairo_paint (widget->crb);
    cairo_restore (widget->crb);
  }
}

void c_tktoplevelwindow::set_min_size (int w, int h) {
  if (tk_app && tk_app->backend)
    tk_app->backend->set_min_size (widget, w, h);
  else
    c_toplevelwindow::set_min_size (w, h);
}

void c_tktoplevelwindow::redraw_child (nbtk::c_widget &child, int pad) {
  if (!widget || !widget->crb || !widget->cr || !child.visible)
    return;

  Metrics_t metrics;
  os_get_window_metrics (widget, &metrics);
  if (!metrics.visible)
    return;

  activate_tk ();

  const nbtk::t_point p = child.local_to_root ({ 0, 0 });
  const int root_w = std::max (1, tk_root.w);
  const int root_h = std::max (1, tk_root.h);
  const int rx = std::max (0, p.x - pad);
  const int ry = std::max (0, p.y - pad);
  const int rw = std::min (root_w - rx, child.w + pad * 2);
  const int rh = std::min (root_h - ry, child.h + pad * 2);
  if (rw <= 0 || rh <= 0)
    return;

  tk_app->cr = widget->crb;
  cairo_save (widget->crb);
  cairo_rectangle (widget->crb, rx, ry, rw, rh);
  cairo_clip (widget->crb);
  cairo_translate (widget->crb, p.x - child.x, p.y - child.y);
  child.draw_tree (widget->crb);
  cairo_restore (widget->crb);
  tk_app->cr = nullptr;

  cairo_save (widget->cr);
  cairo_rectangle (widget->cr, rx, ry, rw, rh);
  cairo_clip (widget->cr);
  cairo_set_source_surface (widget->cr, widget->buffer, 0, 0);
  cairo_paint (widget->cr);
  cairo_restore (widget->cr);

  cairo_surface_flush (widget->surface);
}

void c_tktoplevelwindow::on_close () {
  if (handling_close)
    return;

  handling_close = true;

  if (hide_on_close)
    hide ();

  if (quit_on_close && widget && widget->app)
    widget->app->run = false;

  handling_close = false;
}

void c_tktoplevelwindow::auto_hide_on_close (bool b) {
  hide_on_close = b;
  if (b)
    auto_close (true);
}

void c_tktoplevelwindow::auto_quit_on_close (bool b) {
  quit_on_close = b;
  if (b)
    auto_close (false);
}

void c_tktoplevelwindow::cb_button_press (
    void *w_,
    void *event,
    void *user_data) {

  (void) user_data;
  Widget_t *w = (Widget_t *) w_;
  XButtonEvent *button = (XButtonEvent *) event;
  if (!w || !w->parent_struct || !button)
    return;

  c_tktoplevelwindow *self = (c_tktoplevelwindow *) w->parent_struct;
  if (self->tk_app) {
    self->activate_tk ();
    const int rx = button->x / w->app->hdpi;
    const int ry = button->y / w->app->hdpi;
    const bool handled = self->tk_root.mouse_down_tree (
      rx,
      ry,
      button->button);
    self->tk_app->mouse_captured = false;
    if (handled && self->tk_app->focused_widget) {
      nbtk::t_point local =
        self->tk_app->focused_widget->root_to_local ({ rx, ry });
      self->tk_app->mouse_captured =
        local.x >= 0 &&
        local.y >= 0 &&
        local.x < self->tk_app->focused_widget->w &&
        local.y < self->tk_app->focused_widget->h;
    }
    self->save_tk_state ();
  }
  expose_widget (w);
}

void c_tktoplevelwindow::cb_button_release (
    void *w_,
    void *event,
    void *user_data) {

  (void) user_data;
  Widget_t *w = (Widget_t *) w_;
  XButtonEvent *button = (XButtonEvent *) event;
  if (!w || !w->parent_struct || !button)
    return;

  c_tktoplevelwindow *self = (c_tktoplevelwindow *) w->parent_struct;
  if (self->tk_app) {
    self->activate_tk ();
    const int rx = button->x / w->app->hdpi;
    const int ry = button->y / w->app->hdpi;
    if (self->tk_app->mouse_captured && self->tk_app->focused_widget) {
      nbtk::t_point local =
        self->tk_app->focused_widget->root_to_local ({ rx, ry });
      self->tk_app->focused_widget->on_mouse_up (
        local.x, local.y, button->button);
    } else {
      self->tk_root.mouse_up_tree (rx, ry, button->button);
    }
    self->tk_app->mouse_captured = false;
    self->save_tk_state ();
  }
  expose_widget (w);
}

void c_tktoplevelwindow::cb_motion (
    void *w_,
    void *event,
    void *user_data) {

  (void) user_data;
  Widget_t *w = (Widget_t *) w_;
  XMotionEvent *motion = (XMotionEvent *) event;
  if (!w || !w->parent_struct || !motion)
    return;

  c_tktoplevelwindow *self = (c_tktoplevelwindow *) w->parent_struct;
  if (self->tk_app) {
    self->activate_tk ();
    const int rx = motion->x / w->app->hdpi;
    const int ry = motion->y / w->app->hdpi;
    if (self->tk_app->mouse_captured && self->tk_app->focused_widget) {
      nbtk::t_point local =
        self->tk_app->focused_widget->root_to_local ({ rx, ry });
      self->tk_app->focused_widget->on_mouse_move (local.x, local.y);
    } else {
      self->tk_root.update_hover_tree (rx, ry);
    }
    self->save_tk_state ();
  }
}

void c_tktoplevelwindow::cb_enter (void *w_, void *user_data) {
  (void) user_data;
  Widget_t *w = (Widget_t *) w_;
  if (!w || !w->parent_struct || !w->app)
    return;

  c_tktoplevelwindow *self = (c_tktoplevelwindow *) w->parent_struct;
  if (self->tk_app) {
    nbtk::t_point local;
    if (!self->tk_app->backend ||
        !self->tk_app->backend->query_pointer (w, &local))
      return;

    self->activate_tk ();
    self->tk_root.update_hover_tree (local.x, local.y);
    self->save_tk_state ();
  }
}

void c_tktoplevelwindow::cb_leave (void *w_, void *user_data) {
  (void) user_data;
  Widget_t *w = (Widget_t *) w_;
  if (!w || !w->parent_struct)
    return;

  c_tktoplevelwindow *self = (c_tktoplevelwindow *) w->parent_struct;
  if (self->tk_app) {
    self->activate_tk ();
    self->tk_root.clear_hover_tree ();
    self->save_tk_state ();
  }
}

static int tk_key_from_xkey (XKeyEvent *key) {
  if (!key)
    return nbtk::KEY_UNKNOWN;

  const KeySym sym = XLookupKeysym (key, 0);
  switch (sym) {
    case XK_Tab:       return nbtk::KEY_TAB;
    case XK_BackSpace: return nbtk::KEY_BACKSPACE;
    case XK_Delete:    return nbtk::KEY_DELETE;
    case XK_Up:        return nbtk::KEY_UP;
    case XK_Down:      return nbtk::KEY_DOWN;
    case XK_Left:      return nbtk::KEY_LEFT;
    case XK_Right:     return nbtk::KEY_RIGHT;
    case XK_Home:      return nbtk::KEY_HOME;
    case XK_End:       return nbtk::KEY_END;
    case XK_Return:
    case XK_KP_Enter:  return nbtk::KEY_RETURN;
    case XK_Escape:    return nbtk::KEY_ESCAPE;
    default:           return nbtk::KEY_UNKNOWN;
  }
}

void c_tktoplevelwindow::cb_key_press (
    void *w_,
    void *event,
    void *user_data) {

  (void) user_data;
  Widget_t *w = (Widget_t *) w_;
  XKeyEvent *key = (XKeyEvent *) event;
  if (!w || !w->parent_struct || !key)
    return;

  c_tktoplevelwindow *self = (c_tktoplevelwindow *) w->parent_struct;
  if (!self->tk_app)
    return;

  self->activate_tk ();

  const int tk_key = tk_key_from_xkey (key);
  if (tk_key != nbtk::KEY_UNKNOWN)
    self->tk_app->dispatch_key_down (tk_key);

  char text [32] = {};
  KeySym ignored = 0;
  const int len = XLookupString (key, text, sizeof (text) - 1, &ignored, NULL);
  if (len > 0 && text [0] >= 32)
    self->tk_app->dispatch_text_input (text);

  self->save_tk_state ();
}

void c_tktoplevelwindow::cb_key_release (
    void *w_,
    void *event,
    void *user_data) {

  (void) user_data;
  Widget_t *w = (Widget_t *) w_;
  XKeyEvent *key = (XKeyEvent *) event;
  if (!w || !w->parent_struct || !key)
    return;

  c_tktoplevelwindow *self = (c_tktoplevelwindow *) w->parent_struct;
  if (!self->tk_app)
    return;

  self->activate_tk ();

  const int tk_key = tk_key_from_xkey (key);
  if (tk_key != nbtk::KEY_UNKNOWN)
    self->tk_app->dispatch_key_up (tk_key);

  self->save_tk_state ();
}

////////////////////////////////////////////////////////////////////////////////
// nbtk::c_filepicker

static std::string path_join (const std::string &dir, const std::string &name) {
  if (dir.empty () || dir == "/")
    return "/" + name;

  if (dir.back () == '/')
    return dir + name;

  return dir + "/" + name;
}

static std::string path_parent_dir (const std::string &path) {
  if (path.empty () || path == "/")
    return "/";

  std::string trimmed = path;
  while (trimmed.size () > 1 && trimmed.back () == '/')
    trimmed.pop_back ();

  return path_dirname (trimmed);
}

void nbtk::c_filepicker::sync_owner_metadata () {
  Widget_t *owner_widget_ = as_xputty_widget (owner_widget);
  if (!owner_widget_ || !owner_widget_->parent_struct)
    return;

  c_widget *owner = (c_widget *) owner_widget_->parent_struct;
  if (owner->bank < BANK_COUNT)
    bank = owner->bank;
  if (owner->lane < NB_NUM_MODELS)
    lane = owner->lane;
}

void nbtk::c_filepicker::create (
    c_neuralblender_ui *ui_,
    c_tkappbridge *tk_app_,
    t_native_window parent_,
    t_native_handle owner,
    size_t lane_,
    uint64_t bank_,
    const char *title_) {

  title = title_ ? title_ : "";
  owner_widget = owner;
  if (!create_tk (
      ui_,
      tk_app_,
      parent_,
      title.c_str (),
      0,
      0,
      520,
      420,
      as_xputty_widget (owner)))
    return;

  lane = lane_;
  bank = bank_;
  auto_hide_on_close (true);
  auto_quit_on_close (false);

  frame.create (&tk_root, "", 8, 8, 504, 360);
  label_path.create (&frame, "", 12, 10, 480, 24);
  label_path.align = TEXT_LEFT;
  label_path.size = 12.0f;

  listbox.create (&frame, "", 12, 42, 456, 300);
  listbox.activate_on_click_again = true;
  listbox.activate_on_doubleclick = false;
  listbox.set_vscrollbar (&vscrollbar);

  vscrollbar.create (&frame, "", 472, 42, 18, 300);
  vscrollbar.set_container (&listbox);
  vscrollbar.set_orientation (SCROLLBAR_VERTICAL);

  btn_ok.create (&tk_root, "OK", 342, 380, 80, 30);
  btn_ok.set_image_default (data_xputty_approved_png);

  btn_cancel.create (&tk_root, "Cancel", 432, 380, 80, 30);
  btn_cancel.set_image_default (data_xputty_cancel_png);

  on_resize ();
}

void nbtk::c_filepicker::show () {
  if (!widget)
    return;

  sync_owner_metadata ();

  if (current_dir.empty () && ui)
    current_dir = ui->configfile.get_item (cwd_config_key_for_bank (bank));
  if (current_dir.empty ())
    current_dir = CONFIG_DEFAULT_DIR;

  scan_current_dir ();
  widget_show_all (widget);
  expose ();
}

void nbtk::c_filepicker::hide () {
  if (widget)
    widget_hide (widget);
}

void nbtk::c_filepicker::on_resize () {
  c_tktoplevelwindow::on_resize ();

  const int ww = tk_root.w;
  const int wh = tk_root.h;
  const int pad = 8;
  const int button_w = 120;
  const int button_h = 40;
  const int button_y = std::max (pad, wh - pad - button_h);
  const int list_y = 42;
  const int frame_h = std::max (80, button_y - pad * 2);
  const int frame_w = std::max (120, ww - pad * 2);
  const int scroll_w = UI_SCROLLBAR_WIDTH;
  const int list_h = std::max (24, frame_h - list_y - 12);
  const int list_w = std::max (24, frame_w - 36 - scroll_w);

  frame.move_resize (pad, pad, frame_w, frame_h);
  label_path.move_resize (12, 10, std::max (24, frame_w - 24), 24);
  listbox.move_resize (12, list_y, list_w, list_h);
  vscrollbar.move_resize (12 + list_w + 4, list_y, scroll_w, list_h);
  listbox.sync_scrollbar ();
  btn_ok.move_resize (ww - pad - button_w, button_y, button_w, button_h);
  btn_cancel.move_resize (
      ww - pad * 2 - button_w * 2,
      button_y,
      button_w,
      button_h);
}

void nbtk::c_filepicker::on_tk_action (t_action_event &event) {
  if (event.source_id == btn_cancel.id) {
    event.handled = true;
    hide ();
    return;
  }

  if (event.source_id == btn_ok.id ||
      (event.source_id == listbox.id && event.value)) {
    event.handled = true;
    const int selected =
      event.source_id == listbox.id ? event.source_index : listbox.selected;
    if (selected >= 0 && selected < (int) rows.size () && rows [selected].directory) {
      set_current_dir (rows [selected].path);
    } else {
      std::string path = selected_path ();
      if (!path.empty ())
        on_file_select (path);
    }
    return;
  }

  c_tktoplevelwindow::on_tk_action (event);
}

void nbtk::c_filepicker::scan_current_dir () {
  filelist.clear ();
  rows.clear ();

  if (current_dir.empty ()) {
    listbox.clear ();
    return;
  }

  DIR *dir = opendir (current_dir.c_str ());
  if (!dir) {
    debug ("failed to scan '%s'", current_dir.c_str ());
    listbox.clear ();
    return;
  }

  std::vector<t_listrow> dir_rows;
  std::vector<t_listrow> file_rows;

  if (current_dir != "/")
    dir_rows.push_back ({ "../", path_parent_dir (current_dir), true });

  struct dirent *entry = NULL;
  while ((entry = readdir (dir))) {
    const char *name = entry->d_name;
    if (!name || !strcmp (name, ".") || !strcmp (name, ".."))
      continue;

    std::string full = path_join (current_dir, name);

    struct stat lst;
    const bool is_symlink =
      !lstat (full.c_str (), &lst) && S_ISLNK (lst.st_mode);

    struct stat st;
    if (stat (full.c_str (), &st))
      continue;

    if (S_ISDIR (st.st_mode)) {
      dir_rows.push_back ({ std::string (name) + "/", full, true, is_symlink });
    } else if (S_ISREG (st.st_mode) && is_supported_model_filename (name)) {
      file_rows.push_back ({ name, full, false, is_symlink });
      filelist.push_back (name);
    }
  }
  closedir (dir);

  std::sort (filelist.begin (), filelist.end ());
  std::sort (
      dir_rows.begin (), dir_rows.end (),
      [] (const t_listrow &a, const t_listrow &b) { return a.label < b.label; });
  std::sort (
      file_rows.begin (), file_rows.end (),
      [] (const t_listrow &a, const t_listrow &b) { return a.label < b.label; });

  rows.reserve (dir_rows.size () + file_rows.size ());
  rows.insert (rows.end (), dir_rows.begin (), dir_rows.end ());
  rows.insert (rows.end (), file_rows.begin (), file_rows.end ());

  listbox.selected = -1;
  listbox.first_visible = 0;
  listbox.set_rows (rows);
  label_path.label = current_dir;
  label_path.invalidate ();
  debug (
      "scan '%s': %zu dirs, %zu model files",
      current_dir.c_str (),
      dir_rows.size (),
      filelist.size ());
}

void nbtk::c_filepicker::add_files_from_dir (c_combobox *cb) {
  if (!cb)
    return;

  cb->items.clear ();
  combo_dir = current_dir;

  int sel = -1;
  _lane_bank bank_ = (bank < BANK_COUNT) ? (_lane_bank) bank : BANK_AMP;
  std::string selected_file;
  if (ui && lane < NB_NUM_MODELS)
    selected_file = ui->state.banks [bank_].lanes [lane].filename;

  if (!selected_file.empty ()) {
    std::string loaded_dir = path_dirname (selected_file);
    if (!loaded_dir.empty ())
      combo_dir = loaded_dir;
  }

  std::vector<std::string> combo_files;
  DIR *dir = opendir (combo_dir.c_str ());
  if (dir) {
    struct dirent *entry = NULL;
    while ((entry = readdir (dir))) {
      const char *name = entry->d_name;
      if (!name || !strcmp (name, ".") || !strcmp (name, ".."))
        continue;

      std::string full = path_join (combo_dir, name);
      struct stat st;
      if (!stat (full.c_str (), &st) &&
          S_ISREG (st.st_mode) &&
          is_supported_model_filename (name))
        combo_files.push_back (name);
    }
    closedir (dir);
  }
  std::sort (combo_files.begin (), combo_files.end ());

  for (size_t i = 0; i < combo_files.size (); i++) {
    cb->items.push_back (combo_files [i]);

    std::string full = combo_dir;
    if (!full.empty () && full.back () != '/')
      full += '/';
    full += combo_files [i];

    if (full == selected_file || combo_files [i] == selected_file)
      sel = (int) i;
  }

  cb->selected = sel;
  cb->update_widget ();
}

std::string nbtk::c_filepicker::get_current_dir () const {
  return current_dir;
}

void nbtk::c_filepicker::set_current_dir (std::string str) {
  current_dir = std::move (str);
  if (ui && !current_dir.empty ())
    ui->configfile.set_item (cwd_config_key_for_bank (bank), current_dir);
  scan_current_dir ();
}

bool nbtk::c_filepicker::is_visible () const {
  if (!widget)
    return false;

  Metrics_t m;
  os_get_window_metrics (widget, &m);
  return m.visible;
}

std::string nbtk::c_filepicker::selected_path () const {
  if (listbox.selected < 0 || listbox.selected >= (int) rows.size ())
    return "";

  const t_listrow &row = rows [(size_t) listbox.selected];
  return row.directory ? "" : row.path;
}

void nbtk::c_filepicker::on_file_select (const std::string &filename) {
  if (!ui || filename.empty ())
    return;

  _lane_bank bank_ = (bank < BANK_COUNT) ? (_lane_bank) bank : BANK_AMP;
  if (lane >= NB_NUM_MODELS)
    return;

  current_dir = path_dirname (filename);
  if (!current_dir.empty ())
    ui->configfile.set_item (cwd_config_key_for_bank (bank), current_dir);

  ui->state.banks [bank_].lanes [lane].filename = filename;
  ui->state.current_dir = current_dir;
  scan_current_dir ();
  ui->load_model (bank_, lane, filename.c_str ());

  nbtk::c_combobox *cb = &ui->lanes_for_bank (bank_) [lane].menu_list;
  cb->clear ();
  add_files_from_dir (cb);
  ui->on_fileselected (this, filename.c_str ());
  hide ();
}

bool c_toplevelwindow::on_keydown (XKeyEvent *key) { CP
  if (!widget || !key)
    return false;

  if (!focused_widget)
    return false;

  return focused_widget->on_keydown (key);
}

void c_mainwindow::show () {
  if (!widget)
    return;

  children_mapped = false;
  widget_show (widget);
  /*if (widget->app && widget->app->dpy)
    XFlush (widget->app->dpy);*/
}

void c_mainwindow::show_children () {
  if (!widget || children_mapped)
    return;

  children_mapped = true;
  if (ui)
    ui->sync_page_visibility ();
}

void c_mainwindow::on_expose () {
  if (!widget)
    return;

  c_tktoplevelwindow::on_expose ();
  show_children ();
}

void c_mainwindow::on_resize () { CP
  if (!widget || !ui)
    return;

  Metrics_t metrics;
  os_get_window_metrics (widget, &metrics);
  if (!metrics.visible)
    return;

  debug ("mainwindow resize: metrics=%d,%d init=%d,%d",
         (int) metrics.width, (int) metrics.height,
         widget->scale.init_width, widget->scale.init_height);
  c_tktoplevelwindow::on_resize ();
  ui->on_window_resize (
      metrics.width / widget->app->hdpi,
      metrics.height / widget->app->hdpi);
}

void c_mainwindow::on_configure_notify () {
  if (!ui)
    return;

  c_tktoplevelwindow::on_configure_notify ();
  ui->on_window_configured ();
}

void c_mainwindow::on_tk_action (nbtk::t_action_event &event) {
  if (ui)
    ui->on_tk_action (event);
}
