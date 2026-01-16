#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "coroed/api/task.h"
#include "coroed/core/mutex.h"

#define FIBER_COUNT 14          
#define ITERATIONS_PER_FIBER 1000 

static int shared_counter = 0;   
static struct mutex counter_mutex; 
static int completed_fibers = 0;
static struct mutex completion_mutex;

TASK_DECLARE(worker_fiber, int*, fiber_id_ptr);
TASK_DECLARE(main_fiber, void*, arg);

TASK_DEFINE(worker_fiber, int*, fiber_id_ptr) {
  int fiber_id = *fiber_id_ptr;

  printf("Файбер %d начал работу\n", fiber_id);

  for (int iteration = 0; iteration < ITERATIONS_PER_FIBER; iteration++) {
    MUTEX_LOCK(&counter_mutex);
    shared_counter++;
    MUTEX_UNLOCK(&counter_mutex);

    if (iteration % 200 == 0) {
      printf("Файбер %d: счетчик = %d (итерация %d)\n", 
             fiber_id, shared_counter, iteration);
      YIELD; 
    }

    for (int delay = 0; delay < 100; delay++) {} 
  }

  MUTEX_LOCK(&completion_mutex);
  completed_fibers++;
  printf("Файбер %d завершен. Завершено файберов: %d/%d\n", 
         fiber_id, completed_fibers, FIBER_COUNT);
  MUTEX_UNLOCK(&completion_mutex);

  free(fiber_id_ptr);
}

TASK_DEFINE(main_fiber, void*, arg) {
  mutex_init(&counter_mutex);
  mutex_init(&completion_mutex);

  printf("Создаем %d файберов...\n", FIBER_COUNT);
  
  for (int i = 0; i < FIBER_COUNT; i++) {
    int* fiber_id = malloc(sizeof(int));
    *fiber_id = i + 1;
    GO(worker_fiber, fiber_id);
    YIELD;
  }

  printf("\nЗапущено %d файберов. Ожидаем завершения...\n\n", FIBER_COUNT);

  while (1) {
    YIELD;

    MUTEX_LOCK(&completion_mutex);
    int completed = completed_fibers;
    MUTEX_UNLOCK(&completion_mutex);

    if (completed == FIBER_COUNT) {
      break;
    }

    for (int wait = 0; wait < 1000; wait++) {}
  }
  printf("Итоговое значение счетчика: %d\n", shared_counter);
  printf("Ожидаемое значение: %d\n", FIBER_COUNT * ITERATIONS_PER_FIBER);
  printf("Разница: %d\n", 
         FIBER_COUNT * ITERATIONS_PER_FIBER - shared_counter);

  if (shared_counter == FIBER_COUNT * ITERATIONS_PER_FIBER) {
    printf("\nМьютекс работает корректно!\n");
  } else {
    printf("\nОшибка синхронизации!\n");
  }
}

int main() {
  srand(time(NULL));
  tasks_init();
  tasks_submit(main_fiber, NULL);
  tasks_start();
  tasks_wait();
  tasks_print_statistics();
  tasks_destroy();

  return 0;
}