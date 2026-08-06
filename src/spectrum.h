
/* NeuralBlender
 * This is an inline function used by DSP and UI to generate the same
 * table of frequencies for the spectrum anaylzer. see eqgraph.{h,cpp}
 */

#pragma once
 
#define SPECTRUM_MIN_HZ   20.0f
#define SPECTRUM_MAX_HZ   20000.0f
#define SPECTRUM_BINS     512
#define SPECTRUM_FFT_SIZE 4096
#define SPECTRUM_HOP_SIZE (SPECTRUM_FFT_SIZE / 2)

inline void generate_spectrum_frequencies (
    float *frequencies,
    size_t count,
    float min_hz = SPECTRUM_MIN_HZ,
    float max_hz = SPECTRUM_MAX_HZ) {

  if (!frequencies || count == 0)
    return;

  if (count == 1) {
    frequencies [0] = min_hz;
    return;
  }

  const float log_range = logf (max_hz / min_hz);

  for (size_t i = 0; i < count; i++) {
    const float normalized = (float) i / (float) (count - 1);
    frequencies [i] = min_hz * expf (log_range * normalized);
  }

  frequencies [0] = min_hz;
  frequencies [count - 1] = max_hz;
}
