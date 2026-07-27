
/* NeuralBlender
 *
 * Minimal X11/Cairo backend compatibility layer for nbtk.
 */

#include <algorithm>
#include <cstring>
#include <vector>

#include "nbtk.h"

struct nbtk::t_native_childlist {
  std::vector<nbtk::t_native_widget *> children;
};

static Atom g_wm_delete_window = 0;

static void native_move_window (Display *dpy, nbtk::t_native_widget *w, int x, int y);
static void native_resize_window (Display *dpy, nbtk::t_native_widget *w, int w_, int h_);
static void native_translate_coords (
    nbtk::t_native_widget *w, Window from_window, Window to_window,
    int from_x, int from_y, int *to_x, int *to_y);

static nbtk::t_native_widget *as_native_widget (nbtk::t_native_handle handle) {
  return (nbtk::t_native_widget *) handle;
}

static unsigned long x11_color_pixel (
    Display *display,
    const nbtk::t_gradientcolors &colors) {

  if (!display)
    return 0;

  const int screen = DefaultScreen (display);
  XColor color {};
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

static void set_x11_window_background (
    nbtk::t_native_widget *w,
    const nbtk::t_gradientcolors &colors) {

  if (!w || !w->app || !w->app->dpy || !w->widget)
    return;

  Display *display = w->app->dpy;
  XSetWindowBackground (display, w->widget, x11_color_pixel (display, colors));
  XClearWindow (display, w->widget);
}

static void disable_x11_window_background (nbtk::t_native_widget *w) {
  if (!w || !w->app || !w->app->dpy || !w->widget)
    return;

  XSetWindowBackgroundPixmap (w->app->dpy, w->widget, None);
}

static void set_window_hints (Display *display, Window window, int w, int h) {
  if (!display || !window)
    return;

  XSizeHints *hints = XAllocSizeHints ();
  if (!hints)
    return;

  hints->flags = PMinSize | PBaseSize;
  hints->min_width = w;
  hints->min_height = h;
  hints->base_width = w;
  hints->base_height = h;
  XSetWMNormalHints (display, window, hints);
  XFree (hints);
}

namespace nbtk {

class c_x11_native_backend : public c_native_backend {
public:
  void init_app (t_native_app *app) override {
    if (app)
      native_app_init (app);
  }

  void shutdown_app (t_native_app *app) override {
    if (app)
      native_app_shutdown (app);
  }

  void run_events (t_native_app *app) override {
    if (app)
      native_app_run_events (app);
  }

  void flush_dirty (t_native_app *app) override {
    if (app)
      native_app_flush_dirty (app);
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

    XWindowAttributes attr {};
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
    nbtk::t_native_widget *widget = as_native_widget (handle);
    if (widget)
      native_widget_invalidate (widget);
  }

  void flush (t_native_handle handle) override {
    nbtk::t_native_widget *widget = as_native_widget (handle);
    if (widget && widget->app && widget->app->dpy)
      XFlush (widget->app->dpy);
  }

  bool grab_pointer (t_native_handle handle) override {
    nbtk::t_native_widget *widget = as_native_widget (handle);
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
    nbtk::t_native_widget *widget = as_native_widget (handle);
    if (widget && widget->app && widget->app->dpy)
      XUngrabPointer (widget->app->dpy, CurrentTime);
  }

  bool query_pointer (t_native_handle handle, t_point *local) const override {
    nbtk::t_native_widget *widget = as_native_widget (handle);
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
      const nbtk::t_gradientcolors &colors) override {
    set_x11_window_background (as_native_widget (handle), colors);
  }

  void disable_window_background (t_native_handle handle) override {
    disable_x11_window_background (as_native_widget (handle));
  }

  void set_min_size (t_native_handle handle, int w, int h) override {
    nbtk::t_native_widget *widget = as_native_widget (handle);
    if (!widget || !widget->app || !widget->app->dpy)
      return;

    const float hdpi = widget->app->hdpi > 0.0f ? widget->app->hdpi : 1.0f;
    const int native_w = std::max (1, (int) (w * hdpi));
    const int native_h = std::max (1, (int) (h * hdpi));
    Display *display = widget->app->dpy;
    set_window_hints (display, widget->widget, native_w, native_h);

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

      set_window_hints (display, parent, native_w, native_h);
      current = parent;
    }
  }

  bool request_size (t_native_handle handle, int w, int h) override {
    nbtk::t_native_widget *widget = as_native_widget (handle);
    if (!widget || !widget->app || !widget->app->dpy)
      return false;

    const float hdpi = widget->app->hdpi;
    native_resize_window (
        widget->app->dpy,
        widget,
        std::max (1, (int) (w * hdpi)),
        std::max (1, (int) (h * hdpi)));
    return true;
  }

  void move_resize (t_native_handle handle, int x, int y, int w, int h) override {
    nbtk::t_native_widget *widget = as_native_widget (handle);
    if (!widget || !widget->app)
      return;

    const float hdpi = widget->app->hdpi;
    native_move_window (
        widget->app->dpy,
        widget,
        (int) (x * hdpi),
        (int) (y * hdpi));
    native_resize_window (
        widget->app->dpy,
        widget,
        std::max (1, (int) (w * hdpi)),
        std::max (1, (int) (h * hdpi)));
  }

  void set_mouse_cursor (t_native_handle handle, nbtk::_mouse_cursor cursor) override {
    nbtk::t_native_widget *widget = as_native_widget (handle);
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

  void set_keyboard_focus (t_native_handle handle) override {
    nbtk::t_native_widget *widget = as_native_widget (handle);
    if (widget && widget->app && widget->app->dpy)
      XSetInputFocus (
          widget->app->dpy,
          widget->widget,
          RevertToParent,
          CurrentTime);
  }

  t_native_window root_window (t_native_handle handle, bool is_widget) const override {
    nbtk::t_native_widget *widget = as_native_widget (handle);
    if (!widget || !widget->app)
      return 0;

    return native_get_root_window (widget->app, is_widget ? IS_WIDGET : IS_WINDOW);
  }

  t_point root_to_screen (t_native_handle handle, t_point p) const override {
    nbtk::t_native_widget *widget = as_native_widget (handle);
    if (!widget || !widget->app)
      return p;

    int sx = p.x;
    int sy = p.y;
    const float hdpi = widget->app->hdpi;
    native_translate_coords (
        widget,
        widget->widget,
        root_window (handle, true),
        (int) (p.x * hdpi),
        (int) (p.y * hdpi),
        &sx,
        &sy);

    return { (int) (sx / hdpi), (int) (sy / hdpi) };
  }

  t_point screen_to_root (t_native_handle handle, t_point p) const override {
    nbtk::t_native_widget *widget = as_native_widget (handle);
    if (!widget || !widget->app)
      return p;

    int rx = p.x;
    int ry = p.y;
    const float hdpi = widget->app->hdpi;
    native_translate_coords (
        widget,
        root_window (handle, true),
        widget->widget,
        (int) (p.x * hdpi),
        (int) (p.y * hdpi),
        &rx,
        &ry);

    return { (int) (rx / hdpi), (int) (ry / hdpi) };
  }
};

std::unique_ptr<c_native_backend> create_native_backend () {
  return std::make_unique<c_x11_native_backend> ();
}

} // namespace nbtk

static nbtk::t_native_widget *find_widget_by_window (nbtk::t_native_childlist *list, Window window) {
  if (!list)
    return nullptr;

  for (nbtk::t_native_widget *w : list->children) {
    if (w && w->widget == window)
      return w;
  }

  return nullptr;
}

static void destroy_cairo (nbtk::t_native_widget *w) {
  if (!w)
    return;

  if (w->crb) {
    cairo_destroy (w->crb);
    w->crb = nullptr;
  }
  if (w->buffer) {
    cairo_surface_destroy (w->buffer);
    w->buffer = nullptr;
  }
  if (w->cr) {
    cairo_destroy (w->cr);
    w->cr = nullptr;
  }
  if (w->surface) {
    cairo_surface_destroy (w->surface);
    w->surface = nullptr;
  }
}

static void ensure_cairo (nbtk::t_native_widget *w) {
  if (!w || !w->app || !w->app->dpy || !w->widget)
    return;

  XWindowAttributes attr;
  if (XGetWindowAttributes (w->app->dpy, w->widget, &attr) &&
      attr.width > 0 && attr.height > 0) {
    w->width = attr.width;
    w->height = attr.height;
    w->x = attr.x;
    w->y = attr.y;
    w->visible = attr.map_state == IsViewable;
  }

  const int width = std::max (1, w->width);
  const int height = std::max (1, w->height);
  if (w->surface &&
      cairo_xlib_surface_get_width (w->surface) == width &&
      cairo_xlib_surface_get_height (w->surface) == height &&
      w->buffer)
    return;

  destroy_cairo (w);

  const int screen = DefaultScreen (w->app->dpy);
  Visual *visual = DefaultVisual (w->app->dpy, screen);
  w->surface = cairo_xlib_surface_create (
      w->app->dpy, w->widget, visual, width, height);
  w->cr = cairo_create (w->surface);
  w->buffer = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
  w->crb = cairo_create (w->buffer);
}

void native_app_init (nbtk::t_native_app *app) {
  if (!app)
    return;

  std::memset (app, 0, sizeof (*app));
  app->dpy = XOpenDisplay (nullptr);
  app->run = app->dpy != nullptr;
  app->hdpi = 1.0f;
  app->childlist = new nbtk::t_native_childlist;
  if (app->dpy)
    g_wm_delete_window = XInternAtom (app->dpy, "WM_DELETE_WINDOW", False);
}

void native_app_shutdown (nbtk::t_native_app *app) {
  if (!app)
    return;

  if (app->childlist) {
    for (nbtk::t_native_widget *w : app->childlist->children) {
      if (!w)
        continue;
      destroy_cairo (w);
      if (w->app && w->app->dpy && w->widget && w->owns_native_window)
        XDestroyWindow (w->app->dpy, w->widget);
      delete w->childlist;
      delete w;
    }
    delete app->childlist;
    app->childlist = nullptr;
  }

  if (app->dpy) {
    XCloseDisplay (app->dpy);
    app->dpy = nullptr;
  }
  app->run = false;
}

static void dispatch_event (nbtk::t_native_app *app, XEvent &event) {
  nbtk::t_native_widget *w = find_widget_by_window (app->childlist, event.xany.window);
  if (!w)
    return;

  switch (event.type) {
    case Expose:
      if (event.xexpose.count == 0)
        native_widget_draw (w, nullptr);
      break;
    case ConfigureNotify:
      while (app && app->dpy) {
        XEvent next;
        if (!XCheckTypedWindowEvent (
              app->dpy, event.xconfigure.window, ConfigureNotify, &next))
          break;
        event = next;
      }
      w->width = std::max (1, event.xconfigure.width);
      w->height = std::max (1, event.xconfigure.height);
      w->scale.init_width = w->width;
      w->scale.init_height = w->height;
      destroy_cairo (w);
      if (w->func.resize_notify_callback)
        w->func.resize_notify_callback (w, nullptr);
      if (w->func.configure_notify_callback)
        w->func.configure_notify_callback (w, nullptr);
      if (w->func.configure_callback)
        w->func.configure_callback (w, nullptr);
      break;
    case ButtonPress:
      if (w->func.button_press_callback)
        w->func.button_press_callback (w, &event.xbutton, nullptr);
      break;
    case ButtonRelease:
      if (w->func.button_release_callback)
        w->func.button_release_callback (w, &event.xbutton, nullptr);
      break;
    case MotionNotify:
      if (w->func.motion_callback)
        w->func.motion_callback (w, &event.xmotion, nullptr);
      break;
    case EnterNotify:
      if (w->func.enter_callback)
        w->func.enter_callback (w, nullptr);
      break;
    case LeaveNotify:
      if (w->func.leave_callback)
        w->func.leave_callback (w, nullptr);
      break;
    case KeyPress:
      if (w->func.key_press_callback)
        w->func.key_press_callback (w, &event.xkey, nullptr);
      break;
    case KeyRelease:
      if (w->func.key_release_callback)
        w->func.key_release_callback (w, &event.xkey, nullptr);
      break;
    case ClientMessage:
      if ((Atom) event.xclient.data.l[0] == g_wm_delete_window) {
        if (w->func.unmap_notify_callback)
          w->func.unmap_notify_callback (w, nullptr);
      }
      break;
    case UnmapNotify:
      w->visible = false;
      break;
    case MapNotify:
      w->visible = true;
      if (w->func.map_notify_callback)
        w->func.map_notify_callback (w, nullptr);
      break;
    default:
      break;
  }
}

void native_app_flush_dirty (nbtk::t_native_app *app) {
  if (!app || !app->childlist)
    return;

  bool drew = false;
  for (nbtk::t_native_widget *w : app->childlist->children) {
    if (!w || !w->dirty || !w->visible)
      continue;

    w->dirty = false;
    native_widget_draw (w, nullptr);
    drew = true;
  }

  if (drew && app->dpy)
    XFlush (app->dpy);
}

void native_app_run_events (nbtk::t_native_app *app) {
  if (!app || !app->dpy)
    return;

  while (XPending (app->dpy) > 0) {
    XEvent event;
    XNextEvent (app->dpy, &event);
    dispatch_event (app, event);
  }

  native_app_flush_dirty (app);
}

nbtk::t_native_widget *native_create_window (nbtk::t_native_app *app, Window parent, int x, int y, int w_, int h_) {
  if (!app || !app->dpy)
    return nullptr;

  if (!parent)
    parent = DefaultRootWindow (app->dpy);

  nbtk::t_native_widget *w = new nbtk::t_native_widget {};
  w->app = app;
  w->parent = nullptr;
  w->flags = IS_WINDOW;
  w->x = x;
  w->y = y;
  w->width = std::max (1, w_);
  w->height = std::max (1, h_);
  w->scale.init_x = x;
  w->scale.init_y = y;
  w->scale.init_width = w->width;
  w->scale.init_height = w->height;
  w->scale.ascale = 1.0f;
  w->scale.gravity = NONE;
  w->childlist = new nbtk::t_native_childlist;
  w->visible = false;
  w->owns_native_window = true;
  w->dirty = true;

  const int screen = DefaultScreen (app->dpy);
  const unsigned long bg = BlackPixel (app->dpy, screen);
  w->widget = XCreateSimpleWindow (
      app->dpy, parent, x, y, w->width, w->height, 0, bg, bg);

  native_set_input_mask (w);
  native_childlist_add_child (app->childlist, w);
  return w;
}

void native_widget_show (nbtk::t_native_widget *w) {
  if (!w || !w->app || !w->app->dpy)
    return;
  XMapWindow (w->app->dpy, w->widget);
  w->visible = true;
  w->dirty = true;
}

void native_widget_hide (nbtk::t_native_widget *w) {
  if (!w || !w->app || !w->app->dpy)
    return;
  XUnmapWindow (w->app->dpy, w->widget);
  w->visible = false;
}

void native_widget_draw (nbtk::t_native_widget *w, void *user_data) {
  (void) user_data;
  if (!w || !w->app || !w->app->dpy)
    return;

  ensure_cairo (w);
  if (w->crb) {
    cairo_save (w->crb);
    cairo_set_operator (w->crb, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba (w->crb, 0.0, 0.0, 0.0, 0.0);
    cairo_paint (w->crb);
    cairo_restore (w->crb);
  }
  if (w->func.expose_callback)
    w->func.expose_callback (w, nullptr);

  if (w->cr && w->buffer) {
    cairo_set_source_surface (w->cr, w->buffer, 0, 0);
    cairo_paint (w->cr);
    cairo_surface_flush (w->surface);
  }
}

void native_widget_set_title (nbtk::t_native_widget *w, const char *title) {
  if (!w || !w->app || !w->app->dpy)
    return;
  XStoreName (w->app->dpy, w->widget, title ? title : "");
}

void native_widget_set_icon_from_png (nbtk::t_native_widget *w, const unsigned char *png) {
  (void) w;
  (void) png;
}

void native_widget_invalidate (nbtk::t_native_widget *w) {
  if (!w)
    return;

  w->dirty = true;
}

Window native_get_root_window (nbtk::t_native_app *app, int flag) {
  (void) flag;
  return app && app->dpy ? DefaultRootWindow (app->dpy) : 0;
}

void native_get_window_metrics (nbtk::t_native_widget *w, Metrics_t *metrics) {
  if (!metrics)
    return;
  *metrics = {};
  if (!w)
    return;

  metrics->width = w->width;
  metrics->height = w->height;
  metrics->x = w->x;
  metrics->y = w->y;
  metrics->visible = w->visible;

  if (!w->app || !w->app->dpy || !w->widget)
    return;

  XWindowAttributes attr;
  if (!XGetWindowAttributes (w->app->dpy, w->widget, &attr))
    return;

  metrics->width = attr.width;
  metrics->height = attr.height;
  metrics->x = attr.x;
  metrics->y = attr.y;
  metrics->visible = attr.map_state == IsViewable;

}

void native_set_window_min_size (
    nbtk::t_native_widget *w, int min_width, int min_height, int base_width, int base_height) {
  if (!w || !w->app || !w->app->dpy)
    return;
  XSizeHints hints {};
  hints.flags = PMinSize | PBaseSize;
  hints.min_width = min_width;
  hints.min_height = min_height;
  hints.base_width = base_width;
  hints.base_height = base_height;
  XSetWMNormalHints (w->app->dpy, w->widget, &hints);
}

static void native_move_window (Display *dpy, nbtk::t_native_widget *w, int x, int y) {
  if (!dpy || !w)
    return;
  w->x = x;
  w->y = y;
  XMoveWindow (dpy, w->widget, x, y);
}

static void native_resize_window (Display *dpy, nbtk::t_native_widget *w, int w_, int h_) {
  if (!dpy || !w)
    return;
  w->width = std::max (1, w_);
  w->height = std::max (1, h_);
  destroy_cairo (w);
  XResizeWindow (dpy, w->widget, w->width, w->height);
}

static void native_translate_coords (
    nbtk::t_native_widget *w, Window from_window, Window to_window,
    int from_x, int from_y, int *to_x, int *to_y) {
  if (!w || !w->app || !w->app->dpy || !to_x || !to_y)
    return;
  Window child = 0;
  XTranslateCoordinates (
      w->app->dpy, from_window, to_window, from_x, from_y, to_x, to_y, &child);
}

void native_register_wm_delete_window (nbtk::t_native_widget *w) {
  if (!w || !w->app || !w->app->dpy)
    return;
  if (!g_wm_delete_window)
    g_wm_delete_window = XInternAtom (w->app->dpy, "WM_DELETE_WINDOW", False);
  XSetWMProtocols (w->app->dpy, w->widget, &g_wm_delete_window, 1);
}

void native_set_window_attributes (nbtk::t_native_widget *w) {
  if (!w || !w->app || !w->app->dpy)
    return;
  XSetWindowAttributes attrs {};
  attrs.override_redirect = True;
  XChangeWindowAttributes (w->app->dpy, w->widget, CWOverrideRedirect, &attrs);
}

void native_set_transient_for_hint (nbtk::t_native_widget *parent, nbtk::t_native_widget *child) {
  if (!parent || !child || !child->app || !child->app->dpy)
    return;
  XSetTransientForHint (child->app->dpy, child->widget, parent->widget);
}

void native_set_input_mask (nbtk::t_native_widget *w) {
  if (!w || !w->app || !w->app->dpy)
    return;
  const long mask =
      ExposureMask |
      StructureNotifyMask |
      ButtonPressMask |
      ButtonReleaseMask |
      PointerMotionMask |
      EnterWindowMask |
      LeaveWindowMask |
      KeyPressMask |
      KeyReleaseMask;
  XSelectInput (w->app->dpy, w->widget, mask);
}

int native_key_from_event (void *event) {
  XKeyEvent *key = (XKeyEvent *) event;
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
    case XK_Page_Up:   return nbtk::KEY_PAGE_UP;
    case XK_Page_Down: return nbtk::KEY_PAGE_DOWN;
    case XK_space:     return nbtk::KEY_SPACE;
    case XK_Return:
    case XK_KP_Enter:  return nbtk::KEY_RETURN;
    case XK_Escape:    return nbtk::KEY_ESCAPE;
    default:           return nbtk::KEY_UNKNOWN;
  }
}

int native_key_mods_from_event (void *event) {
  XKeyEvent *key = (XKeyEvent *) event;
  if (!key)
    return 0;

  int mods = 0;
  if (key->state & ShiftMask)
    mods |= nbtk::KEYMOD_SHIFT;
  if (key->state & ControlMask)
    mods |= nbtk::KEYMOD_CTRL;
  if (key->state & Mod1Mask)
    mods |= nbtk::KEYMOD_ALT;
  return mods;
}

int native_text_from_event (void *event, char *text, int text_size) {
  XKeyEvent *key = (XKeyEvent *) event;
  if (!key || !text || text_size <= 0)
    return 0;

  text [0] = '\0';
  KeySym ignored = 0;
  const int len = XLookupString (key, text, text_size - 1, &ignored, NULL);
  if (len > 0)
    text [len] = '\0';
  return len;
}

int native_button_from_event (void *event) {
  XButtonEvent *button = (XButtonEvent *) event;
  return button ? (int) button->button : 0;
}

int native_event_x (void *event) {
  XEvent *xevent = (XEvent *) event;
  if (!xevent)
    return 0;

  switch (xevent->type) {
    case ButtonPress:
    case ButtonRelease:
      return xevent->xbutton.x;

    case MotionNotify:
      return xevent->xmotion.x;

    default:
      return 0;
  }
}

int native_event_y (void *event) {
  XEvent *xevent = (XEvent *) event;
  if (!xevent)
    return 0;

  switch (xevent->type) {
    case ButtonPress:
    case ButtonRelease:
      return xevent->xbutton.y;

    case MotionNotify:
      return xevent->xmotion.y;

    default:
      return 0;
  }
}

void native_childlist_add_child (nbtk::t_native_childlist *list, nbtk::t_native_widget *child) {
  if (list && child)
    list->children.push_back (child);
}

struct png_stream {
  const unsigned char *data = nullptr;
  size_t pos = 0;
};

static cairo_status_t read_png_stream (
    void *closure, unsigned char *data, unsigned int length) {
  png_stream *stream = (png_stream *) closure;
  std::memcpy (data, stream->data + stream->pos, length);
  stream->pos += length;
  return CAIRO_STATUS_SUCCESS;
}

cairo_surface_t *cairo_image_surface_create_from_stream (const unsigned char *data) {
  if (!data)
    return nullptr;
  png_stream stream { data, 0 };
  return cairo_image_surface_create_from_png_stream (read_png_stream, &stream);
}
