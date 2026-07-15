
/* NeuralBlender - RTNeural / NAM based amp modeler
 *
 * lv2 user interface. Mostly written by codex because i don't
 * like the lv2 api.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <thread>
#include <chrono>
#include <condition_variable>
#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/patch/patch.h>
#include <lv2/atom/util.h>
#include <lv2/urid/urid.h>
#include <lv2/state/state.h>
#include <lv2/atom/forge.h>

#include "neuralblender.h"
#include "data.h"

#define NB_URI "http://deimos.ca/neuralblender"
#define LV2_METER_FPS 30.0

enum {
  PORT_AUDIO_IN = 0,
  PORT_AUDIO_OUT,

  PORT_BYPASS,

  PORT_A_GAIN_IN,
  PORT_A_GAIN_OUT,
  PORT_A_DRY_OUT,
  PORT_A_DELAY,
  PORT_A_MUTE,
  PORT_A_DCFLIP,
  PORT_A_CALIBRATE,

  PORT_B_GAIN_IN,
  PORT_B_GAIN_OUT,
  PORT_B_DRY_OUT,
  PORT_B_DELAY,
  PORT_B_MUTE,
  PORT_B_DCFLIP,
  PORT_B_CALIBRATE,

  PORT_C_GAIN_IN,
  PORT_C_GAIN_OUT,
  PORT_C_DRY_OUT,
  PORT_C_DELAY,
  PORT_C_MUTE,
  PORT_C_DCFLIP,
  PORT_C_CALIBRATE,

  PORT_D_GAIN_IN,
  PORT_D_GAIN_OUT,
  PORT_D_DRY_OUT,
  PORT_D_DELAY,
  PORT_D_MUTE,
  PORT_D_DCFLIP,
  PORT_D_CALIBRATE,

  PORT_CONTROL,
	  PORT_NOTIFY,
	  PORT_VU_ENABLE,
	  PORT_MUTE_ALL,
	  PORT_EXCLUSIVE_LANE,
	  PORT_LINKED_CALIB,
	  PORT_CALIB_SOURCE,
	  PORT_CALIB_TARGET_DB,
	  PORT_NOISEGATE_ENABLED,
	  PORT_NOISEGATE_THRESHOLD,
	  PORT_NOISEGATE_ATTACK,
	  PORT_NOISEGATE_HOLD,
	  PORT_NOISEGATE_RELEASE,
    PORT_NOISEGATE_GAIN,
    PORT_TUNER_ON,
	  PORT_TUNER_BASE_FREQ,
	  PORT_TUNER_NOTE,
	  PORT_TUNER_CENTS_OFF,
	  PORT_TUNER_FREQ
};

