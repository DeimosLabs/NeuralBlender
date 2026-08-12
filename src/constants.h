
/* NeuralBlender - shared compile-time constants
 *
 * We need to put these here to avoid circular header dependency.
 */

#pragma once

#define SEMITONE_MULTIPLIER      1.0594630943592953
#define NB_NUM_PEDALS            4
#define NB_NUM_MODELS            4
#define NB_NUM_CABS              4
//#define NB_MAX_LANES             4
static constexpr size_t NB_MAX_LANES = 
        std::max (NB_NUM_PEDALS, std::max (NB_NUM_MODELS, NB_NUM_CABS));
#define NB_STATS_PER_LANE        3 // dsp->ui: delay frames, model type, trim
#define NB_FREQ_MIN              20.0f
#define NB_FREQ_MAX              20000.0f

#define MAX_DELAY_MS             30
#define MAX_DELAY_FRAMES         (MAX_DELAY_MS * 192)
#define MAX_BLOCK_SIZE           8192
#define DB_SILENCE               -120.0f
#define DB_CALIB_TARGET_DEFAULT  -18.0f
#define GAIN_DB_MIN              -40.0f
#define GAIN_DB_MAX              40.0f
#define CALIB_TARGET_DB_MIN      -40.0f
#define CALIB_TARGET_DB_MAX      0.0f
#define NOISEGATE_THRESH_MIN     DB_SILENCE
#define NOISEGATE_THRESH_MAX     -6.0f
#define WARMUP_BLOCKS            5
#define NB_XFADE_MS              10.0f
#define NB_LANE_XFADE_MS         NB_XFADE_MS
#define TUNER_THRESH_DB          -40.0f
#define EQ_NUM_BANDS             8
#define EQ_SLOPE_MAX             4
#define EQ_PARAM_XFADE_MS        2.0f
#define IR_SILENCE_THRESHOLD     -80.0f
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum _nb_error_code {
  NB_ERROR_NONE = 0,
  NB_ERROR_INTERNAL,
  NB_ERROR_FILE_NOTFOUND,
  NB_ERROR_FILE_NOTFOUND_NAM,
  NB_ERROR_FILE_NOTFOUND_WAV,
  NB_ERROR_FILE_READ_NAM,
  NB_ERROR_FILE_READ_JSON,
  NB_ERROR_FILE_READ_WAV,
  NB_ERROR_FILE_READ_GZIP,
  NB_ERROR_FILENAME_NAM,
  NB_ERROR_FILENAME_WAV,
  NB_ERROR_FILENAME_GZIP,
  NB_ERROR_FILE_FORMAT_NAM,
  NB_ERROR_FILE_FORMAT_JSON,
  NB_ERROR_FILE_FORMAT_WAV,
  NB_ERROR_FILE_FORMAT_GZIP,
  NB_ERROR_FILE_SUBFORMAT_NAM,
  NB_ERROR_FILE_SUBFORMAT_WAV,
  NB_ERROR_FILE_SIZE,
  NB_ERROR_FILE_EMPTY,
  NB_ERROR_FILE_OTHER,
  NB_ERROR_MEMORY,
  NB_ERROR_CONVOLVER,
  NB_ERROR_STATE
};
