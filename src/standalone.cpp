
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
 * Standalone wrapper for NeuralBlender
*/

#include <jack/jack.h>
#include <signal.h>
#include <unistd.h>
#include "neuralblender.h"
#include "timestamp.h"

#ifdef CMDLINE_DEBUG
#include "cmdline/cmdline.h"
#define debug(...) cmdline_debug(stderr,ANSI_RED,__FILE__,__LINE__,__FUNC__,__VA_ARGS__)
#else
#define debug(...)
#define CP
#define BP
#endif

const char *g_build_timestamp = BUILD_TIMESTAMP;

/******************************************************************************
 * JACK stuff
 */
 
static jack_client_t *jack_client = nullptr;
static jack_port_t *jack_in = nullptr;
static jack_port_t *jack_out = nullptr;
static volatile bool running = true;

static c_neuralblender g_blender;

static int jack_process (jack_nframes_t nframes, void *) {
  float *in = (float *) jack_port_get_buffer (jack_in, nframes);
  float *out = (float *) jack_port_get_buffer (jack_out, nframes);

  g_blender.process_block (in, out, nframes);
  return 0;
}

static void jack_shutdown (void *) {
  running = false;
}

static void signal_handler (int) {
  running = false;
}

/******************************************************************************
 * args, main etc
 */
 
void do_usage (int argc, char **argv) {
  if (argc < 1)
    return;
  char *c = argv [0];
  
  //while (*c == '.' || *c == '/')
  //  c++;
    
  printf ("NeuralBlender (%s) build timestamp %s\n", c, g_build_timestamp);
}

bool parse_args (int argc, char **argv, c_neuralblender *blender) {
  int i;
  CP
  for (i = 1; i < argc; i++) {
    if (!strcmp (argv [i], "-h") || !strcmp (argv [i], "--help")) {
      do_usage (argc, argv);
      exit (0);
    } else if (!strcmp (argv [i], "-a")) {
      if (argv [i + 1]) {
        blender->amps [0].filename = argv [++i];
      } else {
        printf ("-a needs a filename argument\n");
        return false;
      }
    } else if (!strcmp (argv [i], "-b")) {
      if (argv [i + 1]) {
        blender->amps [1].filename = argv [++i];
      } else {
        printf ("-b needs a filename argument\n");
        return false;
      }
    } else {
      printf ("don't know what to do with '%s'\n", argv [i]);
      return false;
    }
  }
  return true;
}

int main (int argc, char **argv) {
  signal (SIGINT, signal_handler);
  signal (SIGTERM, signal_handler);
  
  if (!parse_args (argc, argv, &g_blender)) {
    printf ("Error parsing command line\n");
    do_usage (argc, argv);
    return 1;
  }
  
  if (g_blender.amps [0].filename != "")
    g_blender.load_model (0);
  if (g_blender.amps [1].filename != "")
    g_blender.load_model (1);

  jack_client = jack_client_open ("NeuralBlender", JackNullOption, nullptr);
  if (!jack_client) {
    fprintf (stderr, "could not open JACK client\n");
    return 1;
  }

  jack_set_process_callback (jack_client, jack_process, &g_blender);
  jack_on_shutdown (jack_client, jack_shutdown, &g_blender);

  jack_in = jack_port_register (
    jack_client,
    "in",
    JACK_DEFAULT_AUDIO_TYPE,
    JackPortIsInput,
    0
  );

  jack_out = jack_port_register (
    jack_client,
    "out",
    JACK_DEFAULT_AUDIO_TYPE,
    JackPortIsOutput,
    0
  );

  if (!jack_in || !jack_out) {
    fprintf (stderr, "could not register JACK ports\n");
    jack_client_close (jack_client);
    return 1;
  }
  
  g_blender.set_samplerate (jack_get_sample_rate (jack_client));
  g_blender.set_blocksize (jack_get_buffer_size (jack_client));
  g_blender.delays [0].set_frames (0);
  g_blender.delays [1].set_frames (24000);
  g_blender.amps [0].gain_in = 1.0;
  g_blender.amps [0].gain_out = 1.0;
  g_blender.amps [1].gain_in = 1.0;
  g_blender.amps [1].gain_out = 0.5;

  if (jack_activate (jack_client)) {
    fprintf (stderr, "could not activate JACK client\n");
    jack_client_close (jack_client);
    return 1;
  }
  
  fprintf(stderr, "NeuralBlender running. Connect ports manually. Ctrl+C to quit.\n");

  while (running)
    sleep (1);

  jack_client_close (jack_client);
  return 0;
}
