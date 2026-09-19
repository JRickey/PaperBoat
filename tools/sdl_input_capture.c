#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

typedef struct {
    double at;
    SDL_Scancode scancode;
    bool down;
    bool sent;
} InputEvent;

static InputEvent gEvents[256];
static size_t gEventCount;
static struct timespec gStart;
static bool gInitialized;
static double gLastCapture = -1.0;
static unsigned int gCaptureNumber;

static double elapsed_seconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double) (now.tv_sec - gStart.tv_sec) + (double) (now.tv_nsec - gStart.tv_nsec) / 1000000000.0;
}

static int event_compare(const void* lhs, const void* rhs) {
    const InputEvent* a = lhs;
    const InputEvent* b = rhs;
    return (a->at > b->at) - (a->at < b->at);
}

static void append_press(double at, const char* key_name) {
    if (gEventCount + 2 > sizeof(gEvents) / sizeof(gEvents[0])) {
        fprintf(stderr, "[SDL-DIAG] too many input events\n");
        return;
    }
    SDL_Scancode scancode = SDL_GetScancodeFromName(key_name);
    if (scancode == SDL_SCANCODE_UNKNOWN) {
        fprintf(stderr, "[SDL-DIAG] unknown SDL key name: %s\n", key_name);
        return;
    }
    gEvents[gEventCount++] = (InputEvent) { .at = at, .scancode = scancode, .down = true };
    gEvents[gEventCount++] = (InputEvent) { .at = at + 0.12, .scancode = scancode, .down = false };
}

static void initialize(void) {
    if (gInitialized) {
        return;
    }
    gInitialized = true;
    clock_gettime(CLOCK_MONOTONIC, &gStart);

    const char* script = getenv("PAPERBOAT_INPUT_SCRIPT");
    if (script != NULL && *script != '\0') {
        char* copy = strdup(script);
        char* save = NULL;
        for (char* token = strtok_r(copy, ",", &save); token != NULL; token = strtok_r(NULL, ",", &save)) {
            char* separator = strchr(token, ':');
            if (separator == NULL) {
                fprintf(stderr, "[SDL-DIAG] invalid event: %s\n", token);
                continue;
            }
            *separator = '\0';
            append_press(strtod(token, NULL), separator + 1);
        }
        free(copy);
    }

    const char* repeat = getenv("PAPERBOAT_INPUT_REPEAT");
    if (repeat != NULL && *repeat != '\0') {
        double start = 0.0;
        double interval = 0.0;
        unsigned int count = 0;
        char key_name[64] = { 0 };
        if (sscanf(repeat, "%lf:%lf:%u:%63s", &start, &interval, &count, key_name) == 4) {
            for (unsigned int i = 0; i < count; i++) {
                append_press(start + interval * i, key_name);
            }
        } else {
            fprintf(stderr, "[SDL-DIAG] invalid repeat: %s\n", repeat);
        }
    }
    qsort(gEvents, gEventCount, sizeof(gEvents[0]), event_compare);
    fprintf(stderr, "[SDL-DIAG] loaded %zu key transitions\n", gEventCount);
}

void SDL_PumpEvents(void) {
    static void (*real_pump_events)(void);
    static int (*real_push_event)(SDL_Event*);
    if (real_pump_events == NULL) {
        real_pump_events = dlsym(RTLD_NEXT, "SDL_PumpEvents");
        real_push_event = dlsym(RTLD_NEXT, "SDL_PushEvent");
    }
    real_pump_events();
    initialize();

    const double elapsed = elapsed_seconds();
    for (size_t i = 0; i < gEventCount; i++) {
        InputEvent* input = &gEvents[i];
        if (input->sent || elapsed < input->at) {
            continue;
        }
        SDL_Event event = { 0 };
        event.type = input->down ? SDL_KEYDOWN : SDL_KEYUP;
        event.key.state = input->down ? SDL_PRESSED : SDL_RELEASED;
        event.key.keysym.scancode = input->scancode;
        event.key.keysym.sym = SDL_GetKeyFromScancode(input->scancode);
        real_push_event(&event);
        input->sent = true;
        fprintf(
            stderr, "[SDL-DIAG] t=%.3f %s %s\n", elapsed, input->down ? "down" : "up",
            SDL_GetScancodeName(input->scancode)
        );
    }
}

static void capture_frame(SDL_Window* window, double elapsed) {
    const char* directory = getenv("PAPERBOAT_CAPTURE_DIR");
    if (directory == NULL || *directory == '\0') {
        return;
    }
    const double interval =
        getenv("PAPERBOAT_CAPTURE_INTERVAL") != NULL ? strtod(getenv("PAPERBOAT_CAPTURE_INTERVAL"), NULL) : 1.0;
    const double capture_start =
        getenv("PAPERBOAT_CAPTURE_START") != NULL ? strtod(getenv("PAPERBOAT_CAPTURE_START"), NULL) : 0.0;
    const double capture_end =
        getenv("PAPERBOAT_CAPTURE_END") != NULL ? strtod(getenv("PAPERBOAT_CAPTURE_END"), NULL) : -1.0;
    if (elapsed < capture_start || (capture_end >= 0.0 && elapsed > capture_end)) {
        return;
    }
    if (gLastCapture >= 0.0 && elapsed - gLastCapture < interval) {
        return;
    }

    static void (*real_get_drawable_size)(SDL_Window*, int*, int*);
    if (real_get_drawable_size == NULL) {
        real_get_drawable_size = dlsym(RTLD_NEXT, "SDL_GL_GetDrawableSize");
    }
    int width = 0;
    int height = 0;
    real_get_drawable_size(window, &width, &height);
    if (width <= 0 || height <= 0) {
        return;
    }

    uint8_t* rgba = malloc((size_t) width * height * 4);
    if (rgba == NULL) {
        return;
    }
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);

    char path[1024];
    snprintf(path, sizeof(path), "%s/frame-%05u-t%07.3f.ppm", directory, gCaptureNumber++, elapsed);
    FILE* output = fopen(path, "wb");
    if (output == NULL) {
        fprintf(stderr, "[SDL-DIAG] cannot create %s: %s\n", path, strerror(errno));
        free(rgba);
        return;
    }
    fprintf(output, "P6\n%d %d\n255\n", width, height);
    for (int y = height - 1; y >= 0; y--) {
        const uint8_t* row = rgba + (size_t) y * width * 4;
        for (int x = 0; x < width; x++) {
            fwrite(row + (size_t) x * 4, 1, 3, output);
        }
    }
    fclose(output);
    free(rgba);
    gLastCapture = elapsed;
    fprintf(stderr, "[SDL-DIAG] captured %s\n", path);
}

void SDL_GL_SwapWindow(SDL_Window* window) {
    static void (*real_swap_window)(SDL_Window*);
    if (real_swap_window == NULL) {
        real_swap_window = dlsym(RTLD_NEXT, "SDL_GL_SwapWindow");
    }
    initialize();
    capture_frame(window, elapsed_seconds());
    real_swap_window(window);
}
