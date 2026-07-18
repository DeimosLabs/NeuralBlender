
/* NeuralBlender - RTNeural / NAM based amp modeler
 *
 * Config file reading/writing
 */

#pragma once

#include <iostream>
#include <filesystem>
#include <stdio.h>

#define CONFIG_FILE_NAME             ".config/neuralblender.conf"
#define CONFIG_KEY_NAME_MODEL_CWD    "modelpath"
#define CONFIG_KEY_NAME_IR_CWD       "irpath"
#define CONFIG_KEY_NAME_ADV          "showadvanced"
#define CONFIG_KEY_NAME_EXCL         "excldefault"
#define CONFIG_KEY_NAME_CALIB        "calibdefault"
#define CONFIG_KEY_NAME_CALIB_TARGET "calibtargetdb"
#define CONFIG_KEY_NAME_VU_SCALE     "vuscaledb"
#define CONFIG_KEY_NAME_VU_HEADROOM  "vuheadroomdb"
#define CONFIG_KEY_NAME_VU           "vu"
#define CONFIG_KEY_NAME_TUNER        "tuner"
#define CONFIG_KEY_NAME_TUNER_BASE   "tunerbasefreq"
#define CONFIG_KEY_NAME_NOISEGATE    "noisegate"
#define CONFIG_KEY_NAME_NOISETHRESH  "noisethresh"
#define CONFIG_KEY_NAME_NOISEATTACK  "noiseattack"
#define CONFIG_KEY_NAME_NOISEHOLD    "noisehold"
#define CONFIG_KEY_NAME_NOISERELEASE "noiserelease"
#define CONFIG_DEFAULT_DIR           "/"

class c_configfile {
public:
  c_configfile ();
  bool read_file (std::string path);
  bool read_file ();
  bool write_file (std::string path);
  bool write_file ();
  std::string get_path ();
  bool set_item (size_t n, std::string value);
  std::string get_item (size_t n);
  bool set_item (std::string name, std::string value);
  std::string get_item (std::string name);
  int find_item (std::string name);
  void dump (); // for debugging

  static bool istrue (std::string name);
  
private:
  void process_in (int which, std::string value);
  void process_out (int which, std::string value);
};
