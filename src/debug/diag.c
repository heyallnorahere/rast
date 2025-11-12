#include "diag.h"

#include "core/list.h"
#include "core/mem.h"
#include "debug/capture.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <cimgui.h>

#include <string.h>

struct saved_capture {
    uint32_t id;
    capture_t* cap;
};

struct diag {
    struct list saved_captures;
    capture_t* current_capture;
    uint32_t current_id;
};

static struct diag* s_diag = NULL;

void diag_init() {
    if (s_diag) {
        return;
    }

    s_diag = mem_alloc(sizeof(struct diag));
    s_diag->current_capture = NULL;
    s_diag->current_id = 0;

    list_init(&s_diag->saved_captures);
}

static void diag_free_saved_capture(void* data, void* user_data) {
    struct saved_capture* saved = data;

    capture_destroy(saved->cap);
    mem_free(saved);
}

void diag_shutdown() {
    if (!s_diag) {
        return;
    }

    list_free_full(&s_diag->saved_captures, diag_free_saved_capture, NULL);
    capture_destroy(s_diag->current_capture);

    mem_free(s_diag);
    s_diag = NULL;
}

static void diag_show_capture(const struct saved_capture* saved, bool* show) {
    char buf[256];
    memset(buf, 0, 256);
    snprintf(buf, 256, "Capture #%u", saved->id + 1);
    igBegin(buf, show, ImGuiWindowFlags_None);

    igText("Placeholder, baby!");

    igEnd();
}

void diag_update() {
    if (!s_diag) {
        return;
    }

    if (s_diag->current_capture) {
        struct saved_capture* saved = mem_alloc(sizeof(struct saved_capture));
        saved->cap = s_diag->current_capture;
        saved->id = s_diag->current_id++;

        list_append(&s_diag->saved_captures, saved);
        s_diag->current_capture = NULL;
    }

    {
        igBegin("Diagnostics", NULL, ImGuiWindowFlags_None);

        ImVec2 size;
        size.x = size.y = 0.f;

        if (igButton("Capture one frame", size)) {
            s_diag->current_capture = capture_new();
        }

        igEnd();
    }

    struct list_node* cur = s_diag->saved_captures.head;
    while (cur) {
        bool show = true;
        diag_show_capture(cur->data, &show);

        struct list_node* next = cur->next;
        if (!show) {
            diag_free_saved_capture(cur->data, NULL);
            list_remove(&s_diag->saved_captures, cur);
        }

        cur = next;
    }
}

capture_t* diag_current_capture() {
    if (!s_diag) {
        return NULL;
    }

    return s_diag->current_capture;
}
