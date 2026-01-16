#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef enum { RB_RED, RB_BLACK } rb_color_t;

struct rb_node {
  struct rb_node* parent;
  struct rb_node* left;
  struct rb_node* right;
  rb_color_t color;
};

struct rb_root {
  struct rb_node* rb_node;
};

#define RB_ROOT      \
  (struct rb_root) { \
    NULL             \
  }

#define rb_entry(ptr, type, member) ((type*)((char*)(ptr) - offsetof(type, member)))

void rb_insert(
    struct rb_root* root, struct rb_node* node, int (*compare)(struct rb_node* a, struct rb_node* b)
);
void rb_erase(struct rb_node* node, struct rb_root* root);
struct rb_node* rb_first(struct rb_root* root);
struct rb_node* rb_last(struct rb_root* root);
struct rb_node* rb_next(struct rb_node* node);
struct rb_node* rb_prev(struct rb_node* node);