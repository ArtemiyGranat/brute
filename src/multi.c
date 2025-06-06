#include "multi.h"

#include "brute.h"
#include "common.h"
#include "log.h"
#include "queue.h"
#include "single.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

status_t
mt_context_init (mt_context_t *context, config_t *config)
{
  if (pthread_mutex_init (&context->mutex, NULL) != 0)
    {
      error ("Could not initialize a mutex");
      return (S_FAILURE);
    }

  if (pthread_cond_init (&context->cond_sem, NULL) != 0)
    {
      error ("Could not initialize a condition variable");
      return (S_FAILURE);
    }

  if (queue_init (&context->queue, sizeof (task_t)) == QS_FAILURE)
    {
      error ("Could not initialize a queue");
      return (S_FAILURE);
    }

  if (thread_pool_init (&context->thread_pool) == S_FAILURE)
    {
      queue_destroy (&context->queue);
      return (S_FAILURE);
    }

  context->config = config;
  context->passwords_remaining = 0;
  context->password[0] = 0;

  return (S_SUCCESS);
}

status_t
mt_context_destroy (mt_context_t *context)
{
  trace ("Destroying multithreading context");
  if (thread_pool_cancel (&context->thread_pool) == S_FAILURE)
    {
      error ("Could not cancel a thread pool");
      return (S_FAILURE);
    }
  trace ("Cancelled thread pool, destroying global queue");
  if (queue_destroy (&context->queue) == QS_FAILURE)
    {
      error ("Could not destroy a queue");
      return (S_FAILURE);
    }
  trace (
      "Global queue is destroyed, destroying conditional semaphore and mutex");
  if (pthread_cond_destroy (&context->cond_sem) != 0)
    {
      error ("Could not destroy a condition variable");
      return (S_FAILURE);
    }

  if (pthread_mutex_destroy (&context->mutex) != 0)
    {
      error ("Could not destroy a mutex");
      return (S_FAILURE);
    }
  trace ("Destroyed multithreaded context");

  return (S_SUCCESS);
}

static status_t
trysignal (mt_context_t *ctx)
{
  if (pthread_mutex_lock (&ctx->mutex) != 0)
    {
      error ("Could not lock a mutex");
      return (S_FAILURE);
    }
  status_t status = S_SUCCESS;
  pthread_cleanup_push (cleanup_mutex_unlock, &ctx->mutex);

  if (--ctx->passwords_remaining == 0 || ctx->password[0] != 0)
    if (pthread_cond_signal (&ctx->cond_sem) != 0)
      {
        error ("Could not signal a condition");
        status = S_FAILURE;
      }

  pthread_cleanup_pop (!0);

  return (status);
}

void *
mt_password_check (void *context)
{
  mt_context_t *mt_ctx = *(mt_context_t **)context;
  task_t task;
  st_context_t st_ctx = {
    .hash = mt_ctx->config->hash,
    .data = { .initialized = 0 },
  };

  while (true)
    {
      if (queue_pop (&mt_ctx->queue, &task) != QS_SUCCESS)
        return (NULL);

      task.to = task.from;
      task.from = 0;

      if (brute (&task, mt_ctx->config, st_password_check, &st_ctx))
        memcpy (mt_ctx->password, task.result.password,
                sizeof (task.result.password));

      if (trysignal (mt_ctx) == S_FAILURE)
        return (NULL);
    }

  return (NULL);
}

bool
queue_push_wrapper (task_t *task, void *context)
{
  mt_context_t *mt_ctx = context;

  if (pthread_mutex_lock (&mt_ctx->mutex) != 0)
    {
      error ("Could not lock a mutex");
      return (false);
    }

  ++mt_ctx->passwords_remaining;

  if (pthread_mutex_unlock (&mt_ctx->mutex) != 0)
    {
      error ("Could not unlock a mutex");
      return (false);
    }

  queue_status_t push_status = queue_push (&mt_ctx->queue, task);
  if (push_status == QS_FAILURE)
    error ("Could not push to a queue");

  if (push_status != QS_SUCCESS)
    return (false);

  return (mt_ctx->password[0] != 0);
}

status_t
wait_password (mt_context_t *ctx)
{
  if (pthread_mutex_lock (&ctx->mutex) != 0)
    {
      error ("Could not lock a mutex");
      return (S_FAILURE);
    }
  status_t status = S_SUCCESS;
  pthread_cleanup_push (cleanup_mutex_unlock, &ctx->mutex);

  trace ("Waiting for password to be found or tasks to end");
  while (ctx->passwords_remaining != 0 && ctx->password[0] == 0)
    if (pthread_cond_wait (&ctx->cond_sem, &ctx->mutex) != 0)
      {
        error ("Could not wait on a condition");
        status = S_FAILURE;
      }

  trace ("Got signal on conditional semaphore, no password are remained or "
         "password was found");

  pthread_cleanup_pop (!0);

  return (status);
}

bool
run_multi (task_t *task, config_t *config)
{
  mt_context_t context;
  mt_context_t *context_ptr = &context;

  if (mt_context_init (&context, config) == S_FAILURE)
    return (false);

  int active_threads = create_threads (
      &context.thread_pool, config->number_of_threads, mt_password_check,
      &context_ptr, sizeof (context_ptr), "mt worker");

  if (active_threads == 0)
    goto fail;

  task->from = (config->length < 3) ? 1 : 2;
  task->to = config->length;

  brute (task, config, queue_push_wrapper, &context);

  if (wait_password (&context) == S_FAILURE)
    goto fail;

  if (queue_cancel (&context.queue) != QS_SUCCESS)
    {
      error ("Could not cancel a queue");
      goto fail;
    }

  if (context.password[0] != 0)
    memcpy (task->result.password, context.password,
            sizeof (context.password));

  if (mt_context_destroy (&context) == S_FAILURE)
    {
      error ("Could not destroy mt context");
      return (false);
    }

  return (context.password[0] != 0);

fail:
  if (mt_context_destroy (&context) == S_FAILURE)
    error ("Could not destroy mt context");

  return (false);
}
