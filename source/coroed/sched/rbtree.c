#include "rbtree.h"

#include <assert.h>

static void rb_rotate_left(struct rb_node* x, struct rb_root* root);
static void rb_rotate_right(struct rb_node* x, struct rb_root* root);
static void rb_insert_fixup(struct rb_node* node, struct rb_root* root);
static void rb_erase_fixup(struct rb_node* node, struct rb_node* parent, struct rb_root* root);

// Вставка в красно-черное дерево, а кто говорил, что будет легко
void rb_insert(
    struct rb_root* root, struct rb_node* node, int (*compare)(struct rb_node* a, struct rb_node* b)
) {
  struct rb_node** current = &root->rb_node;
  struct rb_node* parent = NULL;

  // Находим место для вставки
  while (*current) {
    parent = *current;
    if (compare(node, parent) < 0) {
      current = &parent->left;
    } else {
      current = &parent->right;
    }
  }

  // Вставляем новый узел
  node->parent = parent;            // Папа
  node->left = node->right = NULL;  // Дети мои...
  node->color = RB_RED;
  *current = node;

  // Шаманские фокусы
  rb_insert_fixup(node, root);
}

void rb_erase(struct rb_node* node, struct rb_root* root) {
  struct rb_node *child, *parent;
  rb_color_t color;

  if (!node->left) {
    child = node->right;
  } else if (!node->right) {
    child = node->left;
  } else {
    struct rb_node* old = node;
    struct rb_node* left;

    node = node->right;
    while ((left = node->left) != NULL) {
      node = left;
    }

    child = node->right;
    parent = node->parent;
    color = node->color;

    if (child) {
      child->parent = parent;
    }

    if (parent) {
      if (parent->left == node) {
        parent->left = child;
      } else {
        parent->right = child;
      }
    } else {
      root->rb_node = child;
    }

    if (node->parent == old) {
      parent = node;
    }

    node->parent = old->parent;
    node->color = old->color;
    node->left = old->left;
    node->right = old->right;

    if (old->parent) {
      if (old->parent->left == old) {
        old->parent->left = node;
      } else {
        old->parent->right = node;
      }
    } else {
      root->rb_node = node;
    }

    old->left->parent = node;
    if (old->right) {
      old->right->parent = node;
    }

    goto color;
  }

  parent = node->parent;
  color = node->color;

  if (child) {
    child->parent = parent;
  }

  if (parent) {
    if (parent->left == node) {
      parent->left = child;
    } else {
      parent->right = child;
    }
  } else {
    root->rb_node = child;
  }

color:
  if (color == RB_BLACK) {
    rb_erase_fixup(child, parent, root);
  }
}

struct rb_node* rb_first(struct rb_root* root) {
  struct rb_node* node = root->rb_node;

  if (!node) {
    return NULL;
  }

  while (node->left) {
    node = node->left;
  }

  return node;
}

struct rb_node* rb_last(struct rb_root* root) {
  struct rb_node* node = root->rb_node;

  if (!node) {
    return NULL;
  }

  while (node->right) {
    node = node->right;
  }

  return node;
}

struct rb_node* rb_next(struct rb_node* node) {
  if (node->right) {
    node = node->right;
    while (node->left) {
      node = node->left;
    }
    return node;
  }

  struct rb_node* parent = node->parent;
  while (parent && node == parent->right) {
    node = parent;
    parent = parent->parent;
  }

  return parent;
}

struct rb_node* rb_prev(struct rb_node* node) {
  if (node->left) {
    node = node->left;
    while (node->right) {
      node = node->right;
    }
    return node;
  }

  struct rb_node* parent = node->parent;
  while (parent && node == parent->left) {
    node = parent;
    parent = parent->parent;
  }

  return parent;
}

static void rb_rotate_left(struct rb_node* x, struct rb_root* root) {
  struct rb_node* y = x->right;

  x->right = y->left;
  if (y->left) {
    y->left->parent = x;
  }

  y->parent = x->parent;

  if (!x->parent) {
    root->rb_node = y;
  } else if (x == x->parent->left) {
    x->parent->left = y;
  } else {
    x->parent->right = y;
  }

  y->left = x;
  x->parent = y;
}

static void rb_rotate_right(struct rb_node* y, struct rb_root* root) {
  struct rb_node* x = y->left;

  y->left = x->right;
  if (x->right) {
    x->right->parent = y;
  }

  x->parent = y->parent;

  if (!y->parent) {
    root->rb_node = x;
  } else if (y == y->parent->right) {
    y->parent->right = x;
  } else {
    y->parent->left = x;
  }

  x->right = y;
  y->parent = x;
}

static void rb_insert_fixup(struct rb_node* node, struct rb_root* root) {
  while (node != root->rb_node && node->parent->color == RB_RED) {
    if (node->parent == node->parent->parent->left) {
      struct rb_node* y = node->parent->parent->right;

      if (y && y->color == RB_RED) {
        node->parent->color = RB_BLACK;
        y->color = RB_BLACK;
        node->parent->parent->color = RB_RED;
        node = node->parent->parent;
      } else {
        if (node == node->parent->right) {
          node = node->parent;
          rb_rotate_left(node, root);
        }

        node->parent->color = RB_BLACK;
        node->parent->parent->color = RB_RED;
        rb_rotate_right(node->parent->parent, root);
      }
    } else {
      struct rb_node* y = node->parent->parent->left;

      if (y && y->color == RB_RED) {
        node->parent->color = RB_BLACK;
        y->color = RB_BLACK;
        node->parent->parent->color = RB_RED;
        node = node->parent->parent;
      } else {
        if (node == node->parent->left) {
          node = node->parent;
          rb_rotate_right(node, root);
        }

        node->parent->color = RB_BLACK;
        node->parent->parent->color = RB_RED;
        rb_rotate_left(node->parent->parent, root);
      }
    }
  }

  root->rb_node->color = RB_BLACK;
}

static void rb_erase_fixup(struct rb_node* node, struct rb_node* parent, struct rb_root* root) {
  while ((!node || node->color == RB_BLACK) && node != root->rb_node) {
    if (node == parent->left) {
      struct rb_node* w = parent->right;

      if (w->color == RB_RED) {
        w->color = RB_BLACK;
        parent->color = RB_RED;
        rb_rotate_left(parent, root);
        w = parent->right;
      }

      if ((!w->left || w->left->color == RB_BLACK) && (!w->right || w->right->color == RB_BLACK)) {
        w->color = RB_RED;
        node = parent;
        parent = node->parent;
      } else {
        if (!w->right || w->right->color == RB_BLACK) {
          if (w->left) {
            w->left->color = RB_BLACK;
          }
          w->color = RB_RED;
          rb_rotate_right(w, root);
          w = parent->right;
        }

        w->color = parent->color;
        parent->color = RB_BLACK;
        if (w->right) {
          w->right->color = RB_BLACK;
        }
        rb_rotate_left(parent, root);
        node = root->rb_node;
        break;
      }
    } else {
      struct rb_node* w = parent->left;

      if (w->color == RB_RED) {
        w->color = RB_BLACK;
        parent->color = RB_RED;
        rb_rotate_right(parent, root);
        w = parent->left;
      }

      if ((!w->right || w->right->color == RB_BLACK) && (!w->left || w->left->color == RB_BLACK)) {
        w->color = RB_RED;
        node = parent;
        parent = node->parent;
      } else {
        if (!w->left || w->left->color == RB_BLACK) {
          if (w->right) {
            w->right->color = RB_BLACK;
          }
          w->color = RB_RED;
          rb_rotate_left(w, root);
          w = parent->left;
        }

        w->color = parent->color;
        parent->color = RB_BLACK;
        if (w->left) {
          w->left->color = RB_BLACK;
        }
        rb_rotate_right(parent, root);
        node = root->rb_node;
        break;
      }
    }
  }

  if (node) {
    node->color = RB_BLACK;
  }
}