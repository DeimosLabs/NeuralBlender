/* NeuralBlender - RTNeural / NAM based amp modeler
 *
 * Config file reading/writing
 */

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fcntl.h>
#include <fstream>
#include <string>
#include <sys/file.h>
#include <unistd.h>
#include <vector>
#include "state.h"

#define CMDLINE_DEBUG_COLOR ANSI_DARK_YELLOW
#include "cmdline/debug.h"

struct s_option {
  std::string name;
  std::string value;
} g_options [] = {
  { CONFIG_KEY_NAME_MODEL_CWD, "" },
  { CONFIG_KEY_NAME_IR_CWD, "" },
  { CONFIG_KEY_NAME_PRESET_CWD, "" },
  { CONFIG_KEY_NAME_ADV, "" },
  { CONFIG_KEY_NAME_CALIB, "" },
  { CONFIG_KEY_NAME_CALIB_TARGET, "" },
  { CONFIG_KEY_NAME_VU_SCALE, "" },
  { CONFIG_KEY_NAME_VU_HEADROOM, "" },
  { CONFIG_KEY_NAME_BYP_DCLICK, "" },
  { CONFIG_KEY_NAME_BYP_RCLICK, "" },
  { CONFIG_KEY_NAME_TOOLTIPS, "" },
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
    debug ("can't open '%s' for reading", filename.c_str ());
  }

  while (std::getline (f, line))
    lines.push_back (line);

  return lines;
}

static bool write_lines (
    const std::string &filename,
    const std::vector<std::string> &v) {
  std::ofstream f (filename);
  
  if (f.fail ()) {
    debug ("can't open '%s' for writing", filename.c_str ());
    return false;
  }

  for (const std::string &line : v)
    f << line << "\n";

  return static_cast<bool> (f);
}

class c_config_write_lock {
public:
  explicit c_config_write_lock (const std::string &path) {
    fd = open ((path + ".lock").c_str (), O_CREAT | O_RDWR, 0600);
    if (fd >= 0 && flock (fd, LOCK_EX) != 0) {
      close (fd);
      fd = -1;
    }
  }

  ~c_config_write_lock () {
    if (fd >= 0) {
      flock (fd, LOCK_UN);
      close (fd);
    }
  }

  bool locked () const { return fd >= 0; }

private:
  int fd = -1;
};

bool istrue (std::string value) {
  //std::string value = get_item (name);
  if (!value.size ())
    return false;
  
  if (value == "1")
    return true;
  
  if (value == "true")
    return true;
    
  if (value == "yes")
    return true;

  if (value == "on")
    return true;
    
  if (value == "TRUE")
    return true;
    
  if (value == "YES")
    return true;

  if (value == "ON")
    return true;
    
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// c_configfile

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
  //dump ();
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

int c_configfile::delete_eq_preset (std::string name) {
  int ret = 0;
  std::vector <c_eq_state> new_presets;
  
  //for (int i = eq_presets.size () - 1; i >= 0; i--) {
  for (int i = 0; i < eq_presets.size (); i++) {
    if (!eq_presets [i].builtin && eq_presets [i].preset_name == name) {
      debug ("deleted: %s", name.c_str ());
      ret++;
    } else {
      debug ("passed: %s", eq_presets [i].preset_name.c_str ());
      new_presets.push_back (eq_presets [i]);
    }
  }
  
  eq_presets.clear ();
  eq_presets = new_presets;

  pending_eq_additions.erase (
    std::remove_if (
      pending_eq_additions.begin (),
      pending_eq_additions.end (),
      [&name] (const c_eq_state &preset) {
        return preset.preset_name == name;
      }),
    pending_eq_additions.end ());
  if (ret > 0 && std::find (
        pending_eq_deletions.begin (),
        pending_eq_deletions.end (),
        name) == pending_eq_deletions.end ()) {
    pending_eq_deletions.push_back (name);
  }
  return ret;
}

void c_configfile::add_eq_preset (const c_eq_state &preset) {
  eq_presets.push_back (preset);
  pending_eq_additions.push_back (preset);
}

void c_configfile::reset_eq_presets () {
  static const char *builtin [] = {
    "eq=off;0.00;off:off:off:off:off:off:off:off;0:4:5:5:5:5:6:7;50.000:100.000:250.000:500.000:1000.000:2000.000:4000.000:8000.000;0.000:0.000:0.000:0.000:0.000:0.000:0.000:0.000;1.000:1.000:1.000:1.000:1.000:1.000:1.000:1.000;\"[Builtin] Flat\"",
    "eq=off;0.00;on:on:off:on:on:off:on:off;1:4:5:5:5:5:6:7;90.000:336.807:250.000:556.271:1000.000:2000.000:4000.000:8000.000;0.000:-9.767:0.000:-11.442:12.000:0.000:-4.000:0.000;1.000:1.000:1.000:3.740:1.500:1.000:1.000:1.000;\"[Builtin] Pre-gain sharpness\"",
    "eq=off;0.00;on:off:off:off:on:on:on:off;3:4:5:5:5:5:6:7;80.000:100.000:250.000:500.000:800.000:4700.000:3000.000:8000.000;0.000:0.000:0.000:0.000:-6.000:5.000:3.000:0.000;0.750:1.000:1.000:1.000:1.000:4.000:1.000:1.000;\"[Builtin] Slight midscoop\"",
    "eq=off;0.00;off:off:on:off:off:off:off:off;0:4:5:5:5:5:6:7;50.000:100.000:60.000:500.000:1000.000:2000.000:4000.000:8000.000;0.000:0.000:-36.000:0.000:0.000:0.000:0.000:0.000;1.000:1.000:100.000:1.000:1.000:1.000:1.000:1.000;\"[Builtin] Notch out 60Hz hum\"",

    NULL
  };
  c_eq_state state;
  int i;
  
  eq_presets.clear ();
  
  for (i = 0; builtin [i] && builtin [i] [0]; i++) {
    if (state.from_string (std::string (builtin [i])) == t_parse_result::parsed) {
      debug ("adding builtin preset %d, '%s'", i, state.preset_name.c_str ());
      state.builtin = true;
      eq_presets.push_back (state);
    }
  }
}

bool same_eq_settings (
    const c_eq_state &a,
    const c_eq_state &b) {
  // match at the precision used by c_eq_state::to_string().
  const auto same_float = [] (float x, float y, float tolerance) {
    return std::isfinite (x) && std::isfinite (y) &&
           std::fabs (x - y) <= tolerance;
  };

  if (a.on != b.on ||
      !same_float (a.master_gain_db, b.master_gain_db, 0.0051f))
    return false;

  for (int i = 0; i < EQ_NUM_BANDS; ++i) {
    if (a.enabled [i] != b.enabled [i] ||
        a.mode [i] != b.mode [i] ||
        a.slope [i] != b.slope [i] ||
        !same_float (a.freq [i], b.freq [i], 0.00051f) ||
        !same_float (a.gain_db [i], b.gain_db [i], 0.00051f) ||
        !same_float (a.q [i], b.q [i], 0.00051f))
      return false;
  }

  return true;
}

bool same_eq_preset (
    const c_eq_state &a,
    const c_eq_state &b) {
  return a.preset_name == b.preset_name && same_eq_settings (a, b);
}

std::string sanitize_eq_preset_name (std::string name) {
  static const std::string builtin_tag = "[Builtin]";
  size_t pos = 0;
  while ((pos = name.find (builtin_tag, pos)) != std::string::npos)
    name.erase (pos, builtin_tag.size ());
  return strip_whitespace (name);
}

static std::vector<c_eq_state> read_user_eq_presets (
    const std::string &path) {
  std::vector<c_eq_state> presets;
  int unnamed_eq_id = 1;

  for (const std::string &line : read_lines (path)) {
    std::string token, value;
    split_at_equal (line, token, value);
    if (strip_whitespace (token) != "eq")
      continue;

    c_eq_state preset;
    if (preset.from_string ("eq=" + strip_whitespace (value)) !=
        t_parse_result::parsed)
      continue;
    if (preset.preset_name.empty ())
      preset.preset_name = "Unnamed " + std::to_string (unnamed_eq_id++);
    preset.builtin = false;
    presets.push_back (preset);
  }

  return presets;
}

static bool same_eq_preset_list (
    const std::vector<c_eq_state> &a,
    const std::vector<c_eq_state> &b) {
  if (a.size () != b.size ())
    return false;

  for (size_t i = 0; i < a.size (); ++i) {
    if (a [i].builtin != b [i].builtin ||
        !same_eq_preset (a [i], b [i]))
      return false;
  }
  return true;
}

bool c_configfile::read_file () { CP
  std::string path = get_path ();
  return read_file (path);
}

bool c_configfile::read_file (std::string path) { CP
  if (path.size () <= 0) {
    debug ("empty path");
    return false;
  }
  
  //eq_presets.clear ();
  reset_eq_presets ();
  pending_eq_additions.clear ();
  pending_eq_deletions.clear ();
  
  std::vector<std::string> lines = read_lines (path);
  if (lines.size () <= 0) {
    debug ("file empty");
    return false;
  }
  
  size_t i;
  int unnamed_eq_id = 1;
  for (i = 0; i < lines.size (); i++) {
    //debug ("\ngot line: '%s'", lines [i].c_str ());
    int n = lines [i].find_first_of ("=");
    if (n >= 0 && n < lines [i].size ()) {
      std::string l = strip_whitespace (lines [i].substr (0, n));
      std::string r = strip_whitespace (lines [i].substr (n + 1));
      int j = find_item (l);
      if (j < 0) {
        if (l == "eq") {
          c_eq_state eq_state;
          if (eq_state.from_string ("eq=" + r) == t_parse_result::parsed) {
            if (eq_state.preset_name.size () == 0) {
              eq_state.preset_name = "Unnamed " + std::to_string (unnamed_eq_id++);
            }

            const bool duplicates_builtin = std::any_of (
              eq_presets.begin (),
              eq_presets.end (),
              [&eq_state] (const c_eq_state &existing) {
                return existing.builtin && same_eq_preset (existing, eq_state);
              });
            if (!duplicates_builtin)
              eq_presets.push_back (eq_state);
          } else {
            debug ("invalid EQ line: %s", lines [i].c_str ());
          }
        } else {
          debug ("invalid option name '%s'", l.c_str ());
        }
      } else {
        g_options [j].value = r;
      }
    }
  }
  //debug ("dump:");
  //this->dump ();

  std::error_code ec;
  config_mtime = std::filesystem::last_write_time (path, ec);
  config_mtime_valid = !ec;
  
  return true;
}

bool c_configfile::refresh_eq_presets_if_changed () {
  const std::string path = get_path ();
  std::error_code ec;
  const std::filesystem::file_time_type mtime =
    std::filesystem::last_write_time (path, ec);
  if (ec || (config_mtime_valid && mtime == config_mtime))
    return false;

  const std::vector<c_eq_state> old_presets = eq_presets;
  std::vector<c_eq_state> disk_presets = read_user_eq_presets (path);

  reset_eq_presets ();
  for (const c_eq_state &preset : disk_presets) {
    const bool duplicates_builtin = std::any_of (
      eq_presets.begin (),
      eq_presets.end (),
      [&preset] (const c_eq_state &existing) {
        return existing.builtin && same_eq_preset (existing, preset);
      });
    if (!duplicates_builtin)
      eq_presets.push_back (preset);
  }

  for (const std::string &name : pending_eq_deletions) {
    eq_presets.erase (
      std::remove_if (
        eq_presets.begin (),
        eq_presets.end (),
        [&name] (const c_eq_state &preset) {
          return !preset.builtin && preset.preset_name == name;
        }),
      eq_presets.end ());
  }
  eq_presets.insert (
    eq_presets.end (),
    pending_eq_additions.begin (),
    pending_eq_additions.end ());

  config_mtime = mtime;
  config_mtime_valid = true;
  return !same_eq_preset_list (old_presets, eq_presets);
}

bool c_configfile::write_file () { CP
  std::string path = get_path ();
  return write_file (path);
}

bool c_configfile::write_file (std::string path) { CP
  std::filesystem::path p (path);
  if (p.has_parent_path ())
    mkdir_p (p.parent_path ().string ());

  c_config_write_lock lock (path);
  if (!lock.locked ()) {
    debug ("can't lock '%s' for writing", path.c_str ());
    return false;
  }

  std::vector<c_eq_state> merged_presets = read_user_eq_presets (path);

  for (const std::string &name : pending_eq_deletions) {
    merged_presets.erase (
      std::remove_if (
        merged_presets.begin (),
        merged_presets.end (),
        [&name] (const c_eq_state &preset) {
          return preset.preset_name == name;
        }),
      merged_presets.end ());
  }
  merged_presets.insert (
    merged_presets.end (),
    pending_eq_additions.begin (),
    pending_eq_additions.end ());

  std::vector<std::string> lines;
  for (size_t i = 0; g_options [i].name.size () > 0; i++) {
    lines.push_back (g_options [i].name + "=" + g_options [i].value);
  }

  for (c_eq_state &preset : merged_presets) {
    std::string eqstring;
    preset.to_string (eqstring);
    lines.push_back (eqstring);
  }

  const std::string tmp_path =
    path + ".tmp." + std::to_string ((long long) getpid ()) + "." +
    std::to_string ((unsigned long long) (uintptr_t) this);
  if (!write_lines (tmp_path, lines))
    return false;

  std::error_code ec;
  std::filesystem::rename (tmp_path, path, ec);
  if (ec) {
    std::filesystem::remove (tmp_path);
    debug ("can't replace '%s': %s", path.c_str (), ec.message ().c_str ());
    return false;
  }

  config_mtime = std::filesystem::last_write_time (path, ec);
  config_mtime_valid = !ec;

  reset_eq_presets ();
  eq_presets.insert (
    eq_presets.end (), merged_presets.begin (), merged_presets.end ());
  pending_eq_additions.clear ();
  pending_eq_deletions.clear ();
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// structs serialize/deserialize functions
// These functions take a string or array of strings to/from which read/write
// state data

// c_eq_sate: 7 strings separated by ';' between strings. Each string is:
//   - on/off
//   - master gain
//   - 5 series of 8 values, ':' between values. Each value is:
//     - enabled, combined mode/slope type, freq, gain, q
//   - preset name, " marks get converted to '

void split_at_equal (std::string s, std::string &before, std::string &after) {
  const size_t pos = s.find ('=');
  before = s.substr (0, pos);
  after = pos == std::string::npos ? "" : s.substr (pos + 1);
}

void split_at (std::string s, std::vector<std::string> &v, char c) {
  v.clear ();
  
  size_t begin = 0;
  while (true) {
    const size_t end = s.find (c, begin);
    v.push_back (s.substr (begin, end - begin));
    if (end == std::string::npos)
      break;
    begin = end + 1;
  }
}

const char *bank_name (_lane_bank bank) {
  switch (bank) {
    case BANK_PEDAL: return "pedal";
    case BANK_AMP:   return "amp";
    case BANK_CAB:   return "cab";
    default:         return nullptr;
  }
}

void strip_trailing (std::string &s, char c) {
  while (!s.empty () && s.back () == c)
    s.pop_back ();
}

void strip_leading (std::string &s, char c) {
  const size_t pos = s.find_first_not_of (c);
  s.erase (0, pos == std::string::npos ? s.size () : pos);
}

float tofloat (std::string s) {
  return atof (s.c_str ());
}

t_parse_result c_eq_state::from_string (std::string s) { CP
  int i, j;
  std::string token, value;
  
  split_at_equal (s, token, value);
  debug ("got token '%s'", token.c_str ());
  
  if (token == "eq_pre") {
    which = EQ_PRE;
  } else if (token == "eq_post") {
    which = EQ_POST;
  } else if (token == "eq") {
    which = EQ_PRESET;
  } else {
    debug ("token mismatch: '%s'", token.c_str ());
    return t_parse_result::not_mine;
  }
  
  std::vector<std::string> v;
  split_at (value, v, '"');
  if (v.size () != 3 || v [2].size () != 0) {
    debug ("wrong number of quotes");
    return t_parse_result::invalid;
  }
  
  value = v [0];
  preset_name = v [1];
  strip_trailing (value, ';');
  split_at (value, v, ';');
  
  if (v.size () != 7) {
    debug ("need exactly 7 groups of values, got %d", (int) v.size ());
    return t_parse_result::invalid;
  }
  
  for (i = 0; i < v.size (); i++) {
    debug ("got string '%s'", v [i].c_str ());
    switch (i) {
      
      case 0:CP
        on = istrue (v [i]);
      break;
      
      case 1:CP 
        master_gain_db = tofloat (v [i]);
      break;
      
      default:CP
        std::vector<std::string> vparam;
        split_at (v [i].c_str (), vparam, ':');
        if (vparam.size () != EQ_NUM_BANDS) {
          debug ("group %d doesn't have 8 values", i);
          return t_parse_result::invalid;
        }
        
        for (j = 0; j < vparam.size (); j++) {
          debug ("got individual value '%s'", vparam [j].c_str ());
          float f = tofloat (vparam [j]);
          
          switch (i) {
            case 2:CP
              enabled [j] = istrue (vparam [j]);
            break;
            
            case 3:CP
              eq_type_to_mode ((int) f, mode [j], slope [j]);
            break;
            
            case 4:CP
              freq [j] = f;
            break;
            
            case 5:CP
              gain_db [j] = std::clamp (f, -36.0f, 36.0f);
            break;
            
            case 6:CP
              q [j] = f;
            break;
            
            default:CP
              debug ("don't know what to do with extra value '%s'", vparam [j].c_str ());
              return t_parse_result::invalid;
            break;
          }
        }
      break;
    }
    
    v [i].clear ();
  }
  
  CP
  return t_parse_result::parsed;
}

void c_eq_state::to_string (std::string &s) {
  int i, j;
  char buf [1024];
  
  std::string typestr = "eq";
  if (which == EQ_PRE)
    typestr = "eq_pre";
  else if (which == EQ_POST)
    typestr = "eq_post";
  
  snprintf (buf, 1023, "%s=%s;%.2f;", typestr.c_str (), on ? "on" : "off",
            master_gain_db);
  std::string ret (buf);
  
  for (i = 0; i < 5; i++) {
    for (j = 0; j < EQ_NUM_BANDS; j++) {
      switch (i) {
        case 0:
          snprintf (buf, 1023, enabled [j] ? "on" : "off");
        break;
        
        case 1:
          snprintf (
            buf, 1023, "%d", eq_mode_to_type (mode [j], slope [j]));
        break;
        
        case 2:
          snprintf (buf, 1023, "%.3f", freq [j]);
        break;
        
        case 3:
          snprintf (buf, 1023, "%.3f", gain_db [j]);
        break;
        
        case 4:
          snprintf (buf, 1023, "%.3f", q [j]);
        break;
      }
      ret += std::string (buf);
      
      if (j < EQ_NUM_BANDS - 1)
        ret += std::string (":");
    }
    if (i < 4)
    ret += std::string (";");
  }
  
  std::string noquote_name = preset_name;
  std::replace(noquote_name.begin(), noquote_name.end(), '"', '\'');
  ret += ";\"" + noquote_name + "\"";
  
  debug ("ret='%s'", ret.c_str ());
  s = ret;
}

t_parse_result c_neuralblender_lane_state::from_strings (
    _lane_bank bank,
    int lane,
    std::vector<std::string> &v) {
  
  const std::string prefix =
    std::string (bank_name (bank)) + "_" + std::to_string (lane) + "_";
  
  auto find_value = [&] (
      const std::string &name,
      std::string *r_s,
      float *r_f,
      bool *r_b) {

    for (std::string &line : v) {
      if (line.empty ())
        continue;

      std::string token;
      std::string value;
      split_at_equal (line, token, value);

      if (token != prefix + name)
        continue;

      // Parse and validate value first.
      if (r_s)
        *r_s = value;
      if (r_f)
        *r_f = atof (value.c_str ());
      if (r_b)
        *r_b = istrue (value);

      line.clear();
      return true;
    }
    
    return false;
  };
  
  bool found = false;
  
  found |= find_value ("filename",  &filename, nullptr, nullptr);
  found |= find_value ("gain_in",   nullptr, &gain_in, nullptr);
  found |= find_value ("gain_out",  nullptr, &gain_out, nullptr);
  found |= find_value ("ir_pitch",  nullptr, &ir_pitch_semitones, nullptr);
  found |= find_value ("dry_out",   nullptr, &dry_out, nullptr);
  found |= find_value ("delay",     nullptr, &delay_ms, nullptr);
  found |= find_value ("lane_mute", nullptr, nullptr, &lane_mute);
  found |= find_value ("dcflip",    nullptr, nullptr, &dcflip);
  found |= find_value ("do_calib",  nullptr, nullptr, &do_calib);
  
  return found
    ? t_parse_result::parsed
    : t_parse_result::not_mine;
}

void c_neuralblender_lane_state::to_strings (_lane_bank bank, int lane,
                                             std::vector<std::string> &v) {
  const std::string prefix =
    std::string (bank_name (bank)) + "_" + std::to_string (lane) + "_";
    
  const auto onoff = [] (bool b) { return b ? "on" : "off"; };
  const auto add_string = [&] (const char *name, const std::string &value) {
    v.push_back (prefix + std::string (name) + "=" + value);
  };
  const auto add_float = [&] (const char *name, float value) {
    char value_string [64];
    snprintf (value_string, sizeof (value_string), "%f", value);
    add_string (name, value_string);
  };

  add_string ("filename", filename);
  add_float ("gain_in", gain_in);
  add_float ("gain_out", gain_out);
  add_float ("ir_pitch", ir_pitch_semitones);
  add_float ("dry_out", dry_out);
  add_float ("delay", delay_ms);
  add_string ("lane_mute", onoff (lane_mute));
  add_string ("dcflip", onoff (dcflip));
  add_string ("do_calib", onoff (do_calib));
}

void c_neuralblender_state::to_strings (std::vector<std::string> &v) {
  v.clear ();

  const auto onoff = [] (bool b) { return b ? "on" : "off"; };
  const auto add_string = [&] (const char *name, const std::string &value) {
    v.push_back (std::string (name) + "=" + value);
  };
  const auto add_bool = [&] (const char *name, bool value) {
    add_string (name, onoff (value));
  };
  const auto add_float = [&] (const char *name, float value) {
    char value_string [64];
    snprintf (value_string, sizeof (value_string), "%f", value);
    add_string (name, value_string);
  };
  const auto add_int = [&] (const char *name, int value) {
    add_string (name, std::to_string (value));
  };

  //add_string ("current_dir", current_dir);
  add_bool ("bypass", bypass);
  add_bool ("pedal_bypass", pedal_bypass);
  add_bool ("eqpre_bypass", !eqpre.on);
  add_bool ("amp_bypass", amp_bypass);
  add_bool ("eqpost_bypass", !eqpost.on);
  add_bool ("cab_bypass", cab_bypass);
  add_bool ("mute_all", mute_all);
  add_bool ("do_excl", do_excl);
  add_bool ("do_vu", do_vu);
  add_bool ("showadvanced", showadvanced);
  add_float ("master_gain", master_gain);
  add_float ("presence", presence);
  add_bool ("tuner_on", tuner_on);
  add_float ("tuner_base_freq", tuner_base_freq);
  add_bool ("noisegate_on", noisegate_on);
  add_float ("noisethresh", noisethresh);
  add_float ("noiseattack", noiseattack);
  add_float ("noisehold", noisehold);
  add_float ("noiserelease", noiserelease);
  add_float ("calib_target_db", calib_target_db);
  add_int ("calib_source", calib_source);

  const _lane_bank model_banks [] = {
    BANK_PEDAL,
    BANK_AMP,
    BANK_CAB
  };

  for (_lane_bank bank : model_banks) {
    const std::string prefix = bank_name (bank);
    add_int ((prefix + "_exclusive_lane").c_str (),
             banks [bank].exclusive_lane);
    add_bool ((prefix + "_linked_calib").c_str (),
              banks [bank].linked_calib);

    for (int lane = 0; lane < NB_NUM_MODELS; ++lane)
      banks [bank].lanes [lane].to_strings (bank, lane, v);
  }

  c_eq_state pre = eqpre;
  c_eq_state post = eqpost;
  pre.which = EQ_PRE;
  post.which = EQ_POST;

  std::string eq_string;
  pre.to_string (eq_string);
  v.push_back (eq_string);
  post.to_string (eq_string);
  v.push_back (eq_string);
}

bool c_neuralblender_state::from_strings (std::vector<std::string> &v) {
  c_neuralblender_state parsed = *this;
  std::vector<std::string> remaining = v;
  bool valid = true;

  auto take_value = [&] (const std::string &name, std::string &value) {
    bool found = false;

    for (std::string &line : remaining) {
      if (line.empty ())
        continue;

      std::string token;
      std::string candidate;
      split_at_equal (line, token, candidate);
      if (token != name)
        continue;

      if (found) {
        valid = false;
        continue;
      }

      value = candidate;
      line.clear ();
      found = true;
    }

    return found;
  };

  const auto read_string = [&] (const char *name, std::string &value) {
    std::string text;
    if (take_value (name, text))
      value = text;
  };
  const auto read_bool = [&] (const char *name, bool &value) {
    std::string text;
    if (take_value (name, text))
      value = istrue (text);
  };
  const auto read_float = [&] (const char *name, float &value) {
    std::string text;
    if (take_value (name, text))
      value = tofloat (text);
  };
  const auto read_int = [&] (const char *name, int &value) {
    std::string text;
    if (take_value (name, text))
      value = (int) tofloat (text);
  };

  read_string ("current_dir", parsed.current_dir);
  read_bool ("bypass", parsed.bypass);
  read_bool ("pedal_bypass", parsed.pedal_bypass);
  read_bool ("eqpre_bypass", parsed.eqpre_bypass);
  read_bool ("amp_bypass", parsed.amp_bypass);
  read_bool ("eqpost_bypass", parsed.eqpost_bypass);
  read_bool ("cab_bypass", parsed.cab_bypass);
  read_bool ("mute_all", parsed.mute_all);
  read_bool ("do_excl", parsed.do_excl);
  read_bool ("do_vu", parsed.do_vu);
  read_bool ("showadvanced", parsed.showadvanced);
  read_float ("master_gain", parsed.master_gain);
  read_float ("presence", parsed.presence);
  read_bool ("tuner_on", parsed.tuner_on);
  read_float ("tuner_base_freq", parsed.tuner_base_freq);
  read_bool ("noisegate_on", parsed.noisegate_on);
  read_float ("noisethresh", parsed.noisethresh);
  read_float ("noiseattack", parsed.noiseattack);
  read_float ("noisehold", parsed.noisehold);
  read_float ("noiserelease", parsed.noiserelease);
  read_float ("calib_target_db", parsed.calib_target_db);
  read_int ("calib_source", parsed.calib_source);

  const _lane_bank model_banks [] = {
    BANK_PEDAL,
    BANK_AMP,
    BANK_CAB
  };

  for (_lane_bank bank : model_banks) {
    const std::string prefix = bank_name (bank);
    read_int ((prefix + "_exclusive_lane").c_str (),
              parsed.banks [bank].exclusive_lane);
    read_bool ((prefix + "_linked_calib").c_str (),
               parsed.banks [bank].linked_calib);

    for (int lane = 0; lane < NB_NUM_MODELS; ++lane) {
      const t_parse_result result =
        parsed.banks [bank].lanes [lane].from_strings (
          bank, lane, remaining);
      if (result == t_parse_result::invalid)
        valid = false;
    }
  }

  bool have_eqpre = false;
  bool have_eqpost = false;
  for (std::string &line : remaining) {
    if (line.empty ())
      continue;

    c_eq_state eq;
    const t_parse_result result = eq.from_string (line);
    if (result == t_parse_result::not_mine)
      continue;
    if (result == t_parse_result::invalid) {
      valid = false;
      continue;
    }

    if (eq.which == EQ_PRE) {
      if (have_eqpre)
        valid = false;
      else
        parsed.eqpre = eq;
      have_eqpre = true;
    } else if (eq.which == EQ_POST) {
      if (have_eqpost)
        valid = false;
      else
        parsed.eqpost = eq;
      have_eqpost = true;
    } else {
      valid = false;
    }

    line.clear ();
  }

  for (const std::string &line : remaining) {
    if (!line.empty ()) {
      debug ("unrecognized state entry: '%s'", line.c_str ());
      valid = false;
    }
  }

  if (!valid)
    return false;

  if (have_eqpre)
    parsed.eqpre_bypass = !parsed.eqpre.on;
  if (have_eqpost)
    parsed.eqpost_bypass = !parsed.eqpost.on;

  *this = parsed;
  v = remaining;
  return true;
}

bool c_neuralblender_state::read_from (const std::string &filename) {
  if (filename.empty ()) {
    debug ("can't read state from an empty filename");
    return false;
  }

  std::vector<std::string> lines = read_lines (filename);
  if (lines.empty ()) {
    debug ("state file '%s' is empty or unreadable", filename.c_str ());
    return false;
  }

  if (!from_strings (lines)) {
    debug ("invalid state file '%s'", filename.c_str ());
    return false;
  }

  return true;
}

bool c_neuralblender_state::write_to (const std::string &filename) {
  if (filename.empty ()) {
    debug ("can't write state to an empty filename");
    return false;
  }

  const std::filesystem::path path (filename);
  if (path.has_parent_path () &&
      !mkdir_p (path.parent_path ().string ())) {
    debug ("can't create parent directory for '%s'", filename.c_str ());
    return false;
  }

  std::vector<std::string> lines;
  to_strings (lines);
  return write_lines (filename, lines);
}
