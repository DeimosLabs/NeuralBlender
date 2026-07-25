#include "tk.h"

#include <algorithm>

namespace nbtk {

bool t_rect::contains (int px, int py) const {
  return px >= x && py >= y && px < x + w && py < y + h;
}

void c_widget::create (
    c_widget *parent_,
    const char *label_,
    int x_,
    int y_,
    int w_,
    int h_) {

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
  (void) button;
  return false;
}

bool c_widget::on_mouse_up (int x_, int y_, int button) {
  (void) x_;
  (void) y_;
  (void) button;
  return false;
}

bool c_widget::on_mouse_move (int x_, int y_) {
  (void) x_;
  (void) y_;
  return false;
}

bool c_widget::on_key_down (int key) {
  (void) key;
  return false;
}

bool c_widget::on_key_up (int key) {
  (void) key;
  return false;
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

bool c_widget::mouse_down_tree (int px, int py, int button) {
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

bool c_widget::mouse_up_tree (int px, int py, int button) {
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
  root.app = this;
  root.parent = nullptr;
  root.x = 0;
  root.y = 0;
  root.w = w;
  root.h = h;
}

void c_app::draw () {
  if (!cr)
    return;

  cairo_save (cr);
  root.draw_tree (cr);
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

  root.mouse_down_tree (x, y, button);
}

void c_app::dispatch_mouse_up (int x, int y, int button) {
  root.mouse_up_tree (x, y, button);
}

void c_app::dispatch_mouse_move (int x, int y) {
  root.mouse_move_tree (x, y);
}

void c_app::invalidate_rect (int x, int y, int w, int h) {
  if (main_window)
    main_window->invalidate_rect (x, y, w, h);
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
