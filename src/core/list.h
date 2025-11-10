#ifndef LIST_H_
#define LIST_H_

struct list_node {
    struct list_node* next;
    struct list_node* prev;
    void* data;
};

struct list {
    struct list_node* head;
    struct list_node* tail;
};

void list_init(struct list* list);
void list_free(struct list* list);
void list_free_full(struct list* list, void (*free_func)(void* data, void* user_data),
                    void* user_data);

struct list_node* list_append(struct list* list, void* data);
struct list_node* list_prepend(struct list* list, void* data);

struct list_node* list_insert_after(struct list* list, struct list_node* node, void* data);
struct list_node* list_insert_before(struct list* list, struct list_node* node, void* data);

void list_remove(struct list* list, struct list_node* node);

#endif
