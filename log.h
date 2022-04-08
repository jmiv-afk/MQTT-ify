/* ----------------------------------------------------------------------------
 * @file log.h
 * @brief Adds logging macro (to syslog or printf)
 *
 * @author Jake Michael, jami1063@colorado.edu
 * @resources None
 *---------------------------------------------------------------------------*/

#ifndef _LOG_H_
#define _LOG_H_

#include <syslog.h>
#include <stdio.h>

// by default logs should go to syslog, but can be optionally redirected 
// to printf for debug purposes by setting macro below to 1
#define REDIRECT_LOG_TO_PRINTF (1)

#if REDIRECT_LOG_TO_PRINTF
  #define LOG(LOG_LEVEL, msg, ...) printf(msg "\n", ##__VA_ARGS__)
#else
  #define LOG(LOG_LEVEL, msg, ...) syslog(LOG_LEVEL, msg, ##__VA_ARGS__)  
#endif

#endif // _LOG_H_
