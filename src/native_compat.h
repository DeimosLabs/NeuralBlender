
/* NeuralBlender
 *
 * Temporary native X11 compatibility layer while nbtk sheds legacy names.
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

typedef Cursor OS_CURSOR;

typedef struct XColor_t XColor_t;
typedef struct SystrayColor_t SystrayColor_t;

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

typedef enum {
  NORMAL_,
  PRELIGHT_,
  SELECTED_,
  ACTIVE_,
  INSENSITIVE_,
} Color_state;

typedef enum {
  FORGROUND_,
  BACKGROUND_,
  BASE_,
  TEXT_,
  SHADOW_,
  FRAME_,
  LIGHT_,
} Color_mod;

typedef struct {
  double fg [4];
  double bg [4];
  double base [4];
  double text [4];
  double shadow [4];
  double frame [4];
  double light [4];
} Colors;

struct XColor_t {
  Colors normal;
  Colors prelight;
  Colors selected;
  Colors active;
  Colors insensitive;
};

struct SystrayColor_t {
  double r;
  double g;
  double b;
  double a;
};

struct nbtk::t_native_app {
  nbtk::t_native_childlist *childlist;
  Display *dpy;
  XColor_t *color_scheme;
  SystrayColor_t *systray_color;
  nbtk::t_native_widget *hold_grab;
  nbtk::t_native_widget *key_snooper;
  nbtk::t_native_widget *submenu;
  unsigned char *ctext;
  int small_font;
  int normal_font;
  int big_font;
  int csize;
  int dnd_version;
  bool run;
  bool is_grab;
  float hdpi;
  XIM xim;
};

struct nbtk::t_native_widget {
  char input_label[32];
  long long flags;
  const char *label;
  nbtk::t_native_app *app;
  XColor_t *color_scheme;
  Window widget;
  void *parent;
  void *parent_struct;
  void *private_struct;
  void *user_data;
  OS_CURSOR cursor;
  OS_CURSOR cursor2;
  Func_t func;
  cairo_surface_t *surface;
  cairo_t *cr;
  cairo_surface_t *buffer;
  cairo_t *crb;
  cairo_surface_t *image;
  void *adj_x;
  void *adj_y;
  void *adj;
  nbtk::t_native_childlist *childlist;
  XIC xic;
  Time double_click;
  int data;
  int state;
  int pos_x;
  int pos_y;
  int x;
  int y;
  int width;
  int height;
  Resize_t scale;
  xevfunc xpaste_callback;
  bool visible;
  bool mapped;
  bool owns_native_window;
  bool dirty;
};

void native_app_init (nbtk::t_native_app *app);
void native_app_shutdown (nbtk::t_native_app *app);
void native_app_run_events (nbtk::t_native_app *app);
void native_app_flush_dirty (nbtk::t_native_app *app);

nbtk::t_native_widget *native_create_window (nbtk::t_native_app *app, Window parent, int x, int y, int w, int h);
void native_widget_show (nbtk::t_native_widget *w);
void native_widget_show_all (nbtk::t_native_widget *w);
void native_widget_hide (nbtk::t_native_widget *w);
void native_widget_draw (nbtk::t_native_widget *w, void *user_data);
void native_widget_set_title (nbtk::t_native_widget *w, const char *title);
void native_widget_set_icon_from_png (nbtk::t_native_widget *w, const unsigned char *png);
void native_widget_invalidate (nbtk::t_native_widget *w);

Window native_get_root_window (nbtk::t_native_app *app, int flag);
void native_get_window_metrics (nbtk::t_native_widget *w, Metrics_t *metrics);
void native_set_window_min_size (
    nbtk::t_native_widget *w, int min_width, int min_height, int base_width, int base_height);
void native_move_window (Display *dpy, nbtk::t_native_widget *w, int x, int y);
void native_resize_window (Display *dpy, nbtk::t_native_widget *w, int w_, int h_);
void native_translate_coords (
    nbtk::t_native_widget *w, Window from_window, Window to_window,
    int from_x, int from_y, int *to_x, int *to_y);
void native_register_wm_delete_window (nbtk::t_native_widget *w);
void native_set_window_attributes (nbtk::t_native_widget *w);
void native_set_transient_for_hint (nbtk::t_native_widget *parent, nbtk::t_native_widget *child);
void native_set_input_mask (nbtk::t_native_widget *w);
int native_key_from_event (void *event);
int native_text_from_event (void *event, char *text, int text_size);

void native_childlist_add_child (nbtk::t_native_childlist *list, nbtk::t_native_widget *child);

void set_widget_color (
    nbtk::t_native_widget *w, Color_state st, Color_mod mod,
    double r, double g, double b, double a);
Colors *get_color_scheme (nbtk::t_native_widget *w, Color_state st);
void use_text_color_scheme (nbtk::t_native_widget *w, Color_state st);
void use_fg_color_scheme (nbtk::t_native_widget *w, Color_state st);
void use_bg_color_scheme (nbtk::t_native_widget *w, Color_state st);
void use_base_color_scheme (nbtk::t_native_widget *w, Color_state st);
void use_frame_color_scheme (nbtk::t_native_widget *w, Color_state st);
void use_light_color_scheme (nbtk::t_native_widget *w, Color_state st);
void use_shadow_color_scheme (nbtk::t_native_widget *w, Color_state st);

cairo_surface_t *cairo_image_surface_create_from_stream (const unsigned char *data);

void add_tooltip (nbtk::t_native_widget *w, const char *text);
void tooltip_set_text (nbtk::t_native_widget *w, const char *text);

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif
