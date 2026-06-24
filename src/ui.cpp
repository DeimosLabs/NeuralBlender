
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

// this one must be called AFTER add_* (Widget_t *, ...) in child create functions
void c_widget::create (
    c_neuralblender_ui *ui_,
    Widget_t *parent,
    const char *label_,
    int x, int y, int w, int h) {
    
  debug ("label_='%s'", label_);
  id = get_unique_id ();
  label = label_;
  ui = ui_;
  if (!ui) { debug ("!ui"); }
  if (!widget) {
    debug ("!widget");
    return;
  }
  
  widget->parent_struct = this;
  widget->label = label.c_str ();
}

void c_frame::create (
    c_neuralblender_ui *ui_,
    Widget_t *parent,
    const char *label_,
    int x, int y, int w, int h) {
    
  widget = add_frame (parent, label.c_str (), x, y, w, h);
  c_widget::create (ui_, parent, label_, x, y, w, h);
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
    int x, int y, int w, int h) {
    
  if (is_toggle) {
    widget = add_toggle_button (parent, label.c_str (), x, y, w, h);
    widget->func.value_changed_callback = button_value_changed;
  } else {
    widget = add_button (parent, label.c_str (), x, y, w, h);
    widget->func.double_click_callback = button_double_click;
    widget->func.button_release_callback = button_mouse_up;
  }
  c_widget::create (ui_, parent, label_, x, y, w, h);
}

void c_button::create (
    c_neuralblender_ui *ui_,
    Widget_t *parent,
    const char *label_,
    int x, int y, int w, int h, bool is_toggle_) {
    
  is_toggle = is_toggle_;
  create (ui_, parent, label_, x, y, w, h);
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
    break;
    
    case ROLE_BROWSE: CP
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
    
    default: CP
    break;
  }
}

/*bool c_button::value () {
  if (!widget || !widget->adj)
    return false;

  return adj_get_value (widget->adj) >= 0.5f;
}*/

bool c_button::set_value (bool value) {
  if (!widget || !widget->adj)
    return false;

  this->value = value;
  adj_set_value (widget->adj, this->value ? 1.0f : 0.0f);
  widget->state = this->value ? 3 : 0;
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

void c_combobox::clear () { CP
  items.clear ();
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
  debug ("value=%f", value);
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
  
  debug ("current_dir: '%s'", fp->current_dir.c_str ());
  
  fp->selected_file = std::string (filename);
  fp->current_dir = path_dirname (fp->selected_file);
  fp->scan_current_dir ();
  for (int i = 0; i < fp->filelist.size (); i++) {
    debug ("filelist [%d]: '%s'", i, fp->filelist [i].c_str ());
  }
  ui->load_model (cw->lane, filename);
  
  c_combobox *cb = &ui->lanes [lane].menu_list;
  cb->clear ();
  //cb->add (filename);
  fp->add_files_from_dir (cb);
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
  const char *path = current_dir.empty () ? "/usr/nam" : current_dir.c_str ();
  dialog = open_file_dialog (parent, path, ".nam|.json|.aidax");
}

void c_filepicker::hide () { CP
}

void c_filepicker::on_file_select (c_widget *cw, const std::string &filename) { CP
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

/*void c_filepicker::add_files_from_dir (c_combobox *cb) {
  CP
  int i, sel = -1;
  
  for (i = 0; i < filelist.size (); i++) {
    cb->add (filelist [i]);
    if (filelist [i] == selected_file) {
      sel = i;
      debug ("found selected: %d", i);
    }
  }
  cb->set_selection (sel);
}*/

void c_filepicker::add_files_from_dir(c_combobox *cb) {
  if (!cb)
    return;

  cb->items.clear();

  int sel = -1;

  for (size_t i = 0; i < filelist.size(); i++) {
    cb->items.push_back(filelist[i]);

    std::string full = current_dir;
    if (!full.empty() && full.back() != '/')
      full += '/';
    full += filelist[i];

    if (full == selected_file || filelist[i] == selected_file) {
      sel = (int) i;
      debug("found selected: %d", sel);
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
  }
  
  char buf [64];
  snprintf (buf, 63, "Build timestamp: %s", g_build_timestamp);

  labels [0].textsize = 1.5;
  labels [i].create (ui, w, buf, 0, 360, 400, 20);
  labels [i].textsize = 0.75;
  
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
  
  char label [64];
  ui = ui_;
  snprintf (label, 31, "Amp %c", 'A' + which);
  lane_id = which;
  lane_widget.create (ui, parent, label, x, y, w, h);
  Widget_t *wp = lane_widget.widget;
  
  menu_list.create (ui, wp, label, 104, 24, 330, 32);
  menu_list.widget->func.value_changed_callback = combobox_selected_callback;
  menu_list.lane = which;
  
  /*for (int i = 0; i < 5; i++) {
    snprintf (label, 31, "Item %d", i + 1);
    combobox_add_entry (menu_list.widget, label);
  }*/
  
  int knobs_left = w - 180;
  gain_in.create (ui, wp, "Input", knobs_left + 32, 36, 64, 64);
  gain_out.create (ui, wp, "Output", knobs_left + 90, 36, 64, 64);
  delay.create (ui, wp, "Delay", 32, 36, 64, 64);
  gain_in.role = ROLE_GAIN_IN;
  gain_out.role = ROLE_GAIN_IN;
  delay.role = ROLE_DELAY;
  gain_in.lane = gain_out.lane = delay.lane = which;
  gain_in.set_min (-40);
  gain_in.set_max (40);
  gain_in.set_defaultvalue (0);
  gain_in.set_value (0);
  gain_in.set_step (1);
  gain_out.role = ROLE_GAIN_OUT;
  gain_out.set_min (-40);
  gain_out.set_max (40);
  gain_out.set_defaultvalue (0);
  gain_out.set_value (0);
  gain_out.set_step (1);
  delay.role = ROLE_DELAY;
  delay.set_min (0);
  delay.set_max (30);
  delay.set_defaultvalue (0);
  delay.set_value (0);
  delay.set_step (0.01);
  
  btn_browse.create (ui, wp, "Browse...",  104, 70, 100, 40);
  btn_clear.create  (ui, wp, "Clear",     220, 70, 100, 40);
  btn_mute.create   (ui, wp, "Mute",      336, 70, 100, 40, true);
  btn_browse.lane = which;
  btn_clear.lane = which;
  btn_mute.lane = which;
  btn_browse.role = ROLE_BROWSE;
  btn_clear.role = ROLE_CLEAR;
  btn_mute.role = ROLE_MUTE;

  if (ui && which < NB_UI_MAX_LANES) {
    ui->filepickers [which].create (ui, btn_browse.widget, which, "Select file");
    btn_browse.filepicker = &ui->filepickers [which];
    btn_browse.lane = which;
    ui->filepickers [which].lane = which;
  }
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

bool c_neuralblender_ui::create (Window parent_) { CP
  destroy ();
  
  main_init (&app);
  ui_ready = true;
  app.small_font = 12 * app.hdpi;
  app.normal_font = 15 * app.hdpi;
  app.big_font = 20 * app.hdpi;
  display = app.dpy;
  
  parent = parent_;
  if (!parent)
    parent = DefaultRootWindow (display);

  main_widget = create_window (&app, parent, 0, 0, 640, 640);
  if (!main_widget)
    return false;

  widget_set_title (main_widget, "NeuralBlender");
  main_widget->func.expose_callback = draw_main_window;
  label_big.create (this, main_widget, "NeuralBlender", 120, 12, 400, 32);
  label_big.textsize = 1.5;
  btn_enable.create (this, main_widget, "Enabled",  20, 16, 120, 40, true);
  btn_enable.set_value (true);
  btn_enable.role = ROLE_BYPASS;
  btn_about.create (this, main_widget, "About...", 500, 16, 120, 40);
  btn_about.role = ROLE_ABOUT;
  aboutwindow.create (this);
  for (size_t i = 0; i < NB_UI_MAX_LANES; i++) {
    lanes [i].create (this, main_widget, i, 20, 64 + i * 140, 600, 130);
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

void c_neuralblender_ui::apply_state (const c_neuralblender_state &state) {
  if (!ui_ready)
    return;

  updating_from_state = true;

  const bool enabled = !state.bypass;
  btn_enable.value = enabled;
  btn_enable.set_value (enabled);
  btn_enable.set_label (enabled ? "Enabled" : "Bypass");

  const size_t nlanes = NB_UI_MAX_LANES < NB_MAX_MODELS ? NB_UI_MAX_LANES : NB_MAX_MODELS;
  for (size_t i = 0; i < nlanes; ++i) {
    const c_neuralblender_lane_state &lane = state.lanes [i];

    lanes [i].gain_in.set_value (gain_to_db (lane.gain_in));
    lanes [i].gain_out.set_value (gain_to_db (lane.gain_out));
    lanes [i].delay.set_value (lane.delay_ms);

    lanes [i].btn_mute.value = lane.lane_mute;
    lanes [i].btn_mute.set_value (lane.lane_mute);

    filepickers [i].selected_file = lane.filename;
    filepickers [i].current_dir = path_dirname (lane.filename);
    filepickers [i].scan_current_dir ();
    filepickers [i].add_files_from_dir (&lanes [i].menu_list);
  }

  updating_from_state = false;
}
