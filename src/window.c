#include "window.h"

#include "mem.h"

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>

#include <glib.h>

typedef struct video_data {
    uint32_t references;

    GHashTable* windows;
} video_data_t;

static video_data_t* s_video_data = NULL;

struct window {
    SDL_Window* window;

    bool close_requested;
};

static guint window_hash_id(gconstpointer key) {
    // guh
    size_t id = (size_t)key;

    return (guint)id;
}

static gboolean window_id_equal(gconstpointer lhs, gconstpointer rhs) { return lhs == rhs; }

static bool video_add_ref() {
    if (s_video_data) {
        s_video_data->references++;
        return true;
    }

    SDL_SetMemoryFunctions(mem_alloc, mem_calloc, mem_realloc, mem_free);

    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        return false;
    }

    s_video_data = mem_alloc(sizeof(video_data_t));
    s_video_data->references = 1;
    s_video_data->windows = g_hash_table_new(window_hash_id, window_id_equal);

    return true;
}

static void video_remove_ref() {
    if (!s_video_data) {
        return;
    }

    s_video_data->references--;

    if (s_video_data->references == 0) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);

        guint size = g_hash_table_size(s_video_data->windows);
        g_assert(size == 0);

        g_hash_table_destroy(s_video_data->windows);
        mem_free(s_video_data);

        s_video_data = NULL;
    }
}

window_t* window_create(const char* title, uint32_t width, uint32_t height) {
    if (!video_add_ref()) {
        return NULL;
    }

    SDL_Window* sdl_window = SDL_CreateWindow(title, (int)width, (int)height, SDL_WINDOW_RESIZABLE);
    if (!sdl_window) {
        return NULL;
    }

    window_t* window = mem_alloc(sizeof(window_t));
    window->window = sdl_window;
    window->close_requested = false;

    size_t id = (size_t)SDL_GetWindowID(sdl_window);
    g_hash_table_insert(s_video_data->windows, (gpointer)id, window);

    return window;
}

void window_destroy(window_t* window) {
    if (!window) {
        return;
    }

    size_t id = (size_t)SDL_GetWindowID(window->window);
    g_hash_table_remove(s_video_data->windows, (gconstpointer)id);

    SDL_DestroyWindow(window->window);
    mem_free(window);

    video_remove_ref();
}

static void window_close_requested(const SDL_WindowEvent* event) {
    size_t id = (size_t)event->windowID;
    window_t* window = (window_t*)g_hash_table_lookup(s_video_data->windows, (gconstpointer)id);

    window->close_requested = true;
}

static void video_sdl_quit() {
    GList* windows = g_hash_table_get_values(s_video_data->windows);

    for (GList* iter = g_list_first(windows); iter; iter = iter->next) {
        window_t* window = (window_t*)iter->data;
        window->close_requested = true;
    }

    g_list_free(windows);
}

void window_poll() {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            window_close_requested(&event.window);
            break;
        case SDL_EVENT_QUIT:
            video_sdl_quit();
            break;
        }
    }
}

bool window_is_close_requested(window_t* window) { return window->close_requested; }
