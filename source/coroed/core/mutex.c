#include "mutex.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "coroed/sched/schedy.h"

#define MAX_WAITING_NODES 256

static struct mutex_waiting_node waiting_pool[MAX_WAITING_NODES];
static int waiting_pool_used = 0;
static struct spinlock pool_lock;

static void init_pool(void) {
  static int initialized = 0;
  if (!initialized) {
    spinlock_init(&pool_lock);
    waiting_pool_used = 0;
    for (int i = 0; i < MAX_WAITING_NODES; i++) {
      waiting_pool[i].task = NULL;
      waiting_pool[i].next = NULL;
    }
    initialized = 1;
  }
}

static struct mutex_waiting_node* allocate_node(void) {
  init_pool();

  spinlock_lock(&pool_lock);

  for (int i = 0; i < MAX_WAITING_NODES; i++) {
    if (waiting_pool[i].task == NULL) {
      waiting_pool[i].task = (struct task*)(uintptr_t)0x1;
      waiting_pool[i].next = NULL;
      waiting_pool_used++;
      spinlock_unlock(&pool_lock);
      return &waiting_pool[i];
    }
  }

  spinlock_unlock(&pool_lock);

  struct mutex_waiting_node* node = malloc(sizeof(struct mutex_waiting_node));
  if (node) {
    node->task = NULL;
    node->next = NULL;
  }
  return node;
}

static void free_node(struct mutex_waiting_node* node) {
  if (node >= waiting_pool && node < waiting_pool + MAX_WAITING_NODES) {
    spinlock_lock(&pool_lock);
    node->task = NULL;
    node->next = NULL;
    waiting_pool_used--;
    spinlock_unlock(&pool_lock);
  } else {
    free(node);
  }
}

void mutex_init(struct mutex* mtx) {
  atomic_store(&mtx->locked, false);
  mtx->waiting_head = NULL;
  mtx->waiting_tail = NULL;
  spinlock_init(&mtx->lock);
}

static void add_to_waiting_queue(struct mutex* mtx, struct task* task) {
  struct mutex_waiting_node* node = allocate_node();
  if (!node) {
    return;
  }

  node->task = task;
  node->next = NULL;

  if (mtx->waiting_tail == NULL) {
    mtx->waiting_head = node;
    mtx->waiting_tail = node;
  } else {
    mtx->waiting_tail->next = node;
    mtx->waiting_tail = node;
  }
}

static struct task* remove_from_waiting_queue(struct mutex* mtx) {
  if (mtx->waiting_head == NULL) {
    return NULL;
  }

  struct mutex_waiting_node* node = mtx->waiting_head;
  struct task* task = node->task;

  mtx->waiting_head = node->next;
  if (mtx->waiting_head == NULL) {
    mtx->waiting_tail = NULL;
  }

  free_node(node);
  return task;
}

bool mutex_try_lock(struct mutex* mtx) {
  spinlock_lock(&mtx->lock);

  if (!atomic_load(&mtx->locked)) {
    atomic_store(&mtx->locked, true);
    spinlock_unlock(&mtx->lock);
    return true;
  }

  spinlock_unlock(&mtx->lock);
  return false;
}

void mutex_lock(struct mutex* mtx, struct task* task) {
  spinlock_lock(&mtx->lock);

  if (!atomic_load(&mtx->locked)) {
    atomic_store(&mtx->locked, true);
    spinlock_unlock(&mtx->lock);
    return;
  }

  add_to_waiting_queue(mtx, task);
  spinlock_unlock(&mtx->lock);

  sched_task_block(task);
  sched_switch_to_scheduler(task);
}

void mutex_unlock(struct mutex* mtx) {
  spinlock_lock(&mtx->lock);

  if (mtx->waiting_head != NULL) {
    struct task* next_task = remove_from_waiting_queue(mtx);
    if (next_task) {
      sched_task_unblock(next_task);
    }
    spinlock_unlock(&mtx->lock);
  } else {
    atomic_store(&mtx->locked, false);
    spinlock_unlock(&mtx->lock);
  }
}