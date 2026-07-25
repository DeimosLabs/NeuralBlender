
/* NeuralBlender
 *
 * Minimal X11/Cairo backend compatibility layer for nbtk.
 */

#include <algorithm>
#include <cstring>
#include <vector>

#include "native_compat.h"

struct nbtk::t_native_childlist {
  std::vector<nbtk::t_native_widget *> children;
};

static Atom g_wm_delete_window = 0;

static void init_colors (XColor_t *scheme) {
  if (!scheme)
    return;

  auto init = [] (Colors &c) {
    const double fg[4] = { 0.85, 0.85, 0.85, 1.0 };
    const double bg[4] = { 0.13, 0.13, 0.13, 1.0 };
    const double base[4] = { 0.10, 0.10, 0.10, 1.0 };
    const double text[4] = { 0.95, 0.95, 0.95, 1.0 };
    const double shadow[4] = { 0.02, 0.02, 0.02, 1.0 };
    const double frame[4] = { 0.45, 0.45, 0.45, 1.0 };
    const double light[4] = { 0.75, 0.75, 0.75, 1.0 };
    std::memcpy (c.fg, fg, sizeof (fg));
    std::memcpy (c.bg, bg, sizeof (bg));
    std::memcpy (c.base, base, sizeof (base));
    std::memcpy (c.text, text, sizeof (text));
    std::memcpy (c.shadow, shadow, sizeof (shadow));
    std::memcpy (c.frame, frame, sizeof (frame));
    std::memcpy (c.light, light, sizeof (light));
  };

  init (scheme->normal);
  init (scheme->prelight);
  init (scheme->selected);
  init (scheme->active);
  init (scheme->insensitive);
}

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

void main_init (nbtk::t_native_app *app) {
  if (!app)
    return;

  std::memset (app, 0, sizeof (*app));
  app->dpy = XOpenDisplay (nullptr);
  app->run = app->dpy != nullptr;
  app->hdpi = 1.0f;
  app->small_font = 12;
  app->normal_font = 14;
  app->big_font = 20;
  app->childlist = new nbtk::t_native_childlist;
  app->color_scheme = new XColor_t;
  init_colors (app->color_scheme);
  if (app->dpy)
    g_wm_delete_window = XInternAtom (app->dpy, "WM_DELETE_WINDOW", False);
}

void main_quit (nbtk::t_native_app *app) {
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

  delete app->color_scheme;
  app->color_scheme = nullptr;

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
        widget_draw (w, nullptr);
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

void draw_dirty_widgets (nbtk::t_native_app *app) {
  if (!app || !app->childlist)
    return;

  bool drew = false;
  for (nbtk::t_native_widget *w : app->childlist->children) {
    if (!w || !w->dirty || !w->visible)
      continue;

    w->dirty = false;
    widget_draw (w, nullptr);
    drew = true;
  }

  if (drew && app->dpy)
    XFlush (app->dpy);
}

void run_embedded (nbtk::t_native_app *app) {
  if (!app || !app->dpy)
    return;

  while (XPending (app->dpy) > 0) {
    XEvent event;
    XNextEvent (app->dpy, &event);
    dispatch_event (app, event);
  }

  draw_dirty_widgets (app);
}

nbtk::t_native_widget *create_window (nbtk::t_native_app *app, Window parent, int x, int y, int w_, int h_) {
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
  w->color_scheme = app->color_scheme;
  w->visible = false;
  w->owns_native_window = true;
  w->dirty = true;

  const int screen = DefaultScreen (app->dpy);
  const unsigned long bg = BlackPixel (app->dpy, screen);
  w->widget = XCreateSimpleWindow (
      app->dpy, parent, x, y, w->width, w->height, 0, bg, bg);

  os_set_input_mask (w);
  childlist_add_child (app->childlist, w);
  return w;
}

void widget_show (nbtk::t_native_widget *w) {
  if (!w || !w->app || !w->app->dpy)
    return;
  XMapWindow (w->app->dpy, w->widget);
  w->visible = true;
  w->dirty = true;
}

void widget_show_all (nbtk::t_native_widget *w) {
  widget_show (w);
}

void widget_hide (nbtk::t_native_widget *w) {
  if (!w || !w->app || !w->app->dpy)
    return;
  XUnmapWindow (w->app->dpy, w->widget);
  w->visible = false;
}

void widget_draw (nbtk::t_native_widget *w, void *user_data) {
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

void widget_set_title (nbtk::t_native_widget *w, const char *title) {
  if (!w || !w->app || !w->app->dpy)
    return;
  XStoreName (w->app->dpy, w->widget, title ? title : "");
}

void widget_set_icon_from_png (nbtk::t_native_widget *w, const unsigned char *png) {
  (void) w;
  (void) png;
}

void expose_widget (nbtk::t_native_widget *w) {
  if (!w)
    return;

  w->dirty = true;
}

Window os_get_root_window (nbtk::t_native_app *app, int flag) {
  (void) flag;
  return app && app->dpy ? DefaultRootWindow (app->dpy) : 0;
}

void os_get_window_metrics (nbtk::t_native_widget *w, Metrics_t *metrics) {
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

  if (w->app && w->app->dpy && w->widget) {
    XWindowAttributes attr;
    if (XGetWindowAttributes (w->app->dpy, w->widget, &attr)) {
      metrics->width = attr.width;
      metrics->height = attr.height;
      metrics->x = attr.x;
      metrics->y = attr.y;
      metrics->visible = attr.map_state == IsViewable;
    }
  }
}

void os_set_window_min_size (
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

void os_move_window (Display *dpy, nbtk::t_native_widget *w, int x, int y) {
  if (!dpy || !w)
    return;
  w->x = x;
  w->y = y;
  XMoveWindow (dpy, w->widget, x, y);
}

void os_resize_window (Display *dpy, nbtk::t_native_widget *w, int w_, int h_) {
  if (!dpy || !w)
    return;
  w->width = std::max (1, w_);
  w->height = std::max (1, h_);
  destroy_cairo (w);
  XResizeWindow (dpy, w->widget, w->width, w->height);
}

void os_translate_coords (
    nbtk::t_native_widget *w, Window from_window, Window to_window,
    int from_x, int from_y, int *to_x, int *to_y) {
  if (!w || !w->app || !w->app->dpy || !to_x || !to_y)
    return;
  Window child = 0;
  XTranslateCoordinates (
      w->app->dpy, from_window, to_window, from_x, from_y, to_x, to_y, &child);
}

void os_register_wm_delete_window (nbtk::t_native_widget *w) {
  if (!w || !w->app || !w->app->dpy)
    return;
  if (!g_wm_delete_window)
    g_wm_delete_window = XInternAtom (w->app->dpy, "WM_DELETE_WINDOW", False);
  XSetWMProtocols (w->app->dpy, w->widget, &g_wm_delete_window, 1);
}

void os_set_window_attrb (nbtk::t_native_widget *w) {
  if (!w || !w->app || !w->app->dpy)
    return;
  XSetWindowAttributes attrs {};
  attrs.override_redirect = True;
  XChangeWindowAttributes (w->app->dpy, w->widget, CWOverrideRedirect, &attrs);
}

void os_set_transient_for_hint (nbtk::t_native_widget *parent, nbtk::t_native_widget *child) {
  if (!parent || !child || !child->app || !child->app->dpy)
    return;
  XSetTransientForHint (child->app->dpy, child->widget, parent->widget);
}

void os_set_input_mask (nbtk::t_native_widget *w) {
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

void childlist_add_child (nbtk::t_native_childlist *list, nbtk::t_native_widget *child) {
  if (list && child)
    list->children.push_back (child);
}

static Colors *colors_for_state (XColor_t *scheme, Color_state st) {
  if (!scheme)
    return nullptr;
  switch (st) {
    case PRELIGHT_: return &scheme->prelight;
    case SELECTED_: return &scheme->selected;
    case ACTIVE_: return &scheme->active;
    case INSENSITIVE_: return &scheme->insensitive;
    case NORMAL_:
    default: return &scheme->normal;
  }
}

static double *color_slot (Colors *colors, Color_mod mod) {
  if (!colors)
    return nullptr;
  switch (mod) {
    case FORGROUND_: return colors->fg;
    case BACKGROUND_: return colors->bg;
    case BASE_: return colors->base;
    case TEXT_: return colors->text;
    case SHADOW_: return colors->shadow;
    case FRAME_: return colors->frame;
    case LIGHT_: return colors->light;
    default: return colors->fg;
  }
}

void set_widget_color (
    nbtk::t_native_widget *w, Color_state st, Color_mod mod,
    double r, double g, double b, double a) {
  Colors *colors = get_color_scheme (w, st);
  double *slot = color_slot (colors, mod);
  if (!slot)
    return;
  slot[0] = r;
  slot[1] = g;
  slot[2] = b;
  slot[3] = a;
}

Colors *get_color_scheme (nbtk::t_native_widget *w, Color_state st) {
  XColor_t *scheme = w && w->color_scheme
    ? w->color_scheme
    : (w && w->app ? w->app->color_scheme : nullptr);
  return colors_for_state (scheme, st);
}

static void use_color (nbtk::t_native_widget *w, Color_state st, Color_mod mod) {
  if (!w || !w->crb)
    return;
  Colors *colors = get_color_scheme (w, st);
  double *slot = color_slot (colors, mod);
  if (slot)
    cairo_set_source_rgba (w->crb, slot[0], slot[1], slot[2], slot[3]);
}

void use_text_color_scheme (nbtk::t_native_widget *w, Color_state st) { use_color (w, st, TEXT_); }
void use_fg_color_scheme (nbtk::t_native_widget *w, Color_state st) { use_color (w, st, FORGROUND_); }
void use_bg_color_scheme (nbtk::t_native_widget *w, Color_state st) { use_color (w, st, BACKGROUND_); }
void use_base_color_scheme (nbtk::t_native_widget *w, Color_state st) { use_color (w, st, BASE_); }
void use_frame_color_scheme (nbtk::t_native_widget *w, Color_state st) { use_color (w, st, FRAME_); }
void use_light_color_scheme (nbtk::t_native_widget *w, Color_state st) { use_color (w, st, LIGHT_); }
void use_shadow_color_scheme (nbtk::t_native_widget *w, Color_state st) { use_color (w, st, SHADOW_); }

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

void add_tooltip (nbtk::t_native_widget *w, const char *text) {
  (void) w;
  (void) text;
}

void tooltip_set_text (nbtk::t_native_widget *w, const char *text) {
  (void) w;
  (void) text;
}
