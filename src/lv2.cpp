
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
#include <chrono>
#include <condition_variable>
#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/patch/patch.h>
#include <lv2/atom/util.h>
#include <lv2/urid/urid.h>
#include <lv2/state/state.h>
#include <lv2/atom/forge.h>

#include "lv2.h"
#include "neuralblender.h"
//#include "data.h"

#define NB_URI "http://deimos.ca/neuralblender"
#define LV2_METER_FPS 30.0

#define CMDLINE_IMPLEMENTATION
#define CMDLINE_DEBUG_COLOR ANSI_DARK_RED
#include "cmdline_debug.h"


struct Plugin : public c_lv2_urids {
  // audio buffers
  const float *audio_in     = NULL;
  float *audio_out          = NULL;
  
  // controls
  const float *gain_in_db   [BANK_COUNT] [NB_NUM_MODELS] = {};
  const float *ir_pitch     [BANK_COUNT] [NB_NUM_MODELS] = {};
  const float *gain_out_db  [BANK_COUNT] [NB_NUM_MODELS] = {};
  const float *dry_out_db   [BANK_COUNT] [NB_NUM_MODELS] = {};
  const float *delay        [BANK_COUNT] [NB_NUM_MODELS] = {};
  const float *lane_mute    [BANK_COUNT] [NB_NUM_MODELS] = {};
  const float *dcflip       [BANK_COUNT] [NB_NUM_MODELS] = {};
  const float *calibrate    [BANK_COUNT] [NB_NUM_MODELS] = {};
  const float *bypass       = NULL;
  const float *vu_enable    = NULL;
  const float *mute_all     = NULL;
  const float *exclusive_lane [BANK_COUNT] = { NULL };
  const float *linked_calib [BANK_COUNT] = { NULL };
  const float *calib_source = NULL;
  const float *calib_target_db = NULL;
  const float *noisegate_enabled = NULL;
  const float *noisegate_threshold = NULL;
  const float *noisegate_attack = NULL;
  const float *noisegate_hold = NULL;
  const float *noisegate_release = NULL;
  const float *tuner_on = NULL;
  const float *tuner_base_freq = NULL;
  const float *master_gain = NULL;
  const float *presence = NULL;
  float *noisegate_gain = NULL;
  float *tuner_note = NULL;
  float *tuner_cents_off = NULL;
  float *tuner_freq = NULL;
  
  float last_delay          [BANK_COUNT] [NB_NUM_MODELS] = {};
  float last_gain_in_db     [BANK_COUNT] [NB_NUM_MODELS] = {};
  float last_ir_pitch       [BANK_COUNT] [NB_NUM_MODELS] = {};
  float last_gain_out_db    [BANK_COUNT] [NB_NUM_MODELS] = {};
  float last_dry_out_db     [BANK_COUNT] [NB_NUM_MODELS] = {};
  float last_lane_mute      [BANK_COUNT] [NB_NUM_MODELS] = {};
  float last_dcflip         [BANK_COUNT] [NB_NUM_MODELS] = {};
  float last_calibrate      [BANK_COUNT] [NB_NUM_MODELS] = {};
  float last_bypass         = 1.0;
  float last_vu_enable      = 1.0;
  float last_mute_all       = 0.0;
  float last_exclusive_lane [BANK_COUNT] = { 0.0 };
  float last_linked_calib   [BANK_COUNT] = { 0.0 };
  float last_calib_source   = 0.0;
  float last_calib_target_db = DB_CALIB_TARGET_DEFAULT;
  float last_noisegate_enabled = 0.0;
  float last_noisegate_threshold = -60.0;
  float last_noisegate_attack = 2.0;
  float last_noisegate_hold = 10.0;
  float last_noisegate_release = 20.0;
  float last_tuner_on = 0.0;
  float last_tuner_base_freq = 440.0;
  float last_master_gain = 0.0;
  float last_presence = 0.0;
  bool base_lane_mute [BANK_COUNT] [NB_NUM_MODELS] = {};
  bool host_bypass = false;

  // dsp
  c_neuralblender blender;
  double samplerate            = 48000;
  uint32_t blocksize           = 64;
  
  const LV2_Atom_Sequence *control = NULL;
  LV2_Atom_Sequence *notify    = NULL;
  
  // async file loading stuff
  std::thread loader_thread;
  std::mutex loader_mutex;
  std::condition_variable loader_cv;
  
  std::atomic<bool> loader_running { true };
  std::atomic<bool> load_requested { false };
  
  // hehe what a mess
  bool pending_calibrate [BANK_COUNT] [NB_NUM_MODELS] = {};
  bool pending_calibrate_all [BANK_COUNT] = {};
  std::atomic<bool> pending_calib_enabled [BANK_COUNT] [NB_NUM_MODELS] = {};
  bool pending_ir_pitch [BANK_COUNT] [NB_NUM_MODELS] = {};
  float pending_ir_pitch_value [BANK_COUNT] [NB_NUM_MODELS] = {};
  bool pending_load [BANK_COUNT] [NB_NUM_MODELS] = {};
  std::string pending_path [BANK_COUNT] [NB_NUM_MODELS];
  std::string current_model [BANK_COUNT] [NB_NUM_MODELS];
  
  // to get notified back when model is loaded from session restore
  bool notify_path [BANK_COUNT] [NB_NUM_MODELS] = {};
  std::string current_path [BANK_COUNT] [NB_NUM_MODELS];
  bool restored_from_state = false;
  
  c_vudata meter_in [BANK_COUNT];
  c_vudata meters_out [BANK_COUNT] [NB_NUM_MODELS];
  uint32_t meter_notify_samples = 0;
  std::atomic<bool> stats_dirty { true };
  std::atomic<bool> controls_dirty { false };
  std::atomic<bool> tuner_enabled { false };
  std::atomic<float> detected_tuner_freq { 0.0f };
  std::atomic<float> detected_tuner_note { 0.0f };
  std::atomic<float> detected_tuner_cents { 0.0f };
  
};

static bool read_changed (const float *port, float &last, float &value) {
  if (!port)
    return false;

  value = *port;
  if (value == last)
    return false;

  last = value;
  return true;
}

static bool read_changed_bool (const float *port, float &last, bool &value) {
  float f = 0.0f;
  if (!read_changed (port, last, f))
    return false;

  value = f >= 0.5f;
  return true;
}

static bool calib_enabled_for_lane (
    Plugin *self, _lane_bank bank, size_t which) {
  if (!self || bank < BANK_PEDAL || bank >= BANK_COUNT ||
      which >= NB_NUM_MODELS)
    return false;

  if (self->calibrate [bank] [which])
    return *self->calibrate [bank] [which] >= 0.5f;

  return self->blender.banks [bank].lanes [which].do_calib;
}

static void apply_effective_controls (Plugin *self) {
  if (!self)
    return;

  self->blender.set_bypass (self->host_bypass);

  for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
    const _lane_bank b = (_lane_bank) bank;
    const int exclusive_lane = (int) lrintf (self->last_exclusive_lane [bank]);
    self->blender.banks [bank].exclusive_lane =
      std::clamp (exclusive_lane, 0, (int) NB_NUM_MODELS);
    const bool exclusive_on =
      exclusive_lane > 0 && exclusive_lane <= (int) NB_NUM_MODELS;
    const size_t excl = exclusive_on ? (size_t) (exclusive_lane - 1) : 0;
    const bool exclusive_empty =
      exclusive_on && !self->blender.banks [bank].lanes [excl].loaded ();

    for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
      const bool mute =
        exclusive_on && !exclusive_empty ? i != excl : self->base_lane_mute [bank] [i];
      self->blender.set_lane_mute (b, i, mute);
    }
  }
}

static void run_calibration (
    Plugin *self, _lane_bank bank, size_t which, bool enabled) {
  if (!self || bank < BANK_PEDAL || bank >= BANK_COUNT ||
      which >= NB_NUM_MODELS)
    return;

  self->blender.calib_on (bank, which, enabled);

  if (self->blender.banks [bank].linked_calib)
    self->blender.calibrate_linked (bank, self->blender.calib_source == 1);
  else
    self->blender.calibrate (bank, which, self->blender.calib_source == 1);

  self->stats_dirty.store (true, std::memory_order_release);
}

static void run_linked_calibration (Plugin *self, _lane_bank bank) {
  if (!self || bank < BANK_PEDAL || bank >= BANK_COUNT)
    return;

  self->blender.calibrate_linked (bank, self->blender.calib_source == 1);
  self->stats_dirty.store (true, std::memory_order_release);
}

// loader thread, also does calibration and tuner analysis / pitch tracking
// keeps these tasks OFF the dsp thread
static void loader_main (Plugin *self) { CP
  while (true) {
    _lane_bank bank = BANK_AMP;
    size_t which = 0;
    bool do_load = false;
    bool do_calib = false;
    bool do_calib_all = false;
    bool calib_enabled = false;
    bool do_ir_pitch = false;
    float ir_pitch_value = 0.0f;
    bool do_tuner = false;
    std::string path;
    { // scope: only hold this lock while moving pending jobs into locals
      std::unique_lock<std::mutex> lock (self->loader_mutex);

      self->loader_cv.wait_for(lock, std::chrono::milliseconds (50), [&] {
        for (size_t b = BANK_PEDAL; b < BANK_COUNT; ++b) {
          if (self->pending_calibrate_all [b])
            return true;
          for (size_t i = 0; i < NB_NUM_MODELS; ++i)
            if (self->pending_load [b] [i] ||
                self->pending_calibrate [b] [i] ||
                self->pending_ir_pitch [b] [i])
              return true;
        }
        return !self->loader_running;
      });

      if (!self->loader_running)
        break;

      for (size_t b = BANK_PEDAL; b < BANK_COUNT && !do_load; ++b) {
        for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
          if (self->pending_load [b] [i]) {
            bank = (_lane_bank) b;
            which = i;
            path = self->pending_path [b] [i];
            self->pending_load [b] [i] = false;
            do_load = true;
            break;
          }
        }
      }

      if (!do_load) {
        for (size_t b = BANK_PEDAL; b < BANK_COUNT; ++b) {
          if (self->pending_calibrate_all [b]) {
            bank = (_lane_bank) b;
            self->pending_calibrate_all [b] = false;
            do_calib_all = true;
            break;
          }
        }
      }

      if (!do_load && !do_calib_all) {
        for (size_t b = BANK_PEDAL; b < BANK_COUNT && !do_calib; ++b) {
          for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
            if (self->pending_calibrate [b] [i]) {
              bank = (_lane_bank) b;
              which = i;
              calib_enabled =
                self->pending_calib_enabled [b] [i].load (
                  std::memory_order_acquire);
              self->pending_calibrate [b] [i] = false;
              do_calib = true;
              break;
            }
          }
        }
      }

      if (!do_load && !do_calib_all && !do_calib) {
        for (size_t b = BANK_PEDAL; b < BANK_COUNT && !do_ir_pitch; ++b) {
          for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
            if (self->pending_ir_pitch [b] [i]) {
              bank = (_lane_bank) b;
              which = i;
              ir_pitch_value = self->pending_ir_pitch_value [b] [i];
              self->pending_ir_pitch [b] [i] = false;
              do_ir_pitch = true;
              break;
            }
          }
        }
      }

      if (!do_load && !do_calib_all && !do_calib && !do_ir_pitch)
        do_tuner = self->tuner_enabled.load (std::memory_order_acquire);
    } // unlocks here

    if (do_load) {
      fprintf (stderr, "loader: load_model(%d, %zu, \"%s\")\n",
               (int) bank, which, path.c_str ());
      self->load_requested = false;

      fprintf (stderr, "NeuralBlender: loading model %d:%zu: %s\n",
               (int) bank, which, path.c_str ());

      if (self->blender.load_model (bank, which, path.c_str ())) {
        self->current_model [bank] [which] = path;
        self->notify_path [bank] [which] = true;
      } else {
        self->current_model [bank] [which].clear ();
        self->notify_path [bank] [which] = true;
      }

      bool calib_after_load =
        calib_enabled_for_lane (self, bank, which);
      self->pending_calib_enabled [bank] [which].store (
        calib_after_load, std::memory_order_release);
      {
        std::lock_guard<std::mutex> lock (self->loader_mutex);
        self->pending_calibrate [bank] [which] = false;
      }

      run_calibration (self, bank, which, calib_after_load);
      self->controls_dirty.store (true, std::memory_order_release);
    }
    
    // here calibration runs in the loader thread
    if (do_calib) {
      run_calibration (self, bank, which, calib_enabled);
      self->controls_dirty.store (true, std::memory_order_release);
    }

    if (do_calib_all) {
      run_linked_calibration (self, bank);
      self->controls_dirty.store (true, std::memory_order_release);
    }

    if (do_ir_pitch) {
      self->blender.set_ir_pitch (bank, which, ir_pitch_value);
      if (self->blender.banks [bank].lanes [which].do_calib)
        run_calibration (self, bank, which, true);
      self->stats_dirty.store (true, std::memory_order_release);
      self->controls_dirty.store (true, std::memory_order_release);
    }

    if (do_tuner) {
      self->blender.pitchtracker.analyze ();
      self->detected_tuner_freq.store (
        self->blender.pitchtracker.detected_freq.load (std::memory_order_acquire),
        std::memory_order_release);
      self->detected_tuner_note.store (
        self->blender.pitchtracker.detected_note.load (std::memory_order_acquire),
        std::memory_order_release);
      self->detected_tuner_cents.store (
        self->blender.pitchtracker.detected_cents.load (std::memory_order_acquire),
        std::memory_order_release);
    }
  }
}

// THIS RUNS IN DSP THREAD
static void request_load (
    Plugin *self, _lane_bank bank, size_t which, const char *path) {
  if (!self || !path || !path [0] ||
      bank < BANK_PEDAL || bank >= BANK_COUNT ||
      which >= NB_NUM_MODELS)
    return;
    
  std::lock_guard<std::mutex> lock (self->loader_mutex);
  
  
  /* TODO: check this
  if (which == 0 && self->current_model_a == path)
    return;

  if (which == 1 && self->current_model_b == path)
    return;*/

  self->pending_load [bank] [which] = true;
  self->pending_path [bank] [which] = path;
  self->pending_calibrate_all [bank] = false;

	self->loader_cv.notify_one();
}

static void request_calibrate_linked (Plugin *self, _lane_bank bank);

static void request_ir_pitch (
    Plugin *self, _lane_bank bank, size_t which, float semitones) {
  if (!self || bank < BANK_PEDAL || bank >= BANK_COUNT ||
      which >= NB_NUM_MODELS)
    return;

  std::lock_guard<std::mutex> lock (self->loader_mutex);
  self->pending_ir_pitch [bank] [which] = true;
  self->pending_ir_pitch_value [bank] [which] =
    std::clamp (semitones, -12.0f, 12.0f);
  self->loader_cv.notify_one ();
}

static void clear_model_slot (
    Plugin *self, _lane_bank bank, size_t which, bool notify) {
  if (!self || bank < BANK_PEDAL || bank >= BANK_COUNT ||
      which >= NB_NUM_MODELS)
    return;

  { // scope
    std::lock_guard<std::mutex> lock (self->loader_mutex);
    self->pending_load [bank] [which] = false;
    self->pending_path [bank] [which].clear ();
    self->pending_calibrate [bank] [which] = false;
    self->pending_calibrate_all [bank] = false;
  }

  self->blender.unload_model (bank, which);
  self->current_model [bank] [which].clear ();
  self->notify_path [bank] [which] = notify;
  self->stats_dirty.store (true, std::memory_order_release);
  apply_effective_controls (self);
  if (self->blender.banks [bank].linked_calib)
    request_calibrate_linked (self, bank);
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

static void request_calibrate (
    Plugin *self, _lane_bank bank, size_t which, bool enabled) {
  if (!self || bank < BANK_PEDAL || bank >= BANK_COUNT ||
      which >= NB_NUM_MODELS)
    return;

  {
    // scope
    std::lock_guard<std::mutex> lock (self->loader_mutex);
    self->pending_calibrate [bank] [which] = true;
    self->pending_calib_enabled [bank] [which].store (
      enabled, std::memory_order_release);
  }

  self->loader_cv.notify_one ();
}

static void request_calibrate_linked (Plugin *self, _lane_bank bank) {
  if (!self || bank < BANK_PEDAL || bank >= BANK_COUNT)
    return;

  {
    std::lock_guard<std::mutex> lock (self->loader_mutex);
    self->pending_calibrate_all [bank] = true;
  }

  self->loader_cv.notify_one ();
}

static void set_calib_target_db (Plugin *self, float db) {
  if (!self)
    return;

  const float old_db = self->blender.banks [BANK_AMP].lanes [0].calib_target_db;
  self->blender.set_calib_target_db (db);
  const bool changed = self->blender.banks [BANK_AMP].lanes [0].calib_target_db != old_db;
  if (!changed)
    return;

  self->last_calib_target_db = self->blender.banks [BANK_AMP].lanes [0].calib_target_db;

  for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
    const _lane_bank b = (_lane_bank) bank;
    if (self->blender.banks [bank].linked_calib) {
      request_calibrate_linked (self, b);
    } else {
      for (size_t i = 0; i < NB_NUM_MODELS; i++) {
        if (calib_enabled_for_lane (self, b, i))
          request_calibrate (self, b, i, true);
      }
    }
  }
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
    
    for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
      for (int i = 0; i < NB_NUM_MODELS; i++) {
        const std::string filename =
          self->blender.banks [bank].lanes [i].model_filename ();
        const bool empty = filename.empty ();
        char *abstract_path = NULL;
        const char *stored_path = filename.c_str ();
        if (!empty && map_path && map_path->abstract_path)
          abstract_path =
            map_path->abstract_path (map_path->handle, filename.c_str ());
        if (abstract_path)
          stored_path = abstract_path;

      store (handle,
             self->urid_bank_model [bank] [i],
             stored_path,
             strlen (stored_path) + 1,
             empty ? self->urid_atom_String : self->urid_atom_Path,
             LV2_STATE_IS_POD);

      if (abstract_path && free_path && free_path->free_path)
        free_path->free_path (free_path->handle, abstract_path);
        }
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

  const void *calib_bass =
    retrieve (handle,
              self->urid_calib_bass,
              &size,
              &type,
              &valflags);
  if (calib_bass && type == self->urid_atom_Int && size >= sizeof (int32_t))
    self->blender.calib_source = (*(const int32_t *) calib_bass) != 0 ? 1 : 0;
  self->restored_from_state = true;

  for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
    for (int i = 0; i < NB_NUM_MODELS; i++) {
      const void *p =
        retrieve (handle,
                self->urid_bank_model [bank] [i],
                &size,
                &type,
                &valflags);

      if (p && type == self->urid_atom_Path && size > 1) {
        const char *path = (const char *) p;
        char *absolute_path = NULL;
        if (map_path && map_path->absolute_path)
          absolute_path =
            map_path->absolute_path (map_path->handle, path);

        clear_model_slot (self, (_lane_bank) bank, i, false);
        request_load (self, (_lane_bank) bank, i,
                      absolute_path ? absolute_path : path);

        if (absolute_path && free_path && free_path->free_path)
          free_path->free_path (free_path->handle, absolute_path);
      } else {
          clear_model_slot (self, (_lane_bank) bank, i, true);
      }
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

static void forge_int_notify (Plugin *self, LV2_URID property, int32_t value) {
  LV2_Atom_Forge_Frame frame;

  lv2_atom_forge_frame_time (&self->forge, 0);
  lv2_atom_forge_object (&self->forge,
                         &frame,
                         0,
                         self->urid_patch_Set);

  lv2_atom_forge_key (&self->forge, self->urid_patch_property);
  lv2_atom_forge_urid (&self->forge, property);

  lv2_atom_forge_key (&self->forge, self->urid_patch_value);
  lv2_atom_forge_int (&self->forge, value);

  lv2_atom_forge_pop (&self->forge, &frame);
}

static void forge_meter_notify (Plugin *self) {
  float values [BANK_COUNT * (1 + NB_NUM_MODELS) * 2];
  size_t n = 0;

  for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
    values [n++] = self->meter_in [bank].linear_l ();
    values [n++] = self->meter_in [bank].linear_peak_l ();

    for (int i = 0; i < NB_NUM_MODELS; i++) {
      values [n++] = self->meters_out [bank] [i].linear_l ();
      values [n++] = self->meters_out [bank] [i].linear_peak_l ();
    }
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
  float values [BANK_COUNT * NB_NUM_MODELS * NB_STATS_PER_LANE];
  size_t n = 0;

  for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
    for (size_t i = 0; i < NB_NUM_MODELS; i++) {
      c_neuralamp &lane = self->blender.banks [bank].lanes [i];
      values [n++] = (float) lane.delay.frames ();
      values [n++] = lane.trim.load (std::memory_order_acquire);
      values [n++] = (float) lane.engine ();
    }
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
  self->last_calib_target_db = self->blender.banks [BANK_AMP].lanes [0].calib_target_db;
  for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
    self->meter_in [bank].samplerate = (int) rate;
    self->meter_in [bank].redraw_interval = 1.0f / LV2_METER_FPS;
    for (int i = 0; i < NB_NUM_MODELS; i++) {
      self->meters_out [bank] [i].samplerate = (int) rate;
      self->meters_out [bank] [i].redraw_interval = 1.0f / LV2_METER_FPS;
    }
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
  if (!self->init (self->map)) {
    debug ("failed to initialize LV2 URIDs");
    delete self;
    return NULL;
  }

  for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
    self->blender.banks [bank].meter_in = &self->meter_in [bank];
    for (int i = 0; i < NB_NUM_MODELS; i++)
      self->blender.banks [bank].meters_out [i] =
        &self->meters_out [bank] [i];
  }
  
  // start loader thread LAST
  self->loader_running = true;
  self->loader_thread = std::thread (loader_main, self);
  CP
  return (LV2_Handle) self;
}

static void connect_port (LV2_Handle instance, uint32_t port, void* data) {
	Plugin *self = (Plugin *) instance;

  _lane_bank bank = BANK_AMP;
  size_t lane = 0;
  uint32_t param = 0;
  if (nb_lv2_decode_bank_lane_port (port, &bank, &lane, &param)) {
    switch (param) {
      case NB_LV2_LANE_GAIN_IN:
        self->gain_in_db [bank] [lane] = (const float *) data;
      break;

      case NB_LV2_LANE_IR_PITCH:
        self->ir_pitch [bank] [lane] = (const float *) data;
      break;

      case NB_LV2_LANE_GAIN_OUT:
        self->gain_out_db [bank] [lane] = (const float *) data;
      break;

      case NB_LV2_LANE_DRY_OUT:
        self->dry_out_db [bank] [lane] = (const float *) data;
      break;

      case NB_LV2_LANE_DELAY:
        self->delay [bank] [lane] = (const float *) data;
      break;

      case NB_LV2_LANE_MUTE:
        self->lane_mute [bank] [lane] = (const float *) data;
      break;

      case NB_LV2_LANE_DCFLIP:
        self->dcflip [bank] [lane] = (const float *) data;
      break;

      case NB_LV2_LANE_CALIBRATE:
        self->calibrate [bank] [lane] = (const float *) data;
      break;
    }
    return;
  }

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

    case PORT_EXCLUSIVE_LANE_PEDAL:
      self->exclusive_lane [BANK_PEDAL] = (const float *) data;
    break;

    case PORT_EXCLUSIVE_LANE_AMP:
      self->exclusive_lane [BANK_AMP] = (const float *) data;
    break;

    case PORT_EXCLUSIVE_LANE_CAB:
      self->exclusive_lane [BANK_CAB] = (const float *) data;
    break;

    case PORT_LINKED_CALIB_PEDAL:
      self->linked_calib [BANK_PEDAL] = (const float *) data;
    break;

    case PORT_LINKED_CALIB_AMP:
      self->linked_calib [BANK_AMP] = (const float *) data;
    break;

    case PORT_LINKED_CALIB_CAB:
      self->linked_calib [BANK_CAB] = (const float *) data;
    break;

    case PORT_CALIB_SOURCE:
      self->calib_source = (const float *) data;
    break;

    case PORT_CALIB_TARGET_DB:
      self->calib_target_db = (const float *) data;
    break;

    case PORT_NOISEGATE_ENABLED:
      self->noisegate_enabled = (const float *) data;
    break;

    case PORT_NOISEGATE_THRESHOLD:
      self->noisegate_threshold = (const float *) data;
    break;

    case PORT_NOISEGATE_ATTACK:
      self->noisegate_attack = (const float *) data;
    break;

    case PORT_NOISEGATE_HOLD:
      self->noisegate_hold = (const float *) data;
    break;

    case PORT_NOISEGATE_RELEASE:
      self->noisegate_release = (const float *) data;
    break;

    case PORT_TUNER_ON:
      self->tuner_on = (const float *) data;
    break;

    case PORT_TUNER_BASE_FREQ:
      self->tuner_base_freq = (const float *) data;
    break;

    case PORT_MASTER_GAIN:
      self->master_gain = (const float *) data;
    break;

    case PORT_PRESENCE:
      self->presence = (const float *) data;
    break;

    case PORT_NOISEGATE_GAIN:
      self->noisegate_gain = (float *) data;
    break;

    case PORT_TUNER_NOTE:
      self->tuner_note = (float *) data;
    break;

    case PORT_TUNER_CENTS_OFF:
      self->tuner_cents_off = (float *) data;
    break;

    case PORT_TUNER_FREQ:
      self->tuner_freq = (float *) data;
    break;
  }
}

static void activate (LV2_Handle instance) {
  Plugin *self = (Plugin *) instance;

  for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
    self->blender.banks [BANK_AMP].lanes [i].reset();
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
    for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
      for (i = 0; i < NB_NUM_MODELS; i++) {
        if (self->notify_path [bank] [i]) {
          debug ("notify_path [%d][%d]", (int) bank, i);
          forge_model_path_notify (
            self,
            self->urid_bank_model [bank] [i],
            self->current_model [bank] [i].c_str());

          self->notify_path [bank] [i] = false;
          sent_path_notify = true;
          break; // throttle to 1 per cycle
        }
      }
      if (sent_path_notify)
        break;
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
	        for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank)
	          for (i = 0; i < NB_NUM_MODELS; i++)
	            self->notify_path [bank] [i] = true;
	        self->restored_from_state = false;
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

	      if (prop == self->urid_calib_target_db) {
	        if (value->type == self->urid_atom_Float)
	          set_calib_target_db (self, ((const LV2_Atom_Float *) value)->body);
	        continue;
	      }

			      if (value->type != self->urid_atom_Path &&
		          value->type != self->urid_atom_String)
		        continue;

      const char *path =
        (const char *) LV2_ATOM_BODY_CONST (value);
      
      for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
        for (i = 0; i < NB_NUM_MODELS; i++) {
          if (prop == self->urid_bank_model [bank] [i]) {
            if (path && path [0])
              request_load (self, (_lane_bank) bank, i, path);
            else
              clear_model_slot (self, (_lane_bank) bank, i, true);
            break;
          }
        }
      }
    }
  }
  
  if (self->blocksize == 0 || nframes > self->blocksize) {
    self->blocksize = nframes;
    self->blender.set_blocksize(nframes);
  }

  {
    for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
      self->meter_in [bank].bufsize = (int) nframes;
      for (i = 0; i < NB_NUM_MODELS; i++)
        self->meters_out [bank] [i].bufsize = (int) nframes;
    }
  }

  float v = 0.0f;
  bool b = false;

  if (read_changed (self->bypass, self->last_bypass, v)) { CP
    self->host_bypass = v < 0.5f;
    apply_effective_controls (self);
  }

  if (read_changed_bool (self->vu_enable, self->last_vu_enable, b)) { CP
    self->blender.do_vu = b;
  }

  if (read_changed_bool (self->mute_all, self->last_mute_all, b)) { CP
    self->blender.mute_all = b;
  }

  for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
    if (read_changed (
          self->exclusive_lane [bank], self->last_exclusive_lane [bank], v)) { CP
      self->blender.set_exclusive_lane ((_lane_bank) bank, (int) lrintf (v));
      if (bank == BANK_AMP)
        apply_effective_controls (self);
    }
  }

  for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
    if (read_changed_bool (
          self->linked_calib [bank], self->last_linked_calib [bank], b)) { CP
      self->blender.banks [bank].linked_calib = b;
      self->blender.linked_calib =
        self->blender.banks [BANK_AMP].linked_calib;

      const _lane_bank bank_id = (_lane_bank) bank;
      if (self->blender.banks [bank].linked_calib) {
        request_calibrate_linked (self, bank_id);
      } else {
        for (size_t i = 0; i < NB_NUM_MODELS; ++i) {
          if (calib_enabled_for_lane (self, bank_id, i))
            request_calibrate (self, bank_id, i, true);
        }
      }
    }
  }

	  if (self->calib_source) {
	    float v = *self->calib_source;
	    if (v < 0.0f)
	      v = 0.0f;
	    if (v != self->last_calib_source) { CP
	      self->last_calib_source = v;
	      self->blender.calib_source = (int) lrintf (v);
	      for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
	        const _lane_bank bank_id = (_lane_bank) bank;
	        if (self->blender.banks [bank].linked_calib) {
	          request_calibrate_linked (self, bank_id);
	        } else {
	          for (size_t i = 0; i < NB_NUM_MODELS; ++i)
	            if (calib_enabled_for_lane (self, bank_id, i))
	              request_calibrate (self, bank_id, i, true);
	        }
	      }
	    }
	  }

	  if (self->calib_target_db) {
	    v = *self->calib_target_db;
	    if (v != self->last_calib_target_db)
	      set_calib_target_db (self, v);
	  }

	  if (read_changed_bool (
          self->noisegate_enabled, self->last_noisegate_enabled, b))
	    self->blender.noisegate_on = b;

	  if (read_changed (
          self->noisegate_threshold, self->last_noisegate_threshold, v))
	    self->blender.noisegate.set_threshold (v);

	  if (read_changed (
          self->noisegate_attack, self->last_noisegate_attack, v))
	    self->blender.noisegate.set_attack (v);

	  if (read_changed (
          self->noisegate_hold, self->last_noisegate_hold, v))
	    self->blender.noisegate.set_hold (v);

	  if (read_changed (
          self->noisegate_release, self->last_noisegate_release, v))
	    self->blender.noisegate.set_release (v);

	  if (read_changed_bool (self->tuner_on, self->last_tuner_on, b)) {
	    self->blender.tuner_on = b;
	    self->tuner_enabled.store (self->blender.tuner_on,
	                               std::memory_order_release);
	  }

	  if (read_changed (
          self->tuner_base_freq, self->last_tuner_base_freq, v)) {
	    self->blender.tuner_base_freq = v;
	    self->blender.pitchtracker.set_base_freq ((int) lrintf (v));
	  }

	  if (read_changed (
          self->master_gain, self->last_master_gain, v))
	    self->blender.set_master_gain (v);

	  if (read_changed (
          self->presence, self->last_presence, v))
	    self->blender.set_presence (v);
	  
  // check all lane parameters
  for (size_t bank = BANK_PEDAL; bank < BANK_COUNT; ++bank) {
    const _lane_bank bnk = (_lane_bank) bank;

    for (i = 0; i < NB_NUM_MODELS; i++) {
      if (read_changed (
          self->gain_in_db [bank] [i],
          self->last_gain_in_db [bank] [i], v)) {
        CP
        self->blender.set_gain_in (bnk, i, db_to_gain (v));
      }

      if (read_changed (
          self->ir_pitch [bank] [i],
          self->last_ir_pitch [bank] [i], v)) {
        CP
        request_ir_pitch (self, bnk, i, v);
      }

      if (read_changed (
          self->gain_out_db [bank] [i],
          self->last_gain_out_db [bank] [i], v)) {
        CP
        self->blender.set_gain_out (bnk, i, db_to_gain (v));
      }

      if (read_changed (
          self->dry_out_db [bank] [i],
          self->last_dry_out_db [bank] [i], v)) {
        CP
        self->blender.set_dry_out (
          bnk, i, v <= DB_SILENCE ? 0.0f : db_to_gain (v));
      }

      if (read_changed (
          self->delay [bank] [i],
          self->last_delay [bank] [i], v)) {
        CP
        self->blender.set_delay_ms (bnk, i, v);
        self->stats_dirty.store (true, std::memory_order_release);
      }

      if (read_changed_bool (
          self->lane_mute [bank] [i],
          self->last_lane_mute [bank] [i], b)) {
        CP
        self->base_lane_mute [bank] [i] = b;
        apply_effective_controls (self);
      }

      if (read_changed_bool (
          self->dcflip [bank] [i],
          self->last_dcflip [bank] [i], b)) {
        CP
        self->blender.dcflip (bnk, i, b);
      }

      if (self->calibrate [bank] [i]) {
        const float v = *self->calibrate [bank] [i];
        const bool enabled = v >= 0.5f;
        self->pending_calib_enabled [bank] [i].store (
          enabled, std::memory_order_release);
        if (v != self->last_calibrate [bank] [i]) { CP
          self->last_calibrate [bank] [i] = v;
          request_calibrate (self, bnk, i, enabled);
        }
      }
    }
  }

  if (self->controls_dirty.exchange (false, std::memory_order_acq_rel))
    apply_effective_controls (self);

  // actual DSP
  if (self->audio_in && self->audio_out) {
    self->blender.process_block ((float *) self->audio_in, self->audio_out, nframes);
  }

  if (self->noisegate_gain)
    *self->noisegate_gain = self->blender.noisegate_on
      ? self->blender.noisegate.get_current_gain ()
      : 1.0f;
  if (self->tuner_note)
    *self->tuner_note =
      self->detected_tuner_note.load (std::memory_order_acquire);
  if (self->tuner_cents_off)
    *self->tuner_cents_off =
      self->detected_tuner_cents.load (std::memory_order_acquire);
  if (self->tuner_freq)
    *self->tuner_freq =
      self->detected_tuner_freq.load (std::memory_order_acquire);

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
