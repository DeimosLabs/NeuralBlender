
/* NeuralBlender - RTNeural / NAM based amp modeler
 *
 * Config file reading/writing
 */

#pragma once

#include <iostream>
#include <filesystem>
#include <stdio.h>

#define CONFIG_FILE_NAME       ".config/neuralblender.conf"
#define CONFIG_KEY_NAME_CWD    "modelpath"
#define CONFIG_KEY_NAME_ADV    "showadvanced"
#define CONFIG_KEY_NAME_EXCL   "excldefault"
#define CONFIG_KEY_NAME_CALIB  "calibdefault"
#define CONFIG_DEFAULT_DIR     "/"

class c_configfile {
public:
  c_configfile ();
  bool read_file (std::string path);
  bool read_file ();
  bool write_file (std::string path);
  bool write_file ();
  std::string get_path ();
  bool set_item (size_t n, std::string value);
  bool istrue (std::string name);
  std::string get_item (size_t n);
  bool set_item (std::string name, std::string value);
  std::string get_item (std::string name);
  int find_item (std::string name);
  void dump (); // for debugging
  
private:
  void process_in (int which, std::string value);
  void process_out (int which, std::string value);
};
