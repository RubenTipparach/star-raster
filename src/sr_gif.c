#include "sr_gif.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
    #include <windows.h>
    #include <process.h>
    #include <direct.h>
#else
    #include <pthread.h>
    #include <sys/stat.h>
    #ifndef __EMSCRIPTEN__
        #include <unistd.h>
    #endif
#endif

/* ── Fixed 6x7x6 = 252 color palette ────────────────────────────── */

#define PAL_R_LEVELS 6
#define PAL_G_LEVELS 7
#define PAL_B_LEVELS 6
#define PAL_SIZE     256  /* GIF requires power-of-2, we use 252 + 4 unused */

static void build_palette(uint8_t palette[PAL_SIZE * 3]) {
    memset(palette, 0, PAL_SIZE * 3);
    for (int r = 0; r < PAL_R_LEVELS; r++) {
        for (int g = 0; g < PAL_G_LEVELS; g++) {
            for (int b = 0; b < PAL_B_LEVELS; b++) {
                int idx = r * (PAL_G_LEVELS * PAL_B_LEVELS) + g * PAL_B_LEVELS + b;
                palette[idx * 3 + 0] = (uint8_t)(r * 255 / (PAL_R_LEVELS - 1));
                palette[idx * 3 + 1] = (uint8_t)(g * 255 / (PAL_G_LEVELS - 1));
                palette[idx * 3 + 2] = (uint8_t)(b * 255 / (PAL_B_LEVELS - 1));
            }
        }
    }
}

static inline uint8_t quantize_pixel(uint32_t rgba) {
    uint8_t r = (rgba >>  0) & 0xFF;
    uint8_t g = (rgba >>  8) & 0xFF;
    uint8_t b = (rgba >> 16) & 0xFF;
    int ri = (r * (PAL_R_LEVELS - 1) + 127) / 255;
    int gi = (g * (PAL_G_LEVELS - 1) + 127) / 255;
    int bi = (b * (PAL_B_LEVELS - 1) + 127) / 255;
    return (uint8_t)(ri * (PAL_G_LEVELS * PAL_B_LEVELS) + gi * PAL_B_LEVELS + bi);
}

/* ── LZW encoder for GIF ────────────────────────────────────────── */

#define LZW_MIN_CODE_SIZE 8
#define LZW_CLEAR_CODE    256
#define LZW_EOI_CODE      257
#define LZW_MAX_CODES     4096

/* Hash table for LZW dictionary: maps (prefix, byte) → code */
#define LZW_HASH_SIZE 8192
#define LZW_HASH_EMPTY 0xFFFF

typedef struct {
    uint16_t prefix[LZW_HASH_SIZE];
    uint8_t  suffix[LZW_HASH_SIZE];
    uint16_t code[LZW_HASH_SIZE];
} lzw_dict;

typedef struct {
    FILE     *fp;
    uint32_t  bit_accum;
    int       bit_count;
    uint8_t   sub_block[256];
    int       sub_block_len;
    lzw_dict  dict;
    int       next_code;
    int       code_size;
} lzw_state;

static void lzw_dict_clear(lzw_dict *d) {
    memset(d->prefix, 0xFF, sizeof(d->prefix));
}

static uint16_t lzw_hash(uint16_t prefix, uint8_t byte) {
    uint32_t h = ((uint32_t)prefix << 8) ^ byte;
    h = (h * 2654435761u) >> 19; /* Knuth multiplicative hash → 13 bits */
    return (uint16_t)(h & (LZW_HASH_SIZE - 1));
}

static int lzw_dict_lookup(lzw_dict *d, uint16_t prefix, uint8_t byte) {
    uint16_t h = lzw_hash(prefix, byte);
    while (d->prefix[h] != LZW_HASH_EMPTY) {
        if (d->prefix[h] == prefix && d->suffix[h] == byte)
            return d->code[h];
        h = (h + 1) & (LZW_HASH_SIZE - 1);
    }
    return -1;
}

static void lzw_dict_insert(lzw_dict *d, uint16_t prefix, uint8_t byte, uint16_t code) {
    uint16_t h = lzw_hash(prefix, byte);
    while (d->prefix[h] != LZW_HASH_EMPTY)
        h = (h + 1) & (LZW_HASH_SIZE - 1);
    d->prefix[h] = prefix;
    d->suffix[h] = byte;
    d->code[h] = code;
}

static void lzw_flush_sub_block(lzw_state *s) {
    if (s->sub_block_len > 0) {
        uint8_t len = (uint8_t)s->sub_block_len;
        fwrite(&len, 1, 1, s->fp);
        fwrite(s->sub_block, 1, s->sub_block_len, s->fp);
        s->sub_block_len = 0;
    }
}

static void lzw_emit_code(lzw_state *s, uint16_t code) {
    s->bit_accum |= (uint32_t)code << s->bit_count;
    s->bit_count += s->code_size;
    while (s->bit_count >= 8) {
        s->sub_block[s->sub_block_len++] = (uint8_t)(s->bit_accum & 0xFF);
        s->bit_accum >>= 8;
        s->bit_count -= 8;
        if (s->sub_block_len == 255)
            lzw_flush_sub_block(s);
    }
}

static void lzw_reset(lzw_state *s) {
    lzw_dict_clear(&s->dict);
    s->next_code = LZW_EOI_CODE + 1;
    s->code_size = LZW_MIN_CODE_SIZE + 1;
}

static void lzw_encode_frame(FILE *fp, const uint8_t *pixels, int count) {
    lzw_state s;
    memset(&s, 0, sizeof(s));
    s.fp = fp;

    /* Write min code size */
    uint8_t min_code_size = LZW_MIN_CODE_SIZE;
    fwrite(&min_code_size, 1, 1, fp);

    lzw_reset(&s);
    lzw_emit_code(&s, LZW_CLEAR_CODE);

    uint16_t current = pixels[0];
    for (int i = 1; i < count; i++) {
        uint8_t byte = pixels[i];
        int found = lzw_dict_lookup(&s.dict, current, byte);
        if (found >= 0) {
            current = (uint16_t)found;
        } else {
            lzw_emit_code(&s, current);
            if (s.next_code < LZW_MAX_CODES) {
                lzw_dict_insert(&s.dict, current, byte, (uint16_t)s.next_code);
                s.next_code++;
                if (s.next_code > (1 << s.code_size) && s.code_size < 12)
                    s.code_size++;
            } else {
                lzw_emit_code(&s, LZW_CLEAR_CODE);
                lzw_reset(&s);
            }
            current = byte;
        }
    }
    lzw_emit_code(&s, current);
    lzw_emit_code(&s, LZW_EOI_CODE);

    /* Flush remaining bits */
    if (s.bit_count > 0) {
        s.sub_block[s.sub_block_len++] = (uint8_t)(s.bit_accum & 0xFF);
        if (s.sub_block_len == 255)
            lzw_flush_sub_block(&s);
    }
    lzw_flush_sub_block(&s);

    /* Block terminator */
    uint8_t zero = 0;
    fwrite(&zero, 1, 1, fp);
}

/* ── Recorder state ──────────────────────────────────────────────── */

#define MAX_GIF_FRAMES 600  /* ~25 sec at 24fps */

static struct {
    uint8_t *frames;       /* palette-indexed pixel data, packed sequentially */
    int      frame_count;
    int      width, height;
    bool     recording;
} recorder;

/* ── Capture thread ──────────────────────────────────────────────── */
/*
 * State machine for lock-free producer/consumer:
 *   0 = idle       (main thread may write staging)
 *   1 = frame_ready (capture thread may read staging)
 *   2 = processing  (capture thread is quantizing)
 *
 * Main thread only transitions: 0 → 1
 * Capture thread only transitions: 1 → 2, 2 → 0
 * No concurrent access to staging buffer.
 */

static struct {
    uint32_t     *staging;      /* raw RGBA staging buffer (one frame) */
    volatile int  state;        /* 0=idle, 1=ready, 2=processing */
    volatile bool running;      /* capture thread alive */
#ifdef _WIN32
    HANDLE thread;
    HANDLE event;
#elif !defined(__EMSCRIPTEN__)
    pthread_t thread;
#endif
} capture;

#ifdef _WIN32
static unsigned __stdcall capture_thread_func(void *arg) {
    (void)arg;
    while (capture.running) {
        WaitForSingleObject(capture.event, 100);
        if (!capture.running) break;
        if (capture.state == 1) {
            capture.state = 2;
            int n = recorder.width * recorder.height;
            if (recorder.frame_count < MAX_GIF_FRAMES) {
                uint8_t *dst = recorder.frames + recorder.frame_count * n;
                for (int i = 0; i < n; i++)
                    dst[i] = quantize_pixel(capture.staging[i]);
                recorder.frame_count++;
            }
            capture.state = 0;
        }
    }
    _endthreadex(0);
    return 0;
}
#elif !defined(__EMSCRIPTEN__)
static void *capture_thread_func(void *arg) {
    (void)arg;
    while (capture.running) {
        if (capture.state == 1) {
            capture.state = 2;
            int n = recorder.width * recorder.height;
            if (recorder.frame_count < MAX_GIF_FRAMES) {
                uint8_t *dst = recorder.frames + recorder.frame_count * n;
                for (int i = 0; i < n; i++)
                    dst[i] = quantize_pixel(capture.staging[i]);
                recorder.frame_count++;
            }
            capture.state = 0;
        } else {
            usleep(500);  /* 0.5ms poll */
        }
    }
    return NULL;
}
#endif

static void capture_thread_start(void) {
    capture.staging = (uint32_t *)malloc(recorder.width * recorder.height * sizeof(uint32_t));
    capture.state = 0;
    capture.running = true;
#ifdef _WIN32
    capture.event = CreateEvent(NULL, FALSE, FALSE, NULL);
    capture.thread = (HANDLE)_beginthreadex(NULL, 0, capture_thread_func, NULL, 0, NULL);
#elif !defined(__EMSCRIPTEN__)
    pthread_create(&capture.thread, NULL, capture_thread_func, NULL);
#endif
}

static void capture_thread_stop(void) {
    capture.running = false;
#ifdef _WIN32
    SetEvent(capture.event);  /* wake thread so it exits */
    WaitForSingleObject(capture.thread, 5000);
    CloseHandle(capture.thread);
    CloseHandle(capture.event);
#elif !defined(__EMSCRIPTEN__)
    pthread_join(capture.thread, NULL);
#endif
    free(capture.staging);
    capture.staging = NULL;
    capture.state = 0;
}

/* ── Public API ──────────────────────────────────────────────────── */

void sr_gif_start_recording(int width, int height) {
    if (recorder.recording) return;
    int pixels_per_frame = width * height;
    recorder.frames = (uint8_t *)malloc(pixels_per_frame * MAX_GIF_FRAMES);
    if (!recorder.frames) {
        fprintf(stderr, "[GIF] Failed to allocate recording buffer\n");
        return;
    }
    recorder.width = width;
    recorder.height = height;
    recorder.frame_count = 0;
    recorder.recording = true;

    capture_thread_start();

    printf("[GIF] Recording started (%dx%d, max %d frames)\n", width, height, MAX_GIF_FRAMES);
}

void sr_gif_capture_frame(const uint32_t *pixels) {
    if (!recorder.recording) return;
    if (recorder.frame_count >= MAX_GIF_FRAMES) {
        printf("[GIF] Max frames reached, auto-stopping\n");
        sr_gif_stop_and_save();
        return;
    }

    /* Only copy if capture thread is idle (state==0).
       If busy, skip this frame — keeps main thread non-blocking. */
    if (capture.state != 0) return;

    int n = recorder.width * recorder.height;
#if defined(__EMSCRIPTEN__)
    /* WASM fallback: quantize synchronously (no capture thread) */
    uint8_t *dst = recorder.frames + recorder.frame_count * n;
    for (int i = 0; i < n; i++)
        dst[i] = quantize_pixel(pixels[i]);
    recorder.frame_count++;
#else
    memcpy(capture.staging, pixels, n * sizeof(uint32_t));
    capture.state = 1;
    #ifdef _WIN32
    SetEvent(capture.event);
    #endif
#endif
}

bool sr_gif_is_recording(void) {
    return recorder.recording;
}

/* ── Background GIF encode + save ────────────────────────────────── */

typedef struct {
    uint8_t *frames;
    int      frame_count;
    int      width, height;
    char     filename[256];
} gif_save_job;

static void gif_save_work(gif_save_job *job) {
    FILE *fp = fopen(job->filename, "wb");
    if (!fp) {
        fprintf(stderr, "[GIF] Failed to open %s for writing\n", job->filename);
        free(job->frames);
        free(job);
        return;
    }

    int w = job->width, h = job->height;
    int pixels_per_frame = w * h;

    /* Build palette */
    uint8_t palette[PAL_SIZE * 3];
    build_palette(palette);

    /* ── GIF89a header ───────────────────────────────────────── */
    fwrite("GIF89a", 1, 6, fp);

    /* Logical Screen Descriptor */
    uint16_t width_le  = (uint16_t)w;
    uint16_t height_le = (uint16_t)h;
    fwrite(&width_le, 2, 1, fp);
    fwrite(&height_le, 2, 1, fp);
    uint8_t packed = 0x87;  /* GCT flag=1, color resolution=7 (8 bits), sort=0, GCT size=7 (256 colors) */
    fwrite(&packed, 1, 1, fp);
    uint8_t bg_color = 0;
    fwrite(&bg_color, 1, 1, fp);
    uint8_t aspect = 0;
    fwrite(&aspect, 1, 1, fp);

    /* Global Color Table (256 entries) */
    fwrite(palette, 1, PAL_SIZE * 3, fp);

    /* NETSCAPE2.0 Application Extension (loop forever) */
    {
        uint8_t ext[] = {
            0x21, 0xFF, 0x0B,
            'N','E','T','S','C','A','P','E','2','.','0',
            0x03, 0x01,
            0x00, 0x00,  /* loop count = 0 (infinite) */
            0x00         /* block terminator */
        };
        fwrite(ext, 1, sizeof(ext), fp);
    }

    /* ── Frames ──────────────────────────────────────────────── */
    for (int f = 0; f < job->frame_count; f++) {
        /* Graphic Control Extension */
        uint8_t gce[] = {
            0x21, 0xF9, 0x04,
            0x00,              /* packed: disposal=0, no user input, no transparency */
            0x04, 0x00,        /* delay time = 4 centiseconds (40ms ≈ 25fps, closest to 24) */
            0x00,              /* transparent color index (unused) */
            0x00               /* block terminator */
        };
        fwrite(gce, 1, sizeof(gce), fp);

        /* Image Descriptor */
        uint8_t img_desc = 0x2C;
        fwrite(&img_desc, 1, 1, fp);
        uint16_t left = 0, top = 0;
        fwrite(&left, 2, 1, fp);
        fwrite(&top, 2, 1, fp);
        fwrite(&width_le, 2, 1, fp);
        fwrite(&height_le, 2, 1, fp);
        uint8_t img_packed = 0x00;  /* no LCT, not interlaced */
        fwrite(&img_packed, 1, 1, fp);

        /* LZW Image Data */
        const uint8_t *frame_data = job->frames + f * pixels_per_frame;
        lzw_encode_frame(fp, frame_data, pixels_per_frame);
    }

    /* GIF Trailer */
    uint8_t trailer = 0x3B;
    fwrite(&trailer, 1, 1, fp);

    fclose(fp);
    printf("[GIF] Saved %s (%d frames)\n", job->filename, job->frame_count);

    free(job->frames);
    free(job);
}

#ifdef _WIN32
static unsigned __stdcall gif_save_thread(void *arg) {
    gif_save_work((gif_save_job *)arg);
    _endthreadex(0);
    return 0;
}
#elif !defined(__EMSCRIPTEN__)
static void *gif_save_thread(void *arg) {
    gif_save_work((gif_save_job *)arg);
    return NULL;
}
#endif

void sr_gif_stop_and_save(void) {
    if (!recorder.recording) return;
    recorder.recording = false;

    /* Wait for capture thread to finish any in-flight frame, then stop it */
#if !defined(__EMSCRIPTEN__)
    capture_thread_stop();
#endif

    if (recorder.frame_count == 0) {
        printf("[GIF] No frames recorded, nothing to save\n");
        free(recorder.frames);
        recorder.frames = NULL;
        return;
    }

    /* Prepare save job */
    gif_save_job *job = (gif_save_job *)malloc(sizeof(gif_save_job));
    job->frames = recorder.frames;
    job->frame_count = recorder.frame_count;
    job->width = recorder.width;
    job->height = recorder.height;

    /* Generate filename with timestamp, save to screenshots/ folder */
#ifdef _WIN32
    _mkdir("screenshots");
#else
    mkdir("screenshots", 0755);
#endif
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    snprintf(job->filename, sizeof(job->filename),
             "screenshots/recording_%04d%02d%02d_%02d%02d%02d.gif",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);

    recorder.frames = NULL;
    recorder.frame_count = 0;

    printf("[GIF] Saving %d frames to %s (background)...\n", job->frame_count, job->filename);

    /* Spawn worker thread for GIF encoding */
#ifdef _WIN32
    HANDLE h = (HANDLE)_beginthreadex(NULL, 0, gif_save_thread, job, 0, NULL);
    if (h) CloseHandle(h);
    else gif_save_work(job);  /* fallback: synchronous */
#elif !defined(__EMSCRIPTEN__)
    pthread_t tid;
    if (pthread_create(&tid, NULL, gif_save_thread, job) == 0)
        pthread_detach(tid);
    else
        gif_save_work(job);  /* fallback: synchronous */
#else
    gif_save_work(job);  /* WASM: synchronous */
#endif
}
