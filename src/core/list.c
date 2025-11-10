#include "list.h"

#include "core/mem.h"

void list_init(struct list* list) { list->head = list->tail = NULL; }

void list_free(struct list* list) {
    struct list_node* current = list->head;
    while (current != NULL) {
        struct list_node* next = current->next;

        mem_free(current);
        current = next;
    }

    // reset
    list_init(list);
}

void list_free_full(struct list* list, void (*free_func)(void* data, void* user_data),
                    void* user_data) {
    struct list_node* current = list->head;
    while (current != NULL) {
        struct list_node* next = current->next;

        free_func(current->data, user_data);
        mem_free(current);

        current = next;
    }

    // reset
    list_init(list);
}

static struct list_node* list_node_alloc(void* data) {
    struct list_node* node = mem_alloc(sizeof(struct list_node));

    node->next = node->prev = NULL;
    node->data = data;

    return node;
}

struct list_node* list_append(struct list* list, void* data) {
    struct list_node* new_node = list_node_alloc(data);

    new_node->prev = list->tail;
    list->tail = new_node;

    if (!list->head) {
        list->head = new_node;
    }

    return new_node;
}

struct list_node* list_prepend(struct list* list, void* data) {
    struct list_node* new_node = list_node_alloc(data);

    new_node->next = list->head;
    list->head = new_node;

    if (!list->tail) {
        list->tail = new_node;
    }

    return new_node;
}

struct list_node* list_insert_after(struct list* list, struct list_node* node, void* data) {
    struct list_node* new_node = list_node_alloc(data);

    new_node->prev = node;
    new_node->next = node->next;
    node->next = new_node;

    if (list->tail == node) {
        list->tail = new_node;
    }

    return new_node;
}

struct list_node* list_insert_before(struct list* list, struct list_node* node, void* data) {
    struct list_node* new_node = list_node_alloc(data);

    new_node->next = node;
    new_node->prev = node->prev;
    node->prev = new_node;

    if (list->head == node) {
        list->head = node;
    }

    return new_node;
}

void list_remove(struct list* list, struct list_node* node) {
    if (node->prev) {
        node->prev->next = node->next;
    }

    if (node->next) {
        node->next->prev = node->prev;
    }

    if (list->head == node) {
        list->head = node->next;
    }

    if (list->tail == node) {
        list->tail = node->prev;
    }

    mem_free(node);
}
