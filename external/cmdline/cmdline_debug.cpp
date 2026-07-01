#include "cmdline_debug.h"

#include <cstdarg>
#include <chrono>
#include <cstring>
#include <string>
#include <string_view>

namespace {

uint64_t now_ms () {
  using clock = std::chrono::steady_clock;
  return static_cast<uint64_t> (
    std::chrono::duration_cast<std::chrono::milliseconds> (
      clock::now ().time_since_epoch ()).count ());
}

std::string basename (const char *path) {
  if (!path)
    return {};

#ifdef DEBUG_FULL_PATHS
  return path;
#else
  const char *slash = std::strrchr (path, '/');
  return slash ? slash + 1 : path;
#endif
}

std::string short_function_name (const char *func) {
  if (!func)
    return {};

  std::string_view view (func);

  const size_t paren = view.find ('(');
  if (paren != std::string_view::npos)
    view = view.substr (0, paren);

  const size_t space = view.find_last_of (" \t");
  if (space != std::string_view::npos)
    view = view.substr (space + 1);

  return std::string (view);
}

std::string make_prefix (const char *color,
                         const char *file,
                         int line,
                         const char *func) {
  std::string prefix;

  if (color)
    prefix += color;

  prefix += '[';
  prefix += basename (file);
  prefix += ':';
  prefix += std::to_string (line);
  prefix += ' ';
  prefix += short_function_name (func);
  prefix += "] ";

  if (color)
    prefix += ANSI_RESET;

  return prefix;
}

std::string expand_format (const std::string &prefix, const char *fmt) {
  if (!fmt)
    return prefix + "(null)\n";

  std::string out;
  std::string_view view (fmt);

  while (!view.empty ()) {
    const size_t nl = view.find ('\n');
    const std::string_view line = view.substr (0, nl);

    if (!line.empty ())
      out += prefix;

    out.append (line.data (), line.size ());
    out += '\n';

    if (nl == std::string_view::npos)
      break;

    view.remove_prefix (nl + 1);
  }

  if (out.empty () || out.back () != '\n')
    out += '\n';

  return out;
}

} // namespace

int cmdline_debug (std::FILE *out,
                   const char *color,
                   const char *file,
                   int line,
                   const char *func,
                   const char *fmt,
                   ...) {
  if (!out)
    out = stderr;

  const std::string prefix = make_prefix (color ? color : ANSI_DARK_GREY,
                                          file,
                                          line,
                                          func);
  const std::string expanded = expand_format (prefix, fmt);

#ifdef CMDLINE_DEBUG_TIMESTAMPS
  std::fprintf (out, "[%08llu] ",
                static_cast<unsigned long long> (now_ms ()));
#endif

  va_list args;
  va_start (args, fmt);
  const int retval = std::vfprintf (out, expanded.c_str (), args);
  va_end (args);

  std::fflush (out);
  return retval;
}
