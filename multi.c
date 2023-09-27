#include "multi.h"

#include "brute.h"
#include "queue.h"
#include "single.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// TODO: Refactor it using task->from and task->to params. Smth like task.to = task.from && task.from = 0

void *
mt_password_check (void *context)
{
  mt_context_t *mt_context = (mt_context_t *)context;
  task_t task;
  st_context_t st_context = {
    .hash = mt_context->config->hash,
    .data = { .initialized = 0 },
  };

  while (true)
    {
      if (queue_pop (&mt_context->queue, &task) == S_FAILURE)
        return (NULL);

      if (st_password_check (&task, &st_context))
        memcpy (mt_context->password, task.password, sizeof (task.password));

      if (pthread_mutex_lock (&mt_context->mutex) != 0)
        {
          print_error ("Could not lock a mutex\n");
          return (NULL);
        }
      pthread_cleanup_push (cleanup_mutex_unlock, &mt_context->mutex);

      --mt_context->passwords_remaining;
      if (mt_context->passwords_remaining == 0 || mt_context->password[0] != 0)
        if (pthread_cond_signal (&mt_context->cond_sem) != 0)
          {
            print_error ("Could not signal a condition\n");
            return (NULL);
          }

      pthread_cleanup_pop (!0);
    }
  return (NULL);
}

bool
queue_push_wrapper (task_t *task, void *context)
{
  mt_context_t *mt_context = (mt_context_t *)context;

  if (pthread_mutex_lock (&mt_context->mutex) != 0)
    {
      print_error ("Could not lock a mutex\n");
      return (false);
    }

  ++mt_context->passwords_remaining;

  if (pthread_mutex_unlock (&mt_context->mutex) != 0)
    {
      print_error ("Could not unlock a mutex\n");
      return (false);
    }

  if (queue_push (&mt_context->queue, task) == S_FAILURE)
    {
      print_error ("Could not push to a queue\n");
      return (false);
    }

  return (mt_context->password[0] != 0);
}

bool
run_multi (task_t *task, config_t *config)
{
  mt_context_t context;

  if (pthread_mutex_init (&context.mutex, NULL) != 0)
    {
      print_error ("Could not initialize a mutex\n");
      return (false);
    }

  if (pthread_cond_init (&context.cond_sem, NULL) != 0)
    {
      print_error ("Could not initialize a condition variable\n");
      return (false);
    }

  if (queue_init (&context.queue) == S_FAILURE)
    {
      print_error ("Could not initialize a queue\n");
      return (false);
    }

  context.config = config;
  context.passwords_remaining = 0;
  context.password[0] = 0;

  pthread_t threads[config->number_of_threads];
  int active_threads = 0;

  for (int i = 0; i < config->number_of_threads; ++i)
    if (pthread_create (&threads[i], NULL, mt_password_check, (void *)&context)
        == 0)
      ++active_threads;

  if (active_threads == 0)
    {
      print_error ("Could not create a single thread\n");
      return (false);
    }

  brute (task, config, queue_push_wrapper, &context);

  if (pthread_mutex_lock (&context.mutex) != 0)
    {
      print_error ("Could not lock a mutex\n");
      return (false);
    }
  pthread_cleanup_push (cleanup_mutex_unlock, &context.mutex);

  while (context.passwords_remaining != 0 && context.password[0] == 0)
    if (pthread_cond_wait (&context.cond_sem, &context.mutex) != 0)
      {
        print_error ("Could not wait on a condition\n");
        return (false);
      }

  pthread_cleanup_pop (!0);

  if (queue_cancel (&context.queue) == S_FAILURE)
    {
      print_error ("Could not cancel a queue\n");
      return (false);
    }

  for (int i = 0; i < active_threads; ++i)
    pthread_join (threads[i], NULL);

  if (context.password[0] != 0)
    memcpy (task->password, context.password, sizeof (context.password));

  if (queue_destroy (&context.queue) == S_FAILURE)
    {
      print_error ("Could not destroy a queue\n");
      return (false);
    }

  if (pthread_cond_destroy (&context.cond_sem) != 0)
    {
      print_error ("Could not destroy a condition variable\n");
      return (false);
    }

  if (pthread_mutex_destroy (&context.mutex) != 0)
    {
      print_error ("Could not destroy a mutex\n");
      return (false);
    }

  return (context.password[0] != 0);
}
