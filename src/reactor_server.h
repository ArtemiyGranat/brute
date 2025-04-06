#ifndef REACTOR_SERVER_H
#define REACTOR_SERVER_H

#include "common.h"
#include "config.h"
#include "queue.h"
#include "server_common.h"

#include <event.h>
#include <event2/util.h>
#include <stdbool.h>

typedef struct rsrv_context_t
{
  serv_base_context_t context;
  queue_t jobs_queue;
  queue_t starving_clients;
  struct event_base *base;
  pthread_mutex_t mutex;
} rsrv_context_t;

typedef struct client_state_t
{
  struct iovec vec[5];
  size_t vec_sz;
  command_t cmd[2];
  int32_t length;
} client_state_t;

typedef struct client_context_t
{
  struct event *read_event;
  rsrv_context_t *context;
  evutil_socket_t socket_fd;
  bool is_starving;
  bool is_writing;
  bool registry_used[QUEUE_SIZE];
  task_t registry[QUEUE_SIZE];
  queue_t registry_idx;
  client_state_t write_state;
  client_state_t read_state;
  result_t read_buffer;
  pthread_mutex_t is_writing_mutex;
  pthread_mutex_t write_state_mutex;
  pthread_mutex_t is_starving_mutex;
  pthread_mutex_t registry_used_mutex;
} client_context_t;

typedef struct job_t
{
  void *arg;
  status_t (*job_func) (void *);
} job_t;

bool run_reactor_server (task_t *, config_t *);

#endif // REACTOR_SERVER_H
