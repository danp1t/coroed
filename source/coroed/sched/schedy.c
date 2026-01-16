#include "schedy.h"

#include <assert.h>
#include <sched.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "coroed/api/task.h"
#include "coroed/core/relax.h"
#include "coroed/core/spinlock.h"
#include "kthread.h"
#include "rbtree.h"
#include "task_states.h"
#include "uthread.h"

enum {
  SCHED_THREADS_LIMIT = 512,  // Потолок по потокам
  SCHED_WORKERS_COUNT = 8,    // "Количество ядер"
  SCHED_NEXT_MAX_ATTEMPTS = 16,
};

struct task {
  struct uthread* thread;
  struct worker* worker;
  enum task_state state;
  struct spinlock lock;
  uint64_t vruntime;
  struct rb_node rb_node;  // Узел для красно-черного дерева
  bool in_tree;            // Флаг, указывающий, что задача в дереве
};

struct worker {
  size_t index;
  struct kthread kthread;
  struct uthread sched_thread;
  struct task* running_task;

  struct {
    size_t steps;     // Сколько шагов было выполнено
    size_t finished;  // Сколько задач было завершено
  } statistics;       // Локальная статистика работяги
};

static struct spinlock tasks_lock;
static struct task tasks[SCHED_THREADS_LIMIT];
static struct rb_root runnable_tree = RB_ROOT;  // Наше красно-белое =) дерево

static kthread_id_t kthread_ids[SCHED_WORKERS_COUNT];
static struct worker workers[SCHED_WORKERS_COUNT];

// Функция для выбора задачи, которую возьмут на cpu, то есть "кому справедливее будет отдать
// приоритет"
static int task_compare(struct rb_node* a, struct rb_node* b) {
  struct task* task_a = rb_entry(a, struct task, rb_node);
  struct task* task_b = rb_entry(b, struct task, rb_node);

  if (task_a->vruntime < task_b->vruntime) {
    return -1;
  } else if (task_a->vruntime > task_b->vruntime) {
    return 1;
  } else {
    return (uintptr_t)task_a < (uintptr_t)task_b ? -1 : 1;
  }
}

// Если задача готова выполнится, то значит надо её исполнять
// Вставляем в наше красно-черное дерево
static void sched_enqueue_task(struct task* task) {
  spinlock_lock(&tasks_lock);

  if (task->state == TASK_RUNNABLE && !task->in_tree) {
    task->in_tree = true;
    rb_insert(&runnable_tree, &task->rb_node, task_compare);
  }

  spinlock_unlock(&tasks_lock);
}

// Удаление задачи из красно-черного дерева
static void sched_dequeue_task(struct task* task) {
  spinlock_lock(&tasks_lock);

  if (task->in_tree) {
    task->in_tree = false;
    rb_erase(&task->rb_node, &runnable_tree);
  }

  spinlock_unlock(&tasks_lock);
}

// Инициализация задачи
void sched_task_init(struct task* task) {
  task->thread = NULL;
  task->worker = NULL;
  task->state = TASK_ZOMBIE;
  task->vruntime = 0;  // Сколько на cpu выполнялась задача
  task->in_tree = false;
  memset(&task->rb_node, 0, sizeof(task->rb_node));
  spinlock_init(&task->lock);
}

/**
 * Установить рабочего в пустое состояние.
 */
void sched_worker_init(struct worker* worker, size_t index) {
  worker->index = index;
  worker->sched_thread.context = NULL;
  worker->running_task = NULL;
  worker->statistics.steps = 0;
  worker->statistics.finished = 0;
}

void sched_init() {
  spinlock_init(&tasks_lock);
  runnable_tree = RB_ROOT;

  for (size_t i = 0; i < SCHED_THREADS_LIMIT; ++i) {
    sched_task_init(&tasks[i]);
  }

  for (size_t i = 0; i < SCHED_WORKERS_COUNT; ++i) {
    kthread_ids[i] = 0;
    sched_worker_init(&workers[i], i);
  }
}

/**
 * Находясь в контексте задачи `task`, переключиться
 * в контекст планировщика.
 */
void sched_switch_to_scheduler(struct task* task) {
  struct uthread* sched = &task->worker->sched_thread;
  task->worker = NULL;
  uthread_switch(task->thread, sched);
}

/**
 * Находясь в контексте планировщика, переключиться
 * в контекст задачи `task`. Выполнять ее до следующего
 * невынужденного возвращения в планировщик.
 */
void sched_switch_to(struct worker* worker, struct task* task) {
  assert(task->thread != &worker->sched_thread);

  task->state = TASK_RUNNING;
  task->worker = worker;
  worker->running_task = task;

  struct uthread* sched = &worker->sched_thread;
  uthread_switch(sched, task->thread);
}

/**
 * Получить следующую задачу на исполнение в
 * соответствии с текущим алгоритмом планирования.
 *
 * Вызывающему передается владение задачей, а также
 * захваченный лок `task->lock`.
 */
struct task* sched_acquire_next() {
  struct task* task = NULL;

  spinlock_lock(&tasks_lock);

  struct rb_node* node = rb_first(&runnable_tree);
  if (node) {
    task = rb_entry(node, struct task, rb_node);
    task->in_tree = false;
    rb_erase(node, &runnable_tree);
  }

  spinlock_unlock(&tasks_lock);

  return task;
}

/**
 * Вернуть задачу в очередь планирования.
 *
 * Очереди передается владение задачей,
 * а также она отпустит лок `task->lock`.
 */
void sched_release(struct task* task) {
  spinlock_lock(&task->lock);

  task->worker = NULL;

  switch (task->state) {
    case TASK_FINISHED:
      uthread_reset(task->thread);
      task->state = TASK_ZOMBIE;
      task->vruntime = 0;
      task->in_tree = false;
      break;

    case TASK_RUNNING:
      task->vruntime += 1000;
      task->state = TASK_RUNNABLE;
      sched_enqueue_task(task);
      break;

    default:
      break;
  }

  spinlock_unlock(&task->lock);
}

// Если задача завершилась, то удаляем её из дерева и "обнуляем её"
void sched_finish(struct task* task) {
  spinlock_lock(&task->lock);
  task->state = TASK_FINISHED;
  sched_dequeue_task(task);
  spinlock_unlock(&task->lock);
}

void task_yield(struct task* caller) {
  sched_switch_to_scheduler(caller);
}

void task_exit(struct task* caller) {
  sched_finish(caller);
  task_yield(caller);
}

task_t sched_try_submit(void (*entry)(), void* argument) {
  for (size_t i = 0; i < SCHED_THREADS_LIMIT; ++i) {
    struct task* task = &tasks[i];

    if (!spinlock_try_lock(&task->lock)) {
      continue;
    }

    if (task->thread == NULL) {
      task->thread = uthread_allocate();
      assert(task->thread != NULL);
      task->state = TASK_ZOMBIE;
      task->vruntime = 0;
      task->in_tree = false;
    }

    const bool is_submitted = task->state == TASK_ZOMBIE;

    if (task->state == TASK_ZOMBIE) {
      uthread_reset(task->thread);
      uthread_set_entry(task->thread, entry);
      uthread_set_arg_0(task->thread, task);
      uthread_set_arg_1(task->thread, argument);
      task->state = TASK_RUNNABLE;

      uint64_t min_vruntime = -1000;

      spinlock_lock(&tasks_lock);
      struct rb_node* node = rb_first(&runnable_tree);
      if (node) {
        struct task* first_task = rb_entry(node, struct task, rb_node);
        min_vruntime = first_task->vruntime;
      }
      spinlock_unlock(&tasks_lock);

      task->vruntime = min_vruntime;
      sched_enqueue_task(task);
    }

    spinlock_unlock(&task->lock);
    if (is_submitted) {
      return (task_t){.task = task};
    }
  }

  return (task_t){.task = NULL};
}

task_t sched_submit(void (*entry)(), void* argument) {
  for (size_t attempt = 0; attempt < SCHED_NEXT_MAX_ATTEMPTS; ++attempt) {
    task_t handle = sched_try_submit(entry, argument);
    if (handle.task != NULL) {
      return handle;
    }
    SPINLOOP(2 * attempt);
  }

  assert(false && "Can't create a task");
}

task_t task_submit(struct task* caller, uthread_routine entry, void* argument) {
  (void)caller;
  task_t child = sched_submit(entry, argument);
  return child;
}

/**
 * Цикл планировщика. Выполняется, пока есть задачи.
 */
int sched_loop(void* argument) {
  struct worker* worker = argument;
  kthread_ids[worker->index] = kthread_id();

  for (;;) {
    struct task* task = sched_acquire_next();
    if (task == NULL) {
      break;
    }

    sched_switch_to(worker, task);

    worker->statistics.steps += 1;
    if (task->state == TASK_FINISHED) {
      worker->statistics.finished += 1;
    }

    sched_release(task);
  }

  return 0;
}

void sched_start() {
  for (size_t i = 0; i < SCHED_WORKERS_COUNT; ++i) {
    struct worker* worker = &workers[i];
    enum kthread_status status = kthread_create(&worker->kthread, sched_loop, worker);
    assert(status == KTHREAD_SUCCESS);
  }
}

void sched_wait() {
  for (size_t i = 0; i < SCHED_WORKERS_COUNT; ++i) {
    struct worker* worker = &workers[i];
    enum kthread_status status = kthread_join(&worker->kthread);
    assert(status == KTHREAD_SUCCESS);
  }
}

void sched_print_statistics() {
  printf("\nsched statistics\n");

  size_t tasks_count = 0;
  size_t steps_count = 0;
  for (size_t i = 0; i < SCHED_WORKERS_COUNT; ++i) {
    struct worker* worker = &workers[i];
    tasks_count += worker->statistics.finished;
    steps_count += worker->statistics.steps;
  }

  printf("|- tasks executed %zu\n", tasks_count);
  printf("|- steps done     %zu\n", steps_count);

  for (size_t i = 0; i < SCHED_WORKERS_COUNT; ++i) {
    struct worker* worker = &workers[i];
    printf("|- worker %zu %zu\n", i, kthread_ids[i]);
    printf("   |- steps     %zu\n", worker->statistics.steps);
    printf("   |- finished  %zu\n", worker->statistics.finished);
  }
}

void sched_destroy() {
  for (size_t i = 0; i < SCHED_THREADS_LIMIT; ++i) {
    struct task* task = &tasks[i];
    spinlock_lock(&task->lock);
    if (task->thread != NULL) {
      uthread_free(task->thread);
    }
    spinlock_unlock(&task->lock);
  }
}

void sched_task_block(struct task* task) {
  spinlock_lock(&task->lock);
  task->state = TASK_BLOCKED;
  sched_dequeue_task(task);
  spinlock_unlock(&task->lock);
}

void sched_task_unblock(struct task* task) {
  spinlock_lock(&task->lock);
  if (task->state == TASK_BLOCKED) {
    task->state = TASK_RUNNABLE;

    // Немного "скостим срок" задаче, чтобы она была выше по приориетету
    // Тем самым повысим работоспособность системы, если основной упор делается на ввод-вывод, а не
    // на вычисления (как Столингс учил)
    if (task->vruntime > 1000) {
      task->vruntime -= 1000;
    }

    sched_enqueue_task(task);
  }
  spinlock_unlock(&task->lock);
}