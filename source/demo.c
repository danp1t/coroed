#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "coroed/api/task.h"
#include "coroed/core/mutex.h"

#define THREAD_COUNT 14
#define ITERATIONS 1000

static int shared_counter = 0;
static struct mutex counter_mutex;
static int finished_threads = 0;
static struct mutex finish_mutex;

TASK_DECLARE(worker, int, thread_id);
TASK_DECLARE(main_task, void*, arg);

TASK_DEFINE(worker, int, thread_id) {
  int id = *thread_id;

  printf("Поток %d начал работу\n", id);

  for (int i = 0; i < ITERATIONS; i++) {
    MUTEX_LOCK(&counter_mutex);
    shared_counter++;
    MUTEX_UNLOCK(&counter_mutex);

    if (i % 200 == 0) {
      printf("Поток %d: счетчик = %d (итерация %d)\n", id, shared_counter, i);
      YIELD;
    }

    for (int j = 0; j < 100; j++) {
      // busy wait
    }
  }

  MUTEX_LOCK(&finish_mutex);
  finished_threads++;
  printf("Поток %d завершен. Завершено потоков: %d/%d\n", id, finished_threads, THREAD_COUNT);
  MUTEX_UNLOCK(&finish_mutex);

  free(thread_id);
}

TASK_DEFINE(main_task, void*, arg) {
  mutex_init(&counter_mutex);
  mutex_init(&finish_mutex);

  for (int i = 0; i < THREAD_COUNT; i++) {
    int* thread_id = malloc(sizeof(int));
    *thread_id = i + 1;
    GO(worker, thread_id);
    YIELD;
  }

  printf("\nЗапущено %d потоков. Ждем завершения...\n\n", THREAD_COUNT);

  while (1) {
    YIELD;

    MUTEX_LOCK(&finish_mutex);
    int finished = finished_threads;
    MUTEX_UNLOCK(&finish_mutex);

    if (finished == THREAD_COUNT) {
      break;
    }

    for (int i = 0; i < 1000; i++) {
      // busy wait
    }
  }

  printf("Итоговое значение счетчика: %d\n", shared_counter);
  printf("Ожидаемое значение: %d\n", THREAD_COUNT * ITERATIONS);

  if (shared_counter == THREAD_COUNT * ITERATIONS) {
    printf("\nМьютекс работает корректно!\n");
  } else {
    printf("\nОшибка синхронизации!\n");
    printf("Разница: %d\n", THREAD_COUNT * ITERATIONS - shared_counter);
  }
}

int main() {
  srand(time(NULL));

  printf("Запуск демонстрации мьютекса с файберами...\n\n");

  tasks_init();
  tasks_submit(main_task, NULL);
  tasks_start();
  tasks_wait();
  tasks_print_statistics();
  tasks_destroy();

  return 0;
}