#include "event.h"

#include "coroed/api/task.h"
#include "coroed/sched/schedy.h"

void event_init(struct event* event) {
  atomic_store(&event->is_fired, false);
}

void event_wait(struct task* caller, struct event* event) {
  while (!atomic_load(&event->is_fired)) {
    // Блокируем задачу и переходим к планировщику
    sched_task_block(caller);
    sched_switch_to_scheduler(caller);

    if (!atomic_load(&event->is_fired)) {
      continue;
    }
    break;
  }
}

void event_fire(struct event* event) {
  atomic_store(&event->is_fired, true);
}
