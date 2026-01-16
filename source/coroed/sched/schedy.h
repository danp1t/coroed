#pragma once

#include "coroed/api/task.h"
#include "uthread.h"

struct task;

void sched_init();

void sched_start();
void sched_wait();
void sched_print_statistics();
void sched_destroy();

void sched_task_block(struct task* task);
void sched_task_unblock(struct task* task);
void sched_switch_to_scheduler(struct task* task);

void task_yield(struct task* caller);
void task_exit(struct task* caller);
task_t task_submit(struct task* caller, uthread_routine entry, void* argument);