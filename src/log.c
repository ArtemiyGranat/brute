#define _GNU_SOURCE
#include "log.h"
#include "common.h"

#include <stdarg.h>
#include <stdio.h>

__attribute__ ((format (printf, 5, 6))) status_t
message_impl (log_level_t log_level, const char *file_name,
              const char *func_name, int line, const char *msg, ...)
{
#define LL_ENTRY(LEVEL) [LL_##LEVEL] = #LEVEL
  const char *log_level_str[]
      = { LL_ENTRY (TRACE), LL_ENTRY (DEBUG), LL_ENTRY (INFO),
          LL_ENTRY (WARN),  LL_ENTRY (ERROR), LL_ENTRY (FATAL) };

  char log_info[1 << 7];
  if (snprintf (log_info, sizeof (log_info), "[%s] (%s %s %d)",
                log_level_str[log_level], file_name, func_name, line)
      < 0)
    if (fprintf (stderr, "[%s] (%s %s %d)", log_level_str[log_level],
                 file_name, func_name, line)
        < 0)
      return (S_FAILURE);

  va_list args;
  va_start (args, msg);

  char message[1 << 7];
  if (vsnprintf (message, sizeof (message), msg, args) < 0)
    if (vfprintf (stderr, msg, args) < 0)
      {
        va_end (args);
        return (S_FAILURE);
      }

  va_end (args);

  fprintf (stderr, "%s %s\n", log_info, message);

  fflush (stderr);

  return (S_SUCCESS);
}
