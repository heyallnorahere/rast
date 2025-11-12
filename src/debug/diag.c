#include "diag.h"

#include "core/list.h"
#include "core/mem.h"
#include "debug/capture.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <cimgui.h>

#include <string.h>

struct capture_viewer {
    uint32_t id;
    capture_t* cap;

    uint32_t event_count;
    const struct capture_event** events;

    int32_t selected_index;
    uint32_t selected_attachment;
};

struct diag {
    struct list viewers;
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

    list_init(&s_diag->viewers);
}

static void diag_free_capture_viewer(void* data, void* user_data) {
    struct capture_viewer* viewer = data;

    capture_destroy(viewer->cap);
    mem_free(viewer->events);
    mem_free(viewer);
}

void diag_shutdown() {
    if (!s_diag) {
        return;
    }

    list_free_full(&s_diag->viewers, diag_free_capture_viewer, NULL);
    capture_destroy(s_diag->current_capture);

    mem_free(s_diag);
    s_diag = NULL;
}

static void diag_display_event(struct capture_viewer* viewer) {
    if (viewer->selected_index < 0) {
        igText("No capture event selected.");
        return;
    }

    const struct capture_event* event = viewer->events[viewer->selected_index];

    if (viewer->selected_attachment >= event->attachment_count) {
        viewer->selected_attachment = 0;
    }

    ImVec2 region_avail;
    igGetContentRegionAvail(&region_avail);

    image_t* snapshot = event->results[viewer->selected_attachment];
    ImTextureRef* ref = ImTextureRef_ImTextureRef_TextureID((ImTextureID)snapshot);

    ImVec2 uv0, uv1;
    uv0.x = uv0.y = 0.f;
    uv1.x = uv1.y = 1.f;

    igImage(*ref, region_avail, uv0, uv1);
    ImTextureRef_destroy(ref);
}

static void diag_show_capture(struct capture_viewer* viewer, bool* show) {
    char buf[256];
    snprintf(buf, 256, "Capture #%u", viewer->id + 1);

    igBegin(buf, show, ImGuiWindowFlags_None);
    igColumns(2, NULL, true);

    for (uint32_t i = 0; i < viewer->event_count; i++) {
        const struct capture_event* event = viewer->events[i];

        const char* event_name;
        switch (event->type) {
        case CAPTURE_EVENT_TYPE_FRAMEBUFFER_CLEAR:
            event_name = "Framebuffer clear";
            break;
        case CAPTURE_EVENT_TYPE_RENDER_CALL:
            event_name = "Render call";
            break;
        default:
            event_name = "Unknown event";
            break;
        }

        snprintf(buf, 256, "#%u: %s", i + 1, event_name);

        ImVec2 size;
        size.x = size.y = 0.f;

        if (igSelectable_Bool(buf, i == viewer->selected_index, ImGuiSelectableFlags_None, size)) {
            viewer->selected_index = i;
        }
    }

    igNextColumn();
    diag_display_event(viewer);

    igEnd();
}

static void diag_append_viewer(capture_t* cap) {
    struct capture_viewer* viewer = mem_alloc(sizeof(struct capture_viewer));
    viewer->cap = s_diag->current_capture;
    viewer->id = s_diag->current_id++;
    viewer->selected_index = -1;
    viewer->selected_attachment = 0;

    viewer->event_count = capture_get_events(viewer->cap, NULL);
    viewer->events = mem_alloc(viewer->event_count * sizeof(const struct capture_event*));
    capture_get_events(viewer->cap, viewer->events);

    list_append(&s_diag->viewers, viewer);
}

void diag_update() {
    if (!s_diag) {
        return;
    }

    // if the user pressed the "capture" button *last* frame, then the capture should be ready
    if (s_diag->current_capture) {
        diag_append_viewer(s_diag->current_capture);
        s_diag->current_capture = NULL;
    }

    {
        igBegin("Diagnostics", NULL, ImGuiWindowFlags_None);

        ImVec2 size;
        size.x = size.y = 0.f;

        if (igButton("Capture one frame", size)) {
            // the client should be looking for this to be set
            s_diag->current_capture = capture_new();
        }

        igEnd();
    }

    struct list_node* cur = s_diag->viewers.head;
    while (cur) {
        bool show = true;
        diag_show_capture(cur->data, &show);

        struct list_node* next = cur->next;
        if (!show) {
            diag_free_capture_viewer(cur->data, NULL);
            list_remove(&s_diag->viewers, cur);
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
