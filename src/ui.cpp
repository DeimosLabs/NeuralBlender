
/* NeuralBlender - RTNeural / NAM based amp modeler
 *
 * Shared UI code
 */
 
#include <string.h>
#include "ui.h"

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

// this one must be called AFTER add_* (Widget_t *, ...) in child create functions
void c_widget::create (Widget_t *parent, const char *label_,
                      int x, int y, int w, int h) {
  debug ("label_='%s'", label_);
  id = get_unique_id ();
  label = label_;
  if (!widget) {
    debug ("!widget");
    return;
  }
  
  widget->parent_struct = this;
}

void c_frame::create (Widget_t *parent, const char *label_,
                      int x, int y, int w, int h) {
  widget = add_frame (parent, label.c_str (), x, y, w, h);
  c_widget::create (parent, label_, x, y, w, h);
}

void c_label::create (Widget_t *parent, const char *label_,
                      int x, int y, int w, int h) {
  role = BTN_UNKNOWN;
  widget = add_label (parent, label.c_str (), x, y, w, h);
  widget->func.expose_callback = c_label::draw;
  c_widget::create (parent, label_, x, y, w, h);
}

void c_button::create (Widget_t *parent, const char *label_,
                       int x, int y, int w, int h) {
  if (is_toggle) {
    widget = add_toggle_button (parent, label.c_str (), x, y, w, h);
    widget->func.value_changed_callback = button_value_changed;
  } else {
    widget = add_button (parent, label.c_str (), x, y, w, h);
    widget->func.double_click_callback = button_double_click;
    widget->func.button_release_callback = button_mouse_up;
  }
  c_widget::create (parent, label_, x, y, w, h);
}

void c_button::create (Widget_t *parent, const char *label_,
                       int x, int y, int w, int h, bool is_toggle_) {
  is_toggle = is_toggle_;
  create (parent, label_, x, y, w, h);
}

void c_button::on_mouseup () {
  CP
}

bool c_button::value () {
  if (!widget || !widget->adj)
    return false;

  return adj_get_value (widget->adj) >= 0.5f;
}

bool c_button::set_value (bool value) {
  if (!widget || !widget->adj)
    return false;

  adj_set_value (widget->adj, value ? 1.0f : 0.0f);
  widget->state = value ? 3 : 0;
  expose_widget (widget);
  return true;
}

bool c_button::set_label (const char *label_) {
  label = label_ ? label_ : "";
  if (!widget)
    return false;

  widget->label = label.c_str ();
  expose_widget (widget);
  return true;
}

void c_knob::create (Widget_t *parent, const char *label_,
                     int x, int y, int w, int h) {
  label = label_;
  widget = add_knob (parent, label.c_str (), x, y, w, h);
  widget->func.value_changed_callback = knob_value_changed;
  widget->func.double_click_callback = knob_double_click;
  c_widget::create (parent, label_, x, y, w, h);
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
  value = x;
  adj_set_value (widget->adj, x);
}

void c_knob::set_step (float x) {
  step = widget->adj->step = x;
}

void c_knob::on_change () {
  debug ("value=%f", value);
}

void c_knob::on_doubleclick () { CP
  if (reset_on_doubleclick)
    set_value (defaultvalue);
}

void c_combobox::create (Widget_t *parent, const char *label_,
                         int x, int y, int w, int h) {
  label = label_;
  widget = add_combobox (parent, label.c_str (), x, y, w, h);
  combobox_set_menu_size (widget, 16);
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

  cairo_text_extents_t extents;
  use_text_color_scheme (w, get_color_state (w));
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

  cairo_set_source_rgb (w->crb, 0.12, 0.12, 0.12);
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

static void knob_double_click (void *w_, void *event, void *user_data) { CP
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
  debug ("value=%f", value);
  Widget_t *w = (Widget_t *) w_;
  if (!w || !w->parent_struct) {
    return;
  }

  c_button *b = (c_button *) w->parent_struct;
  b->toggle_value = value >= 0.5f;
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
  k->value = value;
  k->on_change ();
}

/*c_button::c_button () {
  widget = NULL;
}

c_button::c_button (
  Widget_t *parent,
  const char *label,
  int x,
  int y,
  int width,
  int height) {

  widget = NULL;
  create (parent, label, x, y, width, height);
}*/

/*Widget_t *c_button::create (
  Widget_t *parent,
  const char *label,
  int x,
  int y,
  int width,
  int height) {
  
  widget = add_button (parent, label, x, y, width, height);
  if (widget)
    widget->func.double_click_callback = button_double_click;

  return widget;
}*/

void c_lane_widgets::create (Widget_t *parent, size_t which,
                                  int x, int y, int w, int h) { CP
  char label [64];
  snprintf (label, 31, "Amp %c", 'A' + which);
  lane_id = which;
  lane_widget.create (parent, label, x, y, w, h);
  Widget_t *wp = lane_widget.widget;
  
  menu_list.create (wp, label, 32, 24, 400, 32);
  
  for (int i = 0; i < 50; i++) {
    snprintf (label, 31, "Item %d", i + 1);
    combobox_add_entry (menu_list.widget, label);
  }
  int knobs_left = w - 180;
  gain_in.create (wp, "Input", knobs_left + 32, 36, 64, 64);
  gain_out.create (wp, "Output", knobs_left + 90, 36, 64, 64);
  gain_in.set_min (-40);
  gain_in.set_max (40);
  gain_in.set_value (0);
  gain_in.set_step (1);
  gain_out.set_min (-40);
  gain_out.set_max (40);
  gain_out.set_value (0);
  gain_out.set_step (1);
  
  btn_browse.create (wp, "Browse...", 32, 70, 120, 40);
  btn_clear.create  (wp, "Clear",     160, 70, 120, 40);
  btn_mute.create   (wp, "Mute",      288, 70, 120, 40, true);
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

bool c_neuralblender_ui::create (Window parent) { CP
  destroy ();

  CP
  main_init (&app);
  ui_ready = true;
  app.small_font = 12 * app.hdpi;
  app.normal_font = 15 * app.hdpi;
  app.big_font = 20 * app.hdpi;
  display = app.dpy;
  
  if (!parent)
    parent = DefaultRootWindow (display);

  main_widget = create_window (&app, parent, 0, 0, 640, 640);
  if (!main_widget)
    return false;

  widget_set_title (main_widget, "NeuralBlender");
  main_widget->func.expose_callback = draw_main_window;
  label_big.create (main_widget, "NeuralBlender", 120, 12, 400, 32);
  label_big.textsize = 1.5;
  btn_enable.create (main_widget, "Enable",  20, 16, 120, 40, true);
  btn_enable.set_value (true);
  btn_about.create (main_widget, "About...", 500, 16, 120, 40);
  for (size_t i = 0; i < NB_UI_MAX_LANES; i++) {
    lanes [i].create (main_widget, i, 20, 64 + i * 140, 600, 130);
  }
  widget_show_all (main_widget);
  expose_widget (main_widget);

  window = main_widget->widget;
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

int c_neuralblender_ui::idle () {
  if (!ui_ready) {
    CP
    return 0;
  }

  run_embedded (&app);
  return 0;
}

void c_neuralblender_ui::draw () {
  if (!main_widget)
    return;

  widget_draw (main_widget, NULL);
}
