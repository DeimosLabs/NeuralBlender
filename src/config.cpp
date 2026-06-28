/* NeuralBlender - RTNeural / NAM based amp modeler
 *
 * Config file reading/writing
 */

#include <fstream>
#include <string>
#include <cctype>
#include <vector>
#include "config.h"

#ifdef CMDLINE_DEBUG
#include "cmdline/cmdline.h"
#define debug(...) cmdline_debug(stderr,ANSI_YELLOW,__FILE__,__LINE__,__FUNC__,__VA_ARGS__)
#else
#define debug(...) //do{}while(0)
#define CP         //do{}while(0)
#define BP         //do{}while(0)
#endif

struct s_option {
  std::string name;
  std::string value;
} g_options [] = {
  { CONFIG_KEY_NAME_CWD, "" },
  { CONFIG_KEY_NAME_ADV, "" },
  { CONFIG_KEY_NAME_CALIB, "" },
  { "", "" }
};

static bool mkdir_p (const std::string& path) {
    std::error_code ec;

    if (std::filesystem::create_directories (path, ec))
        return true;    // created at least one directory

    if (!ec)
        return true;    // already existed

    return false;       // error
}

std::string strip_whitespace (const std::string& str) {
    size_t start = 0;
    while (start < str.size () &&
           std::isspace (static_cast<unsigned char> (str [start])))
        ++start;

    size_t end = str.size();
    while (end > start &&
           std::isspace (static_cast<unsigned char> (str [end - 1])))
        --end;

    return str.substr (start, end - start);
}

static std::vector<std::string> read_lines (const std::string filename) {
  std::ifstream f (filename);
  std::vector<std::string> lines;
  std::string line;
  
  if (f.fail ()) {
    debug ("can't open '%s'", filename.c_str ());
  }

  while (std::getline (f, line))
    lines.push_back (line);

  return lines;
}

c_configfile::c_configfile () {
  std::string str = get_path ();
}

std::string c_configfile::get_path () {
  char *c = getenv ("HOME");
  if (!c || !c [0])
    return CONFIG_FILE_NAME;
  
  return std::string (c) + "/" + CONFIG_FILE_NAME;
}

void c_configfile::process_in (int which, std::string value) {
  debug ("which=%d (%s), value='%s'", 
         which, g_options [which].name.c_str (), value.c_str ());
  int noptions;
  for (noptions = 0; g_options [noptions].name.length () > 0; noptions++);
  if (which < 0 || which >= noptions) {
    debug ("got out of range index: %d", which);
  }
  
  g_options [which].value = value;
}

void c_configfile::process_out (int which, std::string value) {
}

bool c_configfile::istrue (std::string name) {
  std::string value = get_item (name);
  if (!value.size ())
    return false;
  
  if (value == "1")
    return true;
  
  if (value == "true")
    return true;
    
  if (value == "yes")
    return true;
    
  if (value == "TRUE")
    return true;
    
  if (value == "YES")
    return true;
    
  return false;
}

int c_configfile::find_item (std::string name) {
  debug ("name='%s'", name.c_str ());
  bool found = false;
  for (size_t i = 0; g_options [i].name.size () > 0; i++) {
    if (g_options [i].name == name) {
      found = true;
      debug ("FOUND");
      return (int) i;
    }
  }
  return -1;
}

bool c_configfile::set_item (size_t which, std::string value) {
  debug ("which=%d, value='%s'", (int) which, value.c_str ());
  if (which < 0 || which >= sizeof (g_options) / sizeof (g_options [0])) {
    debug ("value out of range");
    return false;
  }
  g_options [which].value = value;
  
  return true;
}

bool c_configfile::set_item (std::string name, std::string value) {
  int which = find_item (name);
  if (which >= 0)
    return set_item (which, value);
  
  return false;
}

std::string c_configfile::get_item (size_t n) {
  if (n >= sizeof (g_options) / sizeof (g_options [0]) ||
      g_options [n].name.empty ())
    return "";

  return g_options [n].value;
}

std::string c_configfile::get_item (std::string str) {
  dump ();
  int n = find_item (str);
  if (n < 0)
    return "";

  debug ("returning '%s'", g_options [n].value.c_str ());
  return g_options [n].value;
}

void c_configfile::dump () {
  for (int i = 0; g_options [i].name.size (); i++) {
    printf ("option %d: '%s' = '%s'\n",
        i, g_options [i].name.c_str (), g_options [i].value.c_str ());
  }
}

bool c_configfile::read_file () { CP
  std::string path = get_path ();
  return read_file (path);
}

bool c_configfile::read_file (std::string path) { CP
  if (path.size () <= 0) {
    debug ("no path returned from get_path");
    return false;
  }
  
  std::vector<std::string> lines = read_lines (path);
  if (lines.size () <= 0) {
    debug ("file empty");
    return false;
  }
  
  size_t i;
  for (i = 0; i < lines.size (); i++) {
    //debug ("\ngot line: '%s'", lines [i].c_str ());
    size_t n = lines [i].find_first_of ("=");
    if (n >= 0 && n < lines [i].size ()) {
      std::string l = strip_whitespace (lines [i].substr (0, n));
      std::string r = strip_whitespace (lines [i].substr (n + 1));
      int j = find_item (l);
      if (j < 0) {
        debug ("invalid option name '%s'", l.c_str ());
      } else {
        g_options [j].value = r;
      }
    }
  }
  debug ("dump:");
  this->dump ();
  
  return true;
}

bool c_configfile::write_file () { CP
  std::string path = get_path ();
  return write_file (path);
}

bool c_configfile::write_file (std::string path) { CP
  std::filesystem::path p (path);
  if (p.has_parent_path ())
    mkdir_p (p.parent_path ().string ());

  std::ofstream f (path);
  
  for (size_t i = 0; g_options [i].name.size () > 0; i++) {
    std::string line = g_options [i].name + "=" + g_options [i].value;
    //debug ("i=%d, line='%s'", (int) i, line.c_str ());
    f << line << "\n";
  }
  
  return static_cast<bool> (f);
}
