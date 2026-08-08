
/* Loosely based on my old cmdline library that worked but
 * honestly wasn't very well written. Hopefully this version is better =)
 *
 * To use:
 *
 * #define CMDLINE_DEBUG
 * #include <cmdline/debug.h>
 *
 *   - delt.
 */

#ifndef NEURALBLENDER_CMDLINE_DEBUG_H
#define NEURALBLENDER_CMDLINE_DEBUG_H

#include <cstdio>
#include <cstdint>
#include <string>
#include <chrono>
#include <cstdarg>
#include <cstring>
#include <string_view>

inline constexpr const char *ANSI_BLACK        = "\x1B[0;30m";
inline constexpr const char *ANSI_DARK_RED     = "\x1B[0;31m";
inline constexpr const char *ANSI_DARK_GREEN   = "\x1B[0;32m";
inline constexpr const char *ANSI_DARK_YELLOW  = "\x1B[0;33m";
inline constexpr const char *ANSI_DARK_BLUE    = "\x1B[0;34m";
inline constexpr const char *ANSI_DARK_MAGENTA = "\x1B[0;35m";
inline constexpr const char *ANSI_DARK_CYAN    = "\x1B[0;36m";
inline constexpr const char *ANSI_GREY         = "\x1B[0;37m";
inline constexpr const char *ANSI_DARK_GREY    = "\x1B[1;30m";
inline constexpr const char *ANSI_RED          = "\x1B[1;31m";
inline constexpr const char *ANSI_GREEN        = "\x1B[1;32m";
inline constexpr const char *ANSI_YELLOW       = "\x1B[1;33m";
inline constexpr const char *ANSI_BLUE         = "\x1B[1;34m";
inline constexpr const char *ANSI_MAGENTA      = "\x1B[1;35m";
inline constexpr const char *ANSI_CYAN         = "\x1B[1;36m";
inline constexpr const char *ANSI_WHITE        = "\x1B[1;37m";
inline constexpr const char *ANSI_RESET        = "\x1B[0m";

#define cmdline_debugfps(a,b,c,d,e,f) { static c_cmdline_printfps p; \
        long int x=p.tick (); if (x>0) debug ("%s: %ld calls/sec.", f, x); }

namespace {

inline uint64_t _cmdline_now_ms () {
  using clock = std::chrono::steady_clock;
  return static_cast<uint64_t> (
    std::chrono::duration_cast<std::chrono::milliseconds> (
      clock::now ().time_since_epoch ()).count ());
}

class c_cmdline_printfps {
public:
  std::string m_str;
  uint64_t count = 0;
  uint64_t last = _cmdline_now_ms ();
  
  c_cmdline_printfps () { }
  c_cmdline_printfps (std::string str) : m_str (std::move (str)) { }
  
  inline long int tick () {
    count++;
    
    long int now = _cmdline_now_ms ();
    if (now - last >= 1000) {
      //std::cout << m_str << count << "\n";
      //debug ("%s: %ld calls per second", m_str.c_str (), (long int) count);
      //cmdline_debug (a,b,c,d,e,f);
      long int ret = count;
      count = 0;
      last = now;
      return ret;
    }
    return -1;
  }
};

} // namespace

#ifdef CMDLINE_DEBUG
#ifndef CMDLINE_DEBUG_COLOR
#define CMDLINE_DEBUG_COLOR ANSI_RED
#endif

#ifndef CMDLINE_DEBUG_TIMESTAMPS
#define CMDLINE_DEBUG_TIMESTAMPS
#endif

#ifndef __FUNC__
#define __FUNC__ __func__
//#define __FUNC__ __PRETTY_FUNCTION__
#endif

#if defined(__GNUC__) || defined(__clang__)
#define CMDLINE_PRINTF_FORMAT(fmt_index, first_arg) \
  __attribute__ ((format (printf, fmt_index, first_arg)))
#else
#define CMDLINE_PRINTF_FORMAT(fmt_index, first_arg)
#endif

static inline int cmdline_debug (std::FILE *out,
                   const char *color,
                   const char *file,
                   int line,
                   const char *func,
                   const char *fmt,
                   ...) CMDLINE_PRINTF_FORMAT (6, 7);

static inline const char *cmdline_basename (const char *path) {
  if (!path)
    return "";

#ifdef DEBUG_FULL_PATHS
  return path;
#else
  const char *slash = std::strrchr (path, '/');
  return slash ? slash + 1 : path;
#endif
}

static inline int cmdline_debug (
    std::FILE *out,
    const char *color,
    const char *file,
    int line,
    const char *func,
    const char *fmt,
    ...) {

  if (!out)
    out = stderr;

#ifdef CMDLINE_DEBUG_TIMESTAMPS
  std::fprintf (out, "[%08llu] ", static_cast<unsigned long long> (_cmdline_now_ms ()));
#endif

  std::fprintf (out, "%s[%s:%d %s] %s", color ? color : ANSI_DARK_GREY,
       cmdline_basename (file), line, func ? func : "", ANSI_RESET);

  va_list args;
  va_start (args, fmt);
  const int result = std::vfprintf (out, fmt ? fmt : "(null)", args);
  va_end (args);

  std::fputc ('\n', out);
  std::fflush (out);
  return result;
}

#undef CMDLINE_PRINTF_FORMAT

#define debug(...) cmdline_debug(stderr,CMDLINE_DEBUG_COLOR,__FILE__,__LINE__,__FUNC__,__VA_ARGS__)
#define CP         { debug("\x1B[1;37m____CHECKPOINT____\x1B[0m"); }
#define BP         { debug("\x1B[1;37m____BREAKPOINT____\x1B[0m"); getc(stdin); }
#define debugfps(f) { cmdline_debugfps(stderr,CMDLINE_DEBUG_COLOR,__FILE__,__LINE__,__FUNC__,f); }
#else
#define debug(...)    do {} while (0);
#define printfps(...) do {} while (0);
#define CP            do {} while (0);
#define BP            do {} while (0);
#endif

#endif
