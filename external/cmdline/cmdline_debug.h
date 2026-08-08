
/* Loosely based on my old cmdline library that worked but
 * honestly wasn't very well written. Hopefully this version is better =)
 *
 *   - delt.
 */

#ifndef NEURALBLENDER_CMDLINE_DEBUG_H
#define NEURALBLENDER_CMDLINE_DEBUG_H

#include <cstdio>
#include <cstdint>
#include <string>
#include <chrono>

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

#define cmdline_printfps(a,b,c,d,e,f) { static c_cmdline_printfps p; \
        long int x=p.tick (); if (x>0) debug ("%s: %ld calls/sec.", f, x); }
        
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

#if defined(__GNUC__) || defined(__clang__)
int cmdline_debug (std::FILE *out,
                   const char *color,
                   const char *file,
                   int line,
                   const char *func,
                   const char *fmt,
                   ...) __attribute__((format(printf, 6, 7)));

#else

int cmdline_debug (std::FILE *out,
                   const char *color,
                   const char *file,
                   int line,
                   const char *func,
                   const char *fmt,
                   ...);
#endif

#endif
