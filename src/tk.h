#pragma once

#include <memory>
#include <string>
#include <vector>

#include <cairo/cairo.h>

namespace nbtk {

struct t_point {
  int x = 0;
  int y = 0;
};

struct t_rect {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;

  bool contains (int px, int py) const;
};

class c_app;
class c_widget;
class c_nativewindow;
class c_popupwindow;
class c_menu;
class c_tooltip;

class c_widget {
public:
  virtual ~c_widget () = default;

  virtual void create (
      c_widget *parent,
      const char *label,
      int x,
      int y,
      int w,
      int h);

  virtual void draw (cairo_t *cr);
  virtual bool on_mouse_down (int x, int y, int button);
  virtual bool on_mouse_up (int x, int y, int button);
  virtual bool on_mouse_move (int x, int y);
  virtual bool on_key_down (int key);
  virtual bool on_key_up (int key);

  void draw_tree (cairo_t *cr);
  bool mouse_down_tree (int x, int y, int button);
  bool mouse_up_tree (int x, int y, int button);
  bool mouse_move_tree (int x, int y);

  t_point local_to_root (t_point p) const;
  t_point root_to_local (t_point p) const;
  t_point local_to_screen (t_point p) const;

  t_rect rect () const;
  bool contains_local (int px, int py) const;
  void invalidate ();
  void invalidate_rect (int x, int y, int w, int h);

  c_app *app = nullptr;
  c_widget *parent = nullptr;
  std::vector<c_widget *> children;

  std::string label;
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
  bool visible = true;
  bool enabled = true;
};

class c_app {
public:
  virtual ~c_app () = default;

  virtual void create (int w, int h);
  virtual void draw ();
  virtual void dispatch_mouse_down (int x, int y, int button);
  virtual void dispatch_mouse_up (int x, int y, int button);
  virtual void dispatch_mouse_move (int x, int y);
  virtual void invalidate_rect (int x, int y, int w, int h);

  virtual std::unique_ptr<c_popupwindow> create_popup (c_widget *owner);
  virtual t_point root_to_screen (t_point p) const;
  virtual t_point screen_to_root (t_point p) const;

  cairo_t *cr = nullptr;
  c_nativewindow *main_window = nullptr;
  c_widget root;
  std::vector<std::unique_ptr<c_popupwindow>> popups;
};

class c_nativewindow {
public:
  virtual ~c_nativewindow () = default;

  virtual void create (c_app *app, int x, int y, int w, int h);
  virtual void show ();
  virtual void hide ();
  virtual void move_resize (int x, int y, int w, int h);
  virtual void invalidate_rect (int x, int y, int w, int h);

  virtual cairo_t *begin_paint ();
  virtual void end_paint ();
  virtual void on_paint (cairo_t *cr);
  virtual void on_close ();
  virtual bool on_key_down (int key);
  virtual bool on_key_up (int key);
  virtual bool on_mouse_down (int x, int y, int button);
  virtual bool on_mouse_up (int x, int y, int button);
  virtual bool on_mouse_move (int x, int y);

  virtual t_point local_to_screen (t_point p) const;
  virtual t_point screen_to_local (t_point p) const;

  c_app *app = nullptr;
  c_widget root;
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
  bool visible = false;
};

class c_toplevelwindow : public c_nativewindow {
public:
  void on_close () override;
};

class c_embeddedwindow : public c_nativewindow {
public:
  void create_for_parent (c_app *app, void *native_parent, int w, int h);

  void *native_parent = nullptr;
};

class c_popupwindow : public c_nativewindow {
public:
  void create_for_owner (c_app *app, c_widget *owner, int w, int h);
  virtual void show_at_screen_pos (int x, int y);
  virtual void close ();
  virtual bool close_on_outside_click () const;
  virtual bool takes_focus () const;

  c_widget *owner = nullptr;
};

class c_menu : public c_popupwindow {
public:
  bool close_on_outside_click () const override;
  bool takes_focus () const override;
  bool on_key_down (int key) override;
  bool on_mouse_down (int x, int y, int button) override;
};

class c_tooltip : public c_popupwindow {
public:
  bool close_on_outside_click () const override;
  bool takes_focus () const override;
  void set_text (const char *text);
};

} // namespace nbtk
