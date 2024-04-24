#include "sync_server.h"

#include "brute.h"
#include "common.h"
#include "multi.h"
#include "thread_pool.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static status_t
delegate_task (int socket_fd, task_t *task, mt_context_t *ctx)
{
  command_t cmd = CMD_TASK;

  if (send_wrapper (socket_fd, &cmd, sizeof (cmd), 0) == S_FAILURE)
    {
      print_error ("Could not send CMD_TASK to client\n");
      return (S_FAILURE);
    }

  if (send_wrapper (socket_fd, task, sizeof (*task), 0) == S_FAILURE)
    {
      print_error ("Could not send task to client\n");
      return (S_FAILURE);
    }

  int32_t size;
  if (recv_wrapper (socket_fd, &size, sizeof (size), 0) == S_FAILURE)
    {
      print_error ("Could not receive password size from client\n");
      return (S_FAILURE);
    }

  if (size != 0)
    {
      if (recv_wrapper (socket_fd, ctx->password, sizeof (password_t), 0)
          == S_FAILURE)
        {
          print_error ("Could not receive password from client\n");
          return (S_FAILURE);
        }

      if (queue_cancel (&ctx->queue) == QS_FAILURE)
        {
          print_error ("Could not cancel a queue\n");
          return (S_FAILURE);
        }
    }

  return (S_SUCCESS);
}

static void *
handle_client (void *arg)
{
  cl_context_t *cl_ctx = (cl_context_t *)arg;
  mt_context_t *mt_ctx = &cl_ctx->context->context;

  if (send_hash (cl_ctx->socket_fd, mt_ctx) == S_FAILURE)
    return (NULL);

  if (send_alph (cl_ctx->socket_fd, mt_ctx) == S_FAILURE)
    return (NULL);

  while (true)
    {
      task_t task;
      if (queue_pop (&mt_ctx->queue, &task) != QS_SUCCESS)
        return (NULL);

      task.to = task.from;
      task.from = 0;

      if (delegate_task (cl_ctx->socket_fd, &task, mt_ctx) == S_FAILURE)
        {
          if (queue_push (&mt_ctx->queue, &task) == QS_FAILURE)
            print_error ("Could not push to the queue\n");

          return (NULL);
        }

      if (pthread_mutex_lock (&mt_ctx->mutex) != 0)
        {
          print_error ("Could not lock a mutex\n");
          return (NULL);
        }
      pthread_cleanup_push (cleanup_mutex_unlock, &mt_ctx->mutex);

      if (--mt_ctx->passwords_remaining == 0 || mt_ctx->password[0] != 0)
        {
          close_client (cl_ctx->socket_fd);

          if (pthread_cond_signal (&mt_ctx->cond_sem) != 0)
            {
              print_error ("Could not signal a condition\n");
              return (NULL);
            }
        }

      pthread_cleanup_pop (!0);
    }

  return (NULL);
}

static void *
handle_clients (void *arg)
{
  serv_context_t *serv_ctx = *(serv_context_t **)arg;
  mt_context_t *mt_ctx = &serv_ctx->context;

  cl_context_t cl_ctx = {
    .context = serv_ctx,
  };

  while (true)
    {
      if ((cl_ctx.socket_fd = accept (serv_ctx->socket_fd, NULL, NULL)) == -1)
        {
          print_error ("Could not accept new connection\n");
          continue;
        }

      if (thread_create (&mt_ctx->thread_pool, handle_client, &cl_ctx,
                         sizeof (cl_ctx))
          == S_FAILURE)
        {
          print_error ("Could not create client thread\n");

          close_client (cl_ctx.socket_fd);
          continue;
        }
    }

  return (NULL);
}

bool
run_server (task_t *task, config_t *config)
{
  serv_context_t context;
  serv_context_t *context_ptr = &context;

  if (serv_context_init (&context, config) == S_FAILURE)
    {
      print_error ("Could not initialize server context\n");
      return (false);
    }

  if (thread_create (&context.context.thread_pool, handle_clients,
                     &context_ptr, sizeof (context_ptr))
      == S_FAILURE)
    {
      print_error ("Could not create clients thread\n");
      goto fail;
    }

  task->from = (config->length < 3) ? 1 : 2;
  task->to = config->length;

  mt_context_t *mt_ctx = (mt_context_t *)&context;

  brute (task, config, queue_push_wrapper, mt_ctx);

  if (wait_password (mt_ctx) == S_FAILURE)
    goto fail;

  if (queue_cancel (&mt_ctx->queue) == QS_FAILURE)
    {
      print_error ("Could not cancel a queue\n");
      goto fail;
    }

  if (mt_ctx->password[0] != 0)
    memcpy (task->task.password, mt_ctx->password, sizeof (mt_ctx->password));

  if (serv_context_destroy (&context) == S_FAILURE)
    print_error ("Could not destroy server context\n");

  return (mt_ctx->password[0] != 0);

fail:
  if (serv_context_destroy (&context) == S_FAILURE)
    print_error ("Could not destroy server context\n");

  return (false);
}