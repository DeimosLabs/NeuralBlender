
/* NeuralBlender - shared compile-time constants
 *
 * We need to put these here to avoid circular header dependency.
 */

#pragma once

#define SEMITONE_MULTIPLIER      1.0594630943592953
#define NB_NUM_PEDALS            4
#define NB_NUM_MODELS            4
#define NB_NUM_CABS              4
#define NB_MAX_LANES             4
#define NB_STATS_PER_LANE        3 // dsp->ui: delay frames, model type, trim

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
