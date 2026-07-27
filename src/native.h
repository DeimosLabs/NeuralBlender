
/* NeuralBlender
 *
 * Native platform interface for nbtk.
 */

#pragma once

#include <stdbool.h>
#include <cairo.h>
#include <cairo-xlib.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <X11/Xatom.h>

namespace nbtk {
struct t_native_childlist;
struct t_native_widget;
struct t_native_app;

using t_native_handle = t_native_widget *;
using t_native_window = Window;
using t_native_display = Display *;
}

typedef void (*xevfunc)(void *widget, void *user_data);
typedef void (*evfunc)(void *widget, void *event, void *user_data);

typedef struct {
  xevfunc expose_callback;
  xevfunc configure_callback;
  xevfunc enter_callback;
  xevfunc leave_callback;
  xevfunc adj_callback;
  xevfunc value_changed_callback;
  xevfunc user_callback;
  xevfunc mem_free_callback;
  xevfunc configure_notify_callback;
  xevfunc resize_notify_callback;
  xevfunc map_notify_callback;
  xevfunc unmap_notify_callback;
  xevfunc dialog_callback;
  xevfunc dnd_notify_callback;
  xevfunc visibiliy_change_callback;

  evfunc button_press_callback;
  evfunc button_release_callback;
  evfunc double_click_callback;
  evfunc motion_callback;
  evfunc key_press_callback;
  evfunc key_release_callback;
  evfunc user_paste_callback;
} Func_t;

typedef enum {
  NORTHWEST,
  NORTHEAST,
  NORTCENTER,
  SOUTHWEST,
  SOUTHEAST,
  SOUTHCENTER,
  EASTWEST,
  EASTNORTH,
  EASTSOUTH,
  EASTCENTER,
  WESTNORTH,
  WESTSOUTH,
  WESTEAST,
  WESTCENTER,
  CENTER,
  ASPECT,
  FIXEDSIZE,
  MENUITEM,
  NONE,
} Gravity;

typedef struct {
  Gravity gravity;
  int init_x;
  int init_y;
  int init_width;
  int init_height;
  float scale_x;
  float scale_y;
  float cscale_x;
  float cscale_y;
  float rcscale_x;
  float rcscale_y;
  float ascale;
} Resize_t;

enum {
  IS_WIDGET        = 1 << 0,
  IS_WINDOW        = 1 << 1,
  IS_POPUP         = 1 << 2,
  IS_RADIO         = 1 << 3,
  IS_TOOLTIP       = 1 << 4,
  USE_TRANSPARENCY = 1 << 5,
  HAS_FOCUS        = 1 << 6,
  HAS_POINTER      = 1 << 7,
  HAS_TOOLTIP      = 1 << 8,
  HAS_MEM          = 1 << 9,
  NO_AUTOREPEAT    = 1 << 10,
  FAST_REDRAW      = 1 << 11,
  HIDE_ON_DELETE   = 1 << 12,
  REUSE_IMAGE      = 1 << 13,
  NO_PROPAGATE     = 1 << 14,
  IS_SUBMENU       = 1 << 15,
  DONT_PROPAGATE   = 1 << 16,
  MOUSE_CAPTURE    = 1 << 17,
};

typedef struct {
  int width;
  int height;
  int x;
  int y;
  bool visible;
} Metrics_t;

struct nbtk::t_native_app {
  nbtk::t_native_childlist *childlist;
  Display *dpy;
  bool run;
  float hdpi;
};

struct nbtk::t_native_widget {
  long long flags;
  const char *label;
  nbtk::t_native_app *app;
  Window widget;
  void *parent;
  void *parent_struct;
  Func_t func;
  cairo_surface_t *surface;
  cairo_t *cr;
  cairo_surface_t *buffer;
  cairo_t *crb;
  cairo_surface_t *image;
  void *adj;
  nbtk::t_native_childlist *childlist;
  int data;
  int state;
  int x;
  int y;
  int width;
  int height;
  Resize_t scale;
  xevfunc xpaste_callback;
  bool visible;
  bool owns_native_window;
  bool dirty;
};

void native_app_init (nbtk::t_native_app *app);
void native_app_shutdown (nbtk::t_native_app *app);
void native_app_run_events (nbtk::t_native_app *app);
void native_app_flush_dirty (nbtk::t_native_app *app);

nbtk::t_native_widget *native_create_window (nbtk::t_native_app *app, Window parent, int x, int y, int w, int h);
void native_widget_show (nbtk::t_native_widget *w);
void native_widget_hide (nbtk::t_native_widget *w);
void native_widget_draw (nbtk::t_native_widget *w, void *user_data);
void native_widget_set_title (nbtk::t_native_widget *w, const char *title);
void native_widget_set_icon_from_png (nbtk::t_native_widget *w, const unsigned char *png);
void native_widget_invalidate (nbtk::t_native_widget *w);

Window native_get_root_window (nbtk::t_native_app *app, int flag);
void native_get_window_metrics (nbtk::t_native_widget *w, Metrics_t *metrics);
void native_set_window_min_size (
    nbtk::t_native_widget *w, int min_width, int min_height, int base_width, int base_height);
void native_register_wm_delete_window (nbtk::t_native_widget *w);
void native_set_window_attributes (nbtk::t_native_widget *w);
void native_set_transient_for_hint (nbtk::t_native_widget *parent, nbtk::t_native_widget *child);
void native_set_input_mask (nbtk::t_native_widget *w);
int native_key_from_event (void *event);
int native_text_from_event (void *event, char *text, int text_size);
int native_button_from_event (void *event);
int native_event_x (void *event);
int native_event_y (void *event);

void native_childlist_add_child (nbtk::t_native_childlist *list, nbtk::t_native_widget *child);

cairo_surface_t *cairo_image_surface_create_from_stream (const unsigned char *data);

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif
