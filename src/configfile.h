
/* NeuralBlender - RTNeural / NAM based amp modeler
 *
 * Config file reading/writing
 */

#pragma once

#include <iostream>
#include <filesystem>
#include <stdio.h>

#define CONFIG_FILE_NAME             ".config/neuralblender.conf"
#define CONFIG_KEY_NAME_MODEL_CWD    "model_path"
#define CONFIG_KEY_NAME_IR_CWD       "ir_path"
#define CONFIG_KEY_NAME_ADV          "show_advanced"
#define CONFIG_KEY_NAME_EXCL         "excl_default"
#define CONFIG_KEY_NAME_CALIB        "calib_default"
#define CONFIG_KEY_NAME_CALIB_TARGET "calib_target_db"
#define CONFIG_KEY_NAME_VU_SCALE     "vu_scale_db"
#define CONFIG_KEY_NAME_VU_HEADROOM  "vu_headroom_db"
#define CONFIG_KEY_NAME_VU           "vu"
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
