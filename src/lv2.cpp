
/* NeuralBlender - RTNeural / NAM based amp modeler
 *
 * -----------------------------------------------------------------------------
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 * ----------------------------------------------------------------------------
 *
 * LV2 layer for NeuralBlender
 * Thanks to codex for help esp. on all the boilerplate code, of which this
 * file consists 99.5% entirely of.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <thread>
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

#define CMDLINE_IMPLEMENTATION
#define CMDLINE_DEBUG_COLOR ANSI_DARK_RED
#include "cmdline_debug.h"

enum {
  PORT_AUDIO_IN = 0,
  PORT_AUDIO_OUT,

  PORT_BYPASS,

  PORT_A_GAIN_IN,
  PORT_A_GAIN_OUT,
  PORT_A_DELAY,
  PORT_A_MUTE,
  PORT_A_DCFLIP,
  PORT_A_CALIBRATE,

  PORT_B_GAIN_IN,
  PORT_B_GAIN_OUT,
  PORT_B_DELAY,
  PORT_B_MUTE,
  PORT_B_DCFLIP,
  PORT_B_CALIBRATE,

  PORT_C_GAIN_IN,
  PORT_C_GAIN_OUT,
  PORT_C_DELAY,
  PORT_C_MUTE,
  PORT_C_DCFLIP,
  PORT_C_CALIBRATE,

  PORT_D_GAIN_IN,
  PORT_D_GAIN_OUT,
  PORT_D_DELAY,
  PORT_D_MUTE,
  PORT_D_DCFLIP,
  PORT_D_CALIBRATE,

  PORT_CONTROL,
  PORT_NOTIFY,
  PORT_VU_ENABLE,
  PORT_MUTE_ALL,
  PORT_EXCLUSIVE_LANE
};

typedef struct {
  // audio buffers
  const float *audio_in        = NULL;
  float *audio_out             = NULL;
  
  // controls
  const float *gain_in_db  [NB_MAX_MODELS] = { NULL };
  const float *gain_out_db [NB_MAX_MODELS] = { NULL };
  const float *delay       [NB_MAX_MODELS] = { NULL };
  const float *lane_mute   [NB_MAX_MODELS] = { NULL };
  const float *dcflip      [NB_MAX_MODELS] = { NULL };
  const float *calibrate   [NB_MAX_MODELS] = { NULL };
  const float *bypass      = NULL;
  const float *vu_enable   = NULL;
  const float *mute_all    = NULL;
  const float *exclusive_lane = NULL;
  
  float last_delay         [NB_MAX_MODELS] = { 0.0 };
  float last_gain_in_db    [NB_MAX_MODELS] = { 0.0 };
  float last_gain_out_db   [NB_MAX_MODELS] = { 0.0 };
  float last_lane_mute     [NB_MAX_MODELS] = { 0.0 };
  float last_dcflip        [NB_MAX_MODELS] = { 0.0 };
  float last_calibrate     [NB_MAX_MODELS] = { 0.0 };
  float last_bypass        = 1.0;
  float last_vu_enable     = 1.0;
  float last_mute_all      = 0.0;
  float last_exclusive_lane = 0.0;
  bool base_lane_mute [NB_MAX_MODELS] = { false };
  bool host_bypass = false;

  // dsp
  c_neuralblender blender;
  double samplerate            = 48000;
  uint32_t blocksize           = 64;
  
  // control port for filenames
  LV2_URID_Map *map            = NULL;

  LV2_URID urid_patch_Set      = 0;
  LV2_URID urid_patch_Get      = 0;
  LV2_URID urid_patch_property = 0;
  LV2_URID urid_patch_value    = 0;
  LV2_URID urid_atom_Path      = 0;
  LV2_URID urid_atom_String    = 0;
  LV2_URID urid_atom_Blank     = 0;
  LV2_URID urid_atom_Float     = 0;
  LV2_URID urid_atom_Vector    = 0;

  LV2_URID urid_model [NB_MAX_MODELS] = { 0 };
  LV2_URID urid_meters         = 0;
  LV2_URID urid_stats          = 0;
  LV2_URID urid_atom_URID      = 0;
  const LV2_Atom_Sequence *control = NULL;
  LV2_Atom_Sequence *notify    = NULL;
  
  // async file loading stuff
  std::thread loader_thread;
  std::mutex loader_mutex;
  std::condition_variable loader_cv;
  
  std::atomic<bool> loader_running { true };
  std::atomic<bool> load_requested { false };
  
  // hehe what a mess
  bool pending_calibrate[NB_MAX_MODELS] = { false };
  std::atomic<bool> pending_calib_enabled[NB_MAX_MODELS] = {};
  bool pending_load [NB_MAX_MODELS] = { false };
  //size_t pending_which         = 0;
  std::string pending_path [NB_MAX_MODELS];
  std::string current_model [NB_MAX_MODELS];
  
  // to get notified back when model is loaded from session restore
  LV2_Atom_Forge forge;
  LV2_URID urid_atom_Sequence  = 0;
  bool notify_path [NB_MAX_MODELS] = { false };
  std::string current_path [NB_MAX_MODELS];
  
  c_vudata meter_in;
  c_vudata meters_out [NB_MAX_MODELS];
  uint32_t meter_notify_samples = 0;
  std::atomic<bool> stats_dirty { true };
  std::atomic<bool> controls_dirty { false };
  
} Plugin;

static void apply_effective_controls (Plugin *self) {
  if (!self)
    return;

  const int exclusive_lane = (int) lrintf (self->last_exclusive_lane);
  const bool exclusive_on =
    exclusive_lane > 0 && exclusive_lane <= (int) NB_MAX_MODELS;
  const size_t excl = exclusive_on ? (size_t) (exclusive_lane - 1) : 0;
  const bool exclusive_empty =
    exclusive_on && !self->blender.amps [excl].loaded ();

  self->blender.set_bypass (self->host_bypass || exclusive_empty);

  for (size_t i = 0; i < NB_MAX_MODELS; ++i) {
    const bool mute =
      exclusive_on && !exclusive_empty ? i != excl : self->base_lane_mute [i];
    self->blender.set_lane_mute (i, mute);
  }
}

static void run_calibration (Plugin *self, size_t which, bool enabled) {
  if (!self || which >= NB_MAX_MODELS)
    return;

  self->blender.calib_on (which, enabled);

  if (enabled && self->blender.amps [which].loaded ()) {
    float *data = (float *) data_calib_f32;
    const size_t samples = data_calib_f32_len / sizeof (float);
    self->blender.amps [which].calibrate (data, samples);
  } else {
    self->blender.amps [which].calibrate (NULL, 0);
  }

  self->stats_dirty.store (true, std::memory_order_release);
}

// loader thread, also does calibration
// keeps these tasks OFF the dsp thread
static void loader_main (Plugin *self) { CP
  while (true) {
    size_t which = 0;
    bool do_load = false;
    bool do_calib = false;
    bool calib_enabled = false;
    std::string path;
    { // scope: only hold this lock while moving pending jobs into locals
      std::unique_lock<std::mutex> lock (self->loader_mutex);

      self->loader_cv.wait(lock, [&] {
        for (size_t i = 0; i < NB_MAX_MODELS; ++i)
          if (self->pending_load [i] || self->pending_calibrate [i])
            return true;
        return !self->loader_running;
      });

      if (!self->loader_running)
        break;

      for (size_t i = 0; i < NB_MAX_MODELS; ++i) {
        if (self->pending_load [i]) {
          which = i;
          path = self->pending_path[i];
          self->pending_load [i] = false;
          do_load = true;
          break;
        }
      }

      if (!do_load) {
        for (size_t i = 0; i < NB_MAX_MODELS; ++i) {
          if (self->pending_calibrate [i]) {
            which = i;
            calib_enabled = self->pending_calib_enabled [i].load (std::memory_order_acquire);
            self->pending_calibrate [i] = false;
            do_calib = true;
            break;
          }
        }
      }
    } // unlocks here

    if (do_load) {
      fprintf (stderr, "loader: load_model(%zu, \"%s\")\n", which, path.c_str ());
      self->load_requested = false;

      fprintf (stderr, "NeuralBlender: loading model %zu: %s\n",
               which, path.c_str ());

      if (self->blender.load_model (which, path.c_str ())) {
        self->current_model [which] = path;
        self->notify_path [which] = true;
      } else {
        self->current_model [which].clear ();
        self->notify_path [which] = true;
      }

      bool calib_after_load =
        self->calibrate [which] && *self->calibrate [which] >= 0.5f;
      self->pending_calib_enabled [which].store (calib_after_load, std::memory_order_release);
      {
        std::lock_guard<std::mutex> lock (self->loader_mutex);
        self->pending_calibrate [which] = false;
      }

      run_calibration (self, which, calib_after_load);
      self->controls_dirty.store (true, std::memory_order_release);
    }
    
    // here calibration runs in the loader thread
    if (do_calib) {
      run_calibration (self, which, calib_enabled);
      self->controls_dirty.store (true, std::memory_order_release);
    }
  }
}

// THIS RUNS IN DSP THREAD
static void request_load (Plugin *self, size_t which, const char *path) {
  if (!self || !path || !path [0] || which >= NB_MAX_MODELS)
    return;
    
  std::lock_guard<std::mutex> lock (self->loader_mutex);
  
  
  /* TODO: check this
  if (which == 0 && self->current_model_a == path)
    return;

  if (which == 1 && self->current_model_b == path)
    return;*/

  self->pending_load [which] = true;
  self->pending_path [which] = path;

	self->loader_cv.notify_one();
}

static void clear_model_slot (Plugin *self, size_t which, bool notify) {
  if (!self || which >= NB_MAX_MODELS)
    return;

  { // scope
    std::lock_guard<std::mutex> lock (self->loader_mutex);
    self->pending_load [which] = false;
    self->pending_path [which].clear ();
  }

  self->blender.unload_model (which);
  self->current_model [which].clear ();
  self->notify_path [which] = notify;
  self->stats_dirty.store (true, std::memory_order_release);
  apply_effective_controls (self);
}

static void get_state_path_features (
    const LV2_Feature *const *features,
    LV2_State_Map_Path **map_path,
    LV2_State_Free_Path **free_path) {
  if (map_path)
    *map_path = NULL;
  if (free_path)
    *free_path = NULL;

  for (int i = 0; features && features [i]; ++i) {
    if (map_path && !strcmp (features [i]->URI, LV2_STATE__mapPath))
      *map_path = (LV2_State_Map_Path *) features [i]->data;
    else if (free_path && !strcmp (features [i]->URI, LV2_STATE__freePath))
      *free_path = (LV2_State_Free_Path *) features [i]->data;
  }
}

static void request_calibrate (Plugin *self, size_t which, bool enabled) {
  if (!self || which >= NB_MAX_MODELS)
    return;

  {
    // scope
    std::lock_guard<std::mutex> lock (self->loader_mutex);
    self->pending_calibrate [which] = true;
    self->pending_calib_enabled [which].store (enabled, std::memory_order_release);
  }

  self->loader_cv.notify_one ();
}

static LV2_State_Status save (
    LV2_Handle instance,
    LV2_State_Store_Function store,
    LV2_State_Handle handle,
    uint32_t flags,
    const LV2_Feature *const *features) {
	    Plugin *self = (Plugin *) instance;
	    LV2_State_Map_Path *map_path = NULL;
	    LV2_State_Free_Path *free_path = NULL;
	    get_state_path_features (features, &map_path, &free_path);
	    
		    for (int i = 0; i < NB_MAX_MODELS; i++) {
		      const std::string filename = self->blender.amps [i].model_filename ();
		      const bool empty = filename.empty ();
		      char *abstract_path = NULL;
		      const char *stored_path = filename.c_str ();
		      if (!empty && map_path && map_path->abstract_path)
		        abstract_path = map_path->abstract_path (map_path->handle, filename.c_str ());
		      if (abstract_path)
		        stored_path = abstract_path;

	      store (handle,
	             self->urid_model [i],
	             stored_path,
	             strlen (stored_path) + 1,
	             empty ? self->urid_atom_String : self->urid_atom_Path,
	             LV2_STATE_IS_POD);

	      if (abstract_path && free_path && free_path->free_path)
	        free_path->free_path (free_path->handle, abstract_path);
		    }
    return LV2_STATE_SUCCESS;
}

static LV2_State_Status restore (
    LV2_Handle instance,
    LV2_State_Retrieve_Function retrieve,
    LV2_State_Handle handle,
    uint32_t flags,
    const LV2_Feature *const *features) { 
    
    Plugin *self = (Plugin *) instance;
    LV2_State_Map_Path *map_path = NULL;
    LV2_State_Free_Path *free_path = NULL;
    get_state_path_features (features, &map_path, &free_path);

    size_t size;
    uint32_t type;
    uint32_t valflags;

    for (int i = 0; i < NB_MAX_MODELS; i++) {
      const void *p =
          retrieve (handle,
                    self->urid_model [i],
                    &size,
                    &type,
                    &valflags);

		      if (p && type == self->urid_atom_Path && size > 1) {
		        const char *path = (const char *) p;
		        char *absolute_path = NULL;
		        if (map_path && map_path->absolute_path)
		          absolute_path = map_path->absolute_path (map_path->handle, path);

		        clear_model_slot (self, i, false);
		        request_load (self, i, absolute_path ? absolute_path : path);

		        if (absolute_path && free_path && free_path->free_path)
		          free_path->free_path (free_path->handle, absolute_path);
		      } else {
		        clear_model_slot (self, i, true);
		      }
		    }
    return LV2_STATE_SUCCESS;
}

static void forge_model_path_notify (Plugin *self,
                                     LV2_URID property,
                                     const char *path) {
  
  debug ("path=%s", path);
  LV2_Atom_Forge_Frame frame;

  lv2_atom_forge_frame_time (&self->forge, 0);

  lv2_atom_forge_object (&self->forge,
                         &frame,
                         0,
                         self->urid_patch_Set);

  lv2_atom_forge_key (&self->forge, self->urid_patch_property);
  lv2_atom_forge_urid (&self->forge, property);

  lv2_atom_forge_key (&self->forge, self->urid_patch_value);
  lv2_atom_forge_path (&self->forge, path, strlen(path) + 1);

  lv2_atom_forge_pop (&self->forge, &frame);
}

static void forge_meter_notify (Plugin *self) {
  float values [(1 + NB_MAX_MODELS) * 2];
  size_t n = 0;

  values [n++] = self->meter_in.linear_l ();
  values [n++] = self->meter_in.linear_peak_l ();

  for (int i = 0; i < NB_MAX_MODELS; i++) {
    values [n++] = self->meters_out [i].linear_l ();
    values [n++] = self->meters_out [i].linear_peak_l ();
  }

  LV2_Atom_Forge_Frame frame;
  lv2_atom_forge_frame_time (&self->forge, 0);
  lv2_atom_forge_object (&self->forge,
                         &frame,
                         0,
                         self->urid_patch_Set);

  lv2_atom_forge_key (&self->forge, self->urid_patch_property);
  lv2_atom_forge_urid (&self->forge, self->urid_meters);

  lv2_atom_forge_key (&self->forge, self->urid_patch_value);
  lv2_atom_forge_vector (&self->forge,
                         sizeof (float),
                         self->urid_atom_Float,
                         n,
                         values);

  lv2_atom_forge_pop (&self->forge, &frame);
}

static void forge_stats_notify (Plugin *self) {
  float values [NB_MAX_MODELS * 2];
  size_t n = 0;

  for (int i = 0; i < NB_MAX_MODELS; i++) {
    values [n++] = (float) self->blender.delays [i].frames ();
    values [n++] = self->blender.amps [i].trim.load (std::memory_order_acquire);
  }

  LV2_Atom_Forge_Frame frame;
  lv2_atom_forge_frame_time (&self->forge, 0);
  lv2_atom_forge_object (&self->forge,
                         &frame,
                         0,
                         self->urid_patch_Set);

  lv2_atom_forge_key (&self->forge, self->urid_patch_property);
  lv2_atom_forge_urid (&self->forge, self->urid_stats);

  lv2_atom_forge_key (&self->forge, self->urid_patch_value);
  lv2_atom_forge_vector (&self->forge,
                         sizeof (float),
                         self->urid_atom_Float,
                         n,
                         values);

  lv2_atom_forge_pop (&self->forge, &frame);
}

static LV2_Handle instantiate (const LV2_Descriptor *descriptor,
                               double rate,
                               const char *bundle_path,
                               const LV2_Feature *const *features) { CP
  Plugin *self = new Plugin {}; // init everything at NULL / 0
  CP
  self->samplerate = rate;
  self->blocksize = 0;
  
  self->blender.set_samplerate ((uint32_t)rate);
  self->meter_in.samplerate = (int) rate;
  self->meter_in.redraw_interval = 1.0f / LV2_METER_FPS;
  for (int i = 0; i < NB_MAX_MODELS; i++) {
    self->meters_out [i].samplerate = (int) rate;
    self->meters_out [i].redraw_interval = 1.0f / LV2_METER_FPS;
  }
  //self->blender.load_model (0, "/tmp/a.nam");
  //self->blender.load_model (1, "/tmp/b.nam");
  
  // control port
  if (features) {
    for (int i = 0; features [i]; i++) {
      if (!strcmp (features [i]->URI, LV2_URID__map)) {
        debug ("found URID map");
        self->map = (LV2_URID_Map *) features [i]->data;
      }
    }
  } else {
    debug ("features is NULL");
  }
  if (!self->map) {
    debug ("self->map is NULL");
    delete self;
    return NULL;
  }
  CP
  lv2_atom_forge_init(&self->forge, self->map);
  
  self->urid_patch_Set      = self->map->map (self->map->handle, LV2_PATCH__Set);
  self->urid_patch_Get      = self->map->map (self->map->handle, LV2_PATCH__Get);
  self->urid_patch_property = self->map->map (self->map->handle, LV2_PATCH__property);
  self->urid_patch_value    = self->map->map (self->map->handle, LV2_PATCH__value);
  self->urid_atom_Path      = self->map->map (self->map->handle, LV2_ATOM__Path);
  self->urid_atom_String    = self->map->map (self->map->handle, LV2_ATOM__String);
  self->urid_atom_URID      = self->map->map (self->map->handle, LV2_ATOM__URID);
  self->urid_atom_Blank     = self->map->map (self->map->handle, LV2_ATOM__Blank);
  self->urid_atom_Float     = self->map->map (self->map->handle, LV2_ATOM__Float);
  self->urid_atom_Vector    = self->map->map (self->map->handle, LV2_ATOM__Vector);
  
  self->urid_model [0] =
    self->map->map(self->map->handle, "http://deimos.ca/neuralblender#ModelA");
  
  self->urid_model [1] =
  self->map->map(self->map->handle, "http://deimos.ca/neuralblender#ModelB");
  
  self->urid_model [2] =
  self->map->map(self->map->handle, "http://deimos.ca/neuralblender#ModelC");
  
  self->urid_model [3] =
  self->map->map(self->map->handle, "http://deimos.ca/neuralblender#ModelD");

  self->urid_meters =
    self->map->map(self->map->handle, "http://deimos.ca/neuralblender#Meters");

  self->urid_stats =
    self->map->map(self->map->handle, "http://deimos.ca/neuralblender#Stats");

  self->blender.meter_in = &self->meter_in;
  for (int i = 0; i < NB_MAX_MODELS; i++)
    self->blender.meters_out [i] = &self->meters_out [i];
  
  // start loader thread LAST
  self->loader_running = true;
  self->loader_thread = std::thread (loader_main, self);
  CP
  return (LV2_Handle) self;
}

static void connect_port (LV2_Handle instance, uint32_t port, void* data) {
  Plugin *self = (Plugin *) instance;

  switch (port) {
    case PORT_AUDIO_IN:
      self->audio_in = (const float *) data;
      break;

    case PORT_AUDIO_OUT:
      self->audio_out = (float *) data;
      break;

    case PORT_BYPASS:
      self->bypass = (const float *) data;
      break;

    case PORT_A_GAIN_IN:
      self->gain_in_db [0] = (float *) data;
      break;

    case PORT_A_GAIN_OUT:
      self->gain_out_db [0] = (float *) data;
      break;

    case PORT_A_DELAY:
      self->delay [0] = (const float *) data;
      break;

    case PORT_A_MUTE:
      self->lane_mute [0] = (const float *) data;
      break;

    case PORT_A_DCFLIP:
      self->dcflip [0] = (const float *) data;
      break;

    case PORT_A_CALIBRATE:
      self->calibrate [0] = (const float *) data;
      break;

    case PORT_B_GAIN_IN:
      self->gain_in_db [1] = (float *) data;
      break;

    case PORT_B_GAIN_OUT:
      self->gain_out_db [1] = (float *) data;
      break;

    case PORT_B_DELAY:
      self->delay [1] = (const float *) data;
      break;

    case PORT_B_MUTE:
      self->lane_mute [1] = (const float *) data;
      break;

    case PORT_B_DCFLIP:
      self->dcflip [1] = (const float *) data;
      break;

    case PORT_B_CALIBRATE:
      self->calibrate [1] = (const float *) data;
      break;

    case PORT_C_GAIN_IN:
      self->gain_in_db [2] = (float *) data;
      break;

    case PORT_C_GAIN_OUT:
      self->gain_out_db [2] = (float *) data;
      break;

    case PORT_C_DELAY:
      self->delay [2] = (const float *) data;
      break;

    case PORT_C_MUTE:
      self->lane_mute [2] = (const float *) data;
      break;

    case PORT_C_DCFLIP:
      self->dcflip [2] = (const float *) data;
      break;

    case PORT_C_CALIBRATE:
      self->calibrate [2] = (const float *) data;
      break;

    case PORT_D_GAIN_IN:
      self->gain_in_db [3] = (float *) data;
      break;

    case PORT_D_GAIN_OUT:
      self->gain_out_db [3] = (float *) data;
      break;

    case PORT_D_DELAY:
      self->delay [3] = (const float *) data;
      break;

    case PORT_D_MUTE:
      self->lane_mute [3] = (const float *) data;
      break;

    case PORT_D_DCFLIP:
      self->dcflip [3] = (const float *) data;
      break;

    case PORT_D_CALIBRATE:
      self->calibrate [3] = (const float *) data;
      break;

    case PORT_CONTROL:
      self->control = (const LV2_Atom_Sequence *) data;
    break;
    
    case PORT_NOTIFY:
      self->notify = (LV2_Atom_Sequence *) data;
    break;
    
    case PORT_VU_ENABLE:
      self->vu_enable = (const float *) data;
    break;

    case PORT_MUTE_ALL:
      self->mute_all = (const float *) data;
    break;

    case PORT_EXCLUSIVE_LANE:
      self->exclusive_lane = (const float *) data;
    break;
  }
}

static void activate (LV2_Handle instance) {
  Plugin *self = (Plugin *) instance;

  for (size_t i = 0; i < NB_MAX_MODELS; ++i) {
    self->blender.delays [i].clear();
    self->blender.amps [i].reset();
  }
}

static void run (LV2_Handle instance, uint32_t nframes) {
  int i;
  Plugin *self = (Plugin *) instance;
  if (!self) return;
  
  if (self->notify) {
    LV2_Atom_Forge_Frame frame;

    lv2_atom_forge_set_buffer (
      &self->forge,
      (uint8_t *) self->notify,
      self->notify->atom.size);

    lv2_atom_forge_sequence_head (&self->forge, &frame, 0);
    
    // model loaded from UI?
    bool sent_path_notify = false;
    for (i = 0; i < NB_MAX_MODELS; i++) {
      if (self->notify_path [i]) {
        debug ("notify_path [%d]", i);
        forge_model_path_notify (
          self,
          self->urid_model [i],
          self->current_model [i].c_str());

        self->notify_path [i] = false;
        sent_path_notify = true;
        break; // throttle to 1 per cycle
      }
    }

    const bool sent_stats_notify =
      self->stats_dirty.exchange (false, std::memory_order_acq_rel);
    if (sent_stats_notify)
      forge_stats_notify (self);

    const uint32_t meter_interval =
      (uint32_t) (self->samplerate > 0.0 ? self->samplerate / LV2_METER_FPS : 1600.0);
    if (!sent_path_notify && !sent_stats_notify &&
        self->meter_notify_samples >= meter_interval) {
      forge_meter_notify (self);
      self->meter_notify_samples = 0;
    }

    lv2_atom_forge_pop (&self->forge, &frame);
  }
  
  // first parse any incoming atom stuff
  if (self->control) {
    LV2_ATOM_SEQUENCE_FOREACH (self->control, ev) {
      const LV2_Atom_Object *obj = (const LV2_Atom_Object *)&ev->body;
      
      if (obj->body.otype == self->urid_patch_Get) {
        for (i = 0; i < NB_MAX_MODELS; i++)
          self->notify_path [i] = true;
        self->stats_dirty.store (true, std::memory_order_release);

        continue;
      }

      if (obj->body.otype != self->urid_patch_Set)
        continue;

      const LV2_Atom *property = NULL;
      const LV2_Atom *value = NULL;

      lv2_atom_object_get (
        obj,
        self->urid_patch_property, &property,
        self->urid_patch_value, &value,
        0);

      if (!property || !value)
        continue;

      if (property->type != self->urid_atom_URID)
        continue;

      LV2_URID prop =
        ((const LV2_Atom_URID *) property)->body;

      if (value->type != self->urid_atom_Path &&
          value->type != self->urid_atom_String)
        continue;

      const char *path =
        (const char *) LV2_ATOM_BODY_CONST (value);
      
      for (i = 0; i < NB_MAX_MODELS; i++) {
        if (prop == self->urid_model [i]) {
          if (path && path [0])
            request_load (self, i, path);
          else
            clear_model_slot (self, i, true);
          break;
        }
      }
    }
  }
  
  if (nframes != self->blocksize) {
    self->blocksize = nframes;
    self->blender.set_blocksize(nframes);
    self->meter_in.bufsize = (int) nframes;
    for (i = 0; i < NB_MAX_MODELS; i++)
      self->meters_out [i].bufsize = (int) nframes;
  }

  if (self->bypass) {
    const float v = *self->bypass;
    if (v != self->last_bypass) { CP
      self->last_bypass = v;
      self->host_bypass = v < 0.5f;
      apply_effective_controls (self);
    }
  }

  if (self->vu_enable) {
    const float v = *self->vu_enable;
    if (v != self->last_vu_enable) { CP
      self->last_vu_enable = v;
      self->blender.do_vu = v >= 0.5f;
    }
  }

  if (self->mute_all) {
    const float v = *self->mute_all;
    if (v != self->last_mute_all) { CP
      self->last_mute_all = v;
      self->blender.mute_all = v >= 0.5f;
    }
  }

  if (self->exclusive_lane) {
    const float v = *self->exclusive_lane;
    if (v != self->last_exclusive_lane) { CP
      self->last_exclusive_lane = v;
      apply_effective_controls (self);
    }
  }
  
  // check all parameters
  for (i = 0; i < NB_MAX_MODELS; i++) {
    if (self->gain_in_db [i]) {
      const float v = *self->gain_in_db [i];
      if (v != self->last_gain_in_db [i]) { CP
        self->last_gain_in_db [i] = v;
        self->blender.set_gain_in (i, db_to_gain (v));
      }
    }
    
    if (self->gain_out_db [i]) {
      const float v = *self->gain_out_db [i];
      if (v != self->last_gain_out_db [i]) { CP
        self->last_gain_out_db [i] = v;
        self->blender.set_gain_out (i, db_to_gain (v));
      }
    }
    
    if (self->delay [i]) {
      const float v = *self->delay [i];
      if (v != self->last_delay [i]) { CP
        self->last_delay [i] = v;
        self->blender.set_delay_ms (i, v);
        self->stats_dirty.store (true, std::memory_order_release);
      }
    }

    if (self->lane_mute [i]) {
      const float v = *self->lane_mute [i];
      if (v != self->last_lane_mute [i]) { CP
        self->last_lane_mute [i] = v;
        self->base_lane_mute [i] = v >= 0.5f;
        apply_effective_controls (self);
      }
    }

    if (self->dcflip [i]) {
      const float v = *self->dcflip [i];
      if (v != self->last_dcflip [i]) { CP
        self->last_dcflip [i] = v;
        self->blender.dcflip (i, v >= 0.5f);
      }
    }

    if (self->calibrate [i]) {
      const float v = *self->calibrate [i];
      const bool enabled = v >= 0.5f;
      self->pending_calib_enabled [i].store (enabled, std::memory_order_release);
      if (v != self->last_calibrate [i]) { CP
        self->last_calibrate [i] = v;
        request_calibrate (self, i, enabled);
      }
    }
  }

  if (self->controls_dirty.exchange (false, std::memory_order_acq_rel))
    apply_effective_controls (self);

  // actual DSP
  if (self->audio_in && self->audio_out) {
    self->blender.process_block ((float *) self->audio_in, self->audio_out, nframes);
  }

  self->meter_notify_samples += nframes;
}

static void deactivate (LV2_Handle instance) {
}

static void cleanup (LV2_Handle instance) {
  Plugin *self = (Plugin *) instance;
  if (!self)
    return;

  // scope again: release asap
  {
    std::lock_guard<std::mutex> lock(self->loader_mutex);
    self->loader_running = false;
    self->load_requested = false;
  }

  self->loader_cv.notify_one();

  if (self->loader_thread.joinable())
    self->loader_thread.join();

  delete self;
}

static const LV2_State_Interface state_interface = {
  save,
  restore
};

static const void* extension_data (const char* uri) {
  if (!strcmp (uri, LV2_STATE__interface))
    return &state_interface;

  return NULL;  return NULL;
}

static const LV2_Descriptor descriptor = {
  NB_URI,
  instantiate,
  connect_port,
  activate,
  run,
  deactivate,
  cleanup,
  extension_data
};

extern "C" const LV2_Descriptor* lv2_descriptor (uint32_t index) {
  switch (index) {
    case 0:   return &descriptor;
    default:  return NULL;
  }
}
