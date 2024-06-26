#include "async_client.h"
#include "async_server.h"
#include "brute.h"
#include "common.h"
#include "config.h"
#include "gen.h"
#include "log.h"
#include "multi.h"
#include "queue.h"
#include "single.h"
#include "sync_client.h"
#include "sync_server.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void
usage (char *first_arg)
{
  fprintf (stderr,
           "usage: %s [-l length] [-a alphabet] [-h hash] [-t number] "
           "[-p port] [-A addr] [-T timeout] "
           "[-s | -m | -g | -c | -L number | -S | -v | -w] [-i | -r"
#ifndef __APPLE__
           " | -y"
#endif
           "]\n"
           "options:\n"
           "\t-l length    password length\n"
           "\t-a alphabet  alphabet\n"
           "\t-h hash      hash\n"
           "\t-t number    number of threads\n"
           "\t-p port      server port\n"
           "\t-A addr      server address\n"
           "\t-T timeout   timeout between task receiving and its processing "
           "in milliseconds\n"
           "run modes:\n"
           "\t-s           singlethreaded mode\n"
           "\t-m           multithreaded mode\n"
           "\t-g           generator mode\n"
           "\t-c           synchronous client mode\n"
           "\t-L number    spawn N load clients\n"
           "\t-S           synchronous server mode\n"
           "\t-v           asynchronous client mode\n"
           "\t-w           asynchronous server mode\n"
           "brute modes:\n"
           "\t-i           iterative bruteforce\n"
           "\t-r           recursive bruteforce\n"
#ifndef __APPLE__
           "\t-y           recursive generator\n"
#endif
           ,
           first_arg);
}

static status_t
parse_params (config_t *config, int argc, char *argv[])
{
  int opt = 0;
  while ((opt = getopt (argc, argv, "l:a:H:t:p:A:L:T:smgcSvwiryh")) != -1)
    {
      switch (opt)
        {
        case 'l':
          config->length = atoi (optarg);
          if (config->length <= 0 || config->length > MAX_PASSWORD_LENGTH)
            {
              error ("Password's length must be a number between 0 and %d",
                     MAX_PASSWORD_LENGTH);
              return (S_FAILURE);
            }
          break;
        case 'a':
          config->alph = optarg;
          if (strlen (config->alph) <= 0
              || strlen (config->alph) > MAX_ALPH_LENGTH)
            {
              error ("Alphabet's length must be between 0 and %d",
                     MAX_ALPH_LENGTH);
              return (S_FAILURE);
            }
          break;
        case 'H':
          config->hash = optarg;
          break;
        case 't':
          {
            int number_of_cpus = sysconf (_SC_NPROCESSORS_ONLN);
            config->number_of_threads = atoi (optarg);
            if (config->number_of_threads < 1
                || config->number_of_threads > number_of_cpus)
              {
                error ("Number of threads must be a number between 1 and %d",
                       number_of_cpus);
                return (S_FAILURE);
              }
            break;
          }
        case 'p':
          config->port = atoi (optarg);
          if (config->length <= 0 || config->length > MAX_TCP_PORT)
            {
              error ("Port must be a number between 0 and %d", MAX_TCP_PORT);
              return (S_FAILURE);
            }
          break;
        case 'A':
          config->addr = optarg;
          break;
        case 'T':
          config->timeout = atoi (optarg);
          if (config->timeout < 0)
            {
              error ("Timeout must be a number greater than 0");
              return (S_FAILURE);
            }
          break;
        case 's':
          config->run_mode = RM_SINGLE;
          break;
        case 'm':
          config->run_mode = RM_MULTI;
          break;
        case 'g':
          config->run_mode = RM_GENERATOR;
          break;
        case 'c':
          config->run_mode = RM_CLIENT;
          break;
        case 'L':
          config->number_of_threads = atoi (optarg);
          if (config->number_of_threads < 1)
            {
              error ("Number of load clients to spawn must be a number "
                     "greater than 1");
              return (S_FAILURE);
            }
          config->run_mode = RM_LOAD_CLIENT;
          break;
        case 'S':
          config->run_mode = RM_SERVER;
          break;
        case 'v':
          config->run_mode = RM_ASYNC_CLIENT;
          break;
        case 'w':
          config->run_mode = RM_ASYNC_SERVER;
          break;
        case 'i':
          config->brute_mode = BM_ITER;
          break;
        case 'r':
          config->brute_mode = BM_RECU;
          break;
        case 'y':
          config->brute_mode = BM_REC_GEN;
          break;
        case 'h':
          usage (argv[0]);
          exit (EXIT_SUCCESS);
        default:
          return (S_FAILURE);
        }
    }

  return (S_SUCCESS);
}

int
main (int argc, char *argv[])
{
  config_t config = {
    .run_mode = RM_SINGLE,
    .brute_mode = BM_ITER,
    .length = 3,
    .number_of_threads = sysconf (_SC_NPROCESSORS_ONLN),
    .alph = "abc",
    .hash = "abFZSxKKdq5s6", /* crypt ("abc", "abc"); */
    .port = 9000,
    .addr = "127.0.0.1",
    .timeout = 0,
  };

  if (parse_params (&config, argc, argv) == S_FAILURE)
    {
      usage (argv[0]);
      return (EXIT_FAILURE);
    }

  task_t task;
  memset (&task, 0, sizeof (task));

  bool is_found = false;
  switch (config.run_mode)
    {
    case RM_SINGLE:
      is_found = run_single (&task, &config);
      break;
    case RM_MULTI:
      is_found = run_multi (&task, &config);
      break;
    case RM_GENERATOR:
      is_found = run_generator (&task, &config);
      break;
    case RM_ASYNC_SERVER:
      is_found = run_async_server (&task, &config);
      break;
    case RM_SERVER:
      is_found = run_server (&task, &config);
      break;
    case RM_ASYNC_CLIENT:
      run_async_client (&config);
      break;
    case RM_CLIENT:
      run_client (&config, find_password);
      break;
    case RM_LOAD_CLIENT:
      spawn_clients (&config, NULL);
      break;
    }

  /* Clients should not output anything, only computations and data exchange
   * with the server */
  if (config.run_mode == RM_CLIENT || config.run_mode == RM_LOAD_CLIENT
      || config.run_mode == RM_ASYNC_CLIENT)
    return (EXIT_SUCCESS);

  if (is_found)
    printf ("Password found: %s\n", task.task.password);
  else
    printf ("Password not found\n");

  return (EXIT_SUCCESS);
}
