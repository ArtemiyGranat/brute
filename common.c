#include "common.h"

#include <stdarg.h>
#include <stdio.h>

__attribute__ ((format (printf, 1, 2))) status_t
print_error (const char *msg, ...)
{
  va_list args;
  va_start (args, msg);

  if (vfprintf (stderr, msg, args) != 0)
    return (S_FAILURE);

  va_end (args);

  return (S_SUCCESS);
}

void
cleanup_mutex_unlock (void *mutex)
{
  pthread_mutex_unlock ((pthread_mutex_t *)mutex);
}

int
create_threads (pthread_t *threads, int number_of_threads, void *func (void *),
                void *context)
{
  int active_threads = 0;
  for (int i = 0; i < number_of_threads; ++i)
    if (pthread_create (&threads[i], NULL, func, context) == 0)
      ++active_threads;

  if (active_threads == 0)
    print_error ("Could not create a single thread\n"); 

  return active_threads;
}
