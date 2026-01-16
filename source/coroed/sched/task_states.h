#pragma once

/**
 * Состояния задач для планировщика.
 */
enum task_state { TASK_RUNNABLE, TASK_RUNNING, TASK_FINISHED, TASK_ZOMBIE, TASK_BLOCKED };