#ifndef SPAWN_H
#define SPAWN_H

#include <stddef.h>
#include <sys/types.h>

struct spawn;

struct spawn* spawn_create(const char *exec, void *user_data);

void* spawn_user_data(struct spawn*);

pid_t spawn_pid(struct spawn*);

int spawn_exit_code(struct spawn*);

const char* spawn_arg_add(struct spawn*, const char *arg);

void spawn_env_set(struct spawn*, const char *key, const char *val);

void spawn_env_path_prepend(struct spawn*, const char *path);

void spawn_set_stdin_provider(struct spawn*, size_t (*provider)(char *buf, size_t buflen, struct spawn*));

void spawn_set_stdout_handler(struct spawn*, void (*handler)(char *buf, size_t buflen, struct spawn*));

void spawn_set_stderr_handler(struct spawn*, void (*handler)(char *buf, size_t buflen, struct spawn*));

int spawn_do(struct spawn*);

void spawn_destroy(struct spawn*);

#endif
