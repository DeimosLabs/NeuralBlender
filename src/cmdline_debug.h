
/* 
 * let's solve these debug macro shenanigans ONCE AND FOR ALL
 *
 * #define CMDLINE_DEBUG_COLOR to your liking for each file, then
 * #incude this
 *
 * as always #define CMDLINE_IMPLEMENTATION should be
 * before this for ONE implementation file per linking
 * object.
 */
 
#ifdef CMDLINE_DEBUG
#ifndef CMDLINE_DEBUG_COLOR
#define CMDLINE_DEBUG_COLOR ANSI_RED
#endif

#ifndef CMDLINE_DEBUG_TIMESTAMPS
#define CMDLINE_DEBUG_TIMESTAMPS
#endif

#include "cmdline/cmdline.h"
#define debug(...) cmdline_debug(stderr,CMDLINE_DEBUG_COLOR,__FILE__,__LINE__,__FUNC__,__VA_ARGS__)
#else
#define debug(...) do{}while(0);
#define CP         do{}while(0);
#define BP         do{}while(0);
#endif
