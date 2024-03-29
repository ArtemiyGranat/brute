#include "client.h"

#include "brute.h"
#include "common.h"
#include "config.h"
#include "single.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

status_t
find_password (config_t *config, int socket_fd, task_t *task,
               st_context_t *ctx)
{
  if (brute (task, config, st_password_check, ctx))
    {
      print_error ("Found something\n");

      int password_size = sizeof (task->password);
      if (send_wrapper (socket_fd, &password_size, sizeof (password_size), 0)
          == S_FAILURE)
        {
          print_error ("Could not send data to server\n");
          return (S_FAILURE);
        }

      print_error ("Sent %d to server\n", password_size);

      if (send_wrapper (socket_fd, task->password, password_size, 0)
          == S_FAILURE)
        {
          print_error ("Could not send data to server\n");
          return (S_FAILURE);
        }

      print_error ("Sent %s to server\n", task->password);
    }

  return (S_SUCCESS);
}

bool
run_client (task_t *task, config_t *config)
{
  int socket_fd = socket (AF_INET, SOCK_STREAM, 0);
  if (socket_fd == -1)
    {
      print_error ("Could not initialize client socket\n");
      return (false);
    }

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr (config->addr);
  addr.sin_port = htons (config->port);

  if (connect (socket_fd, (struct sockaddr *)&addr, sizeof (addr)) == -1)
    {
      print_error ("Could not connect to server\n");
      return (false);
    }

  print_error ("Connected to server\n");

  char hash[HASH_LENGTH];
  if (recv_wrapper (socket_fd, hash, HASH_LENGTH, 0) == S_FAILURE)
    {
      print_error ("Could not receive hash from server\n");
      goto fail;
    }

  print_error ("Received hash from server\n");

  st_context_t st_context = {
    .hash = hash,
    .data = { .initialized = 0 },
  };

  while (true)
    {
      print_error ("Waiting for task\n");
      if (recv_wrapper (socket_fd, task, sizeof (task_t), 0) == S_FAILURE)
        {
          print_error ("Could not receive data from server\n");
          goto fail;
        }

      print_error ("Received task %s from server\n", task->password);

      // In RM_LOAD_CLIENT mode we should not search for a password
      if (config->run_mode == RM_CLIENT)
        {
          if (find_password (config, socket_fd, task, &st_context)
              == S_FAILURE)
            goto fail;

          if (task->password[0] != 0)
            return (true);
        }

      print_error ("Haven't found anything\n");

      int wrong_password = 0;
      if (send_wrapper (socket_fd, &wrong_password, sizeof (wrong_password), 0)
          == S_FAILURE)
        {
          print_error ("Could not send data to server\n");
          goto fail;
        }

      print_error ("Sent %d to server\n", wrong_password);
    }

fail:
  close (socket_fd);
  return (false);
}
