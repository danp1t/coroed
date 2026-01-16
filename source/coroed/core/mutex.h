#pragma once

#include <stdatomic.h>
#include <stdbool.h>

#include "coroed/api/task.h"
#include "coroed/core/spinlock.h"

struct task;

struct mutex_waiting_node {
  struct task* task;
  struct mutex_waiting_node* next;
};

// Мьютекс, очередной примитив синхронизации
// Используем его, потому что файберу нужен системный поток для исполнения
// Из плюсов: Не тратим процессорное время, как на spinlock 
struct mutex {
  atomic_bool locked;                      
  struct mutex_waiting_node* waiting_head;
  struct mutex_waiting_node* waiting_tail;
  struct spinlock lock;
};

// Инициализация мьютекса
void mutex_init(struct mutex* mtx);

 // Функция для захвата мьютекса. 
 // Если мьютекс захвачен, то файбер блокируется и добавляется в очередь ожидания
void mutex_lock(struct mutex* mtx, struct task* task);

// Попробовать захватить мьютекс
bool mutex_try_lock(struct mutex* mtx);

// Функция для освобождения мьютекса
// Если в очереди ожидания есть файберы, то передаем первому из них мьтекс
void mutex_unlock(struct mutex* mtx);

#define MUTEX_LOCK(mtx) mutex_lock((mtx), __self)
#define MUTEX_UNLOCK(mtx) mutex_unlock((mtx))