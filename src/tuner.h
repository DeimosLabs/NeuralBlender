
/* NeuralBlender - tuner widget.
 * Code for c_customwidget used for c_meter and here, originally written
 * for wxWidgets, translated to cairo by codex.
 */

#include "meter.h"


class c_tunerwidget : public c_customwidget {
  c_tunerwidget ();
  ~c_tunerwidget ();
  
  void create (Widget_t *parent,
               const char *label,
               int x, int y, int w, int h) override;
};
