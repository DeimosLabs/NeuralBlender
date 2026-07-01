
#ifdef CMDLINE_DEBUG
#ifndef CMDLINE_DEBUG_COLOR
#define CMDLINE_DEBUG_COLOR ANSI_RED
#endif

#ifndef CMDLINE_DEBUG_TIMESTAMPS
#define CMDLINE_DEBUG_TIMESTAMPS
#endif

#ifndef __FUNC__
#define __FUNC__ __PRETTY_FUNCTION__
#endif

#include "../external/cmdline/cmdline_debug.h"
#define debug(...) cmdline_debug(stderr,CMDLINE_DEBUG_COLOR,__FILE__,__LINE__,__FUNC__,__VA_ARGS__)
#define CP         { debug("\x1B[1;37m____CHECKPOINT____\x1B[0m"); }
#define BP         { debug("\x1B[1;37m____BREAKPOINT____\x1B[0m"); getc(stdin); }
#else
#define debug(...) do {} while (0)
#define CP         do {} while (0);
#define BP         do {} while (0);
#endif
