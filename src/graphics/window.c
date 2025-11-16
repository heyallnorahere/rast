#include "window.h"

#include "core/mem.h"
#include "graphics/image.h"

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>

#include <glib.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <cimgui.h>
#include <cimgui_impl.h>

typedef struct video_data {
    uint32_t references;

    GHashTable* windows;
} video_data_t;

static video_data_t* s_video_data = NULL;

struct window {
    SDL_Window* window;
    bool close_requested;

    image_t* backbuffer;
    SDL_Surface* backbuffer_surface;

    ImGuiContext* imgui;
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
    window->backbuffer = NULL;
    window->backbuffer_surface = NULL;
    window->imgui = NULL;

    size_t id = (size_t)SDL_GetWindowID(sdl_window);
    g_hash_table_insert(s_video_data->windows, (gpointer)id, window);

    return window;
}

void window_destroy(window_t* window) {
    if (!window) {
        return;
    }

    if (window->imgui) {
        igSetCurrentContext(window->imgui);
        ImGui_ImplSDL3_Shutdown();
    }

    size_t id = (size_t)SDL_GetWindowID(window->window);
    g_hash_table_remove(s_video_data->windows, (gconstpointer)id);

    SDL_DestroySurface(window->backbuffer_surface);
    image_free(window->backbuffer);

    SDL_DestroyWindow(window->window);
    mem_free(window);

    video_remove_ref();
}

bool window_init_imgui(window_t* window) {
    if (window->imgui) {
        return false;
    }

    ImGuiContext* context = igGetCurrentContext();
    if (!context) {
        return false;
    }

    if (!ImGui_ImplSDL3_InitForOther(window->window)) {
        return false;
    }

    window->imgui = context;
    return true;
}

bool window_get_framebuffer_size(window_t* window, uint32_t* width, uint32_t* height) {
    SDL_Surface* surface = SDL_GetWindowSurface(window->window);
    if (!surface) {
        return false;
    }

    if (width) {
        *width = (uint32_t)surface->w;
    }

    if (height) {
        *height = (uint32_t)surface->h;
    }

    return true;
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

static void window_process_imgui_events(const SDL_Event* event) {}

void window_poll() {
    GList* windows = g_hash_table_get_values(s_video_data->windows);

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        for (GList* node = windows; node != NULL; node = g_list_next(node)) {
            window_t* window = node->data;
            if (!window->imgui) {
                continue;
            }

            igSetCurrentContext(window->imgui);
            ImGui_ImplSDL3_ProcessEvent(&event);
        }

        switch (event.type) {
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            window_close_requested(&event.window);
            break;
        case SDL_EVENT_QUIT:
            video_sdl_quit();
            break;
        }
    }

    for (GList* node = windows; node != NULL; node = g_list_next(node)) {
        window_t* window = node->data;
        if (!window->imgui) {
            continue;
        }

        igSetCurrentContext(window->imgui);
        ImGui_ImplSDL3_NewFrame();
    }

    g_list_free(windows);
}

bool window_is_close_requested(window_t* window) { return window->close_requested; }

static bool window_is_backbuffer_valid(window_t* window) {
    if (!window->backbuffer_surface) {
        return false;
    }

    uint32_t width, height;
    if (!window_get_framebuffer_size(window, &width, &height)) {
        return false;
    }

    return window->backbuffer_surface->w == width && window->backbuffer_surface->h == height;
}

static bool window_validate_backbuffer(window_t* window) {
    if (window_is_backbuffer_valid(window)) {
        return true;
    }

    uint32_t width, height;
    if (!window_get_framebuffer_size(window, &width, &height)) {
        return false;
    }

    SDL_DestroySurface(window->backbuffer_surface);
    image_free(window->backbuffer);

    window->backbuffer = image_allocate(width, height, IMAGE_FORMAT_COLOR);
    if (window->backbuffer) {
        window->backbuffer_surface =
            SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA8888, window->backbuffer->data,
                                  width * window->backbuffer->pixel_stride);

        return true;
    } else {
        window->backbuffer_surface = NULL;
        return false;
    }
}

image_t* window_get_backbuffer(window_t* window) {
    if (!window_validate_backbuffer(window)) {
        return NULL;
    }

    return window->backbuffer;
}

bool window_swap_buffers(window_t* window) {
    if (!window_is_backbuffer_valid(window)) {
        return true;
    }

    SDL_Surface* window_surface = SDL_GetWindowSurface(window->window);
    if (!window_surface) {
        return false;
    }

    if (!SDL_BlitSurface(window->backbuffer_surface, NULL, window_surface, NULL)) {
        return false;
    }

    if (!SDL_UpdateWindowSurface(window->window)) {
        return false;
    }

    return true;
}
