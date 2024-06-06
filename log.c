#define _GNU_SOURCE
#include "log.h"
#include "common.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

__attribute__ ((format (printf, 4, 5))) status_t
message_impl (const char *file_name, const char *func_name, int line,
              const char *msg, ...)
{
  char log[1 << 7];
  if (sprintf (log, "(%s %s %d)", file_name, func_name, line) < 0)
    if (fprintf (stderr, "(%s %s %d)", file_name, func_name, line) < 0)
      return (S_FAILURE);

  va_list args;
  va_start (args, msg);

  char message[1 << 7];
  if (vsprintf (message, msg, args) < 0)
    if (vfprintf (stderr, msg, args) < 0)
      {
        return (S_FAILURE);
      }

  va_end (args);

  fprintf (stderr, "%s %s\n", log, message);

  fflush (stderr);

  return (S_SUCCESS);
}
