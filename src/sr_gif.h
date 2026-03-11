#ifndef SR_GIF_H
#define SR_GIF_H

#include <stdint.h>
#include <stdbool.h>

/* Start recording frames at the given resolution.
   Frames are stored as palette-indexed (1 byte/pixel). */
void sr_gif_start_recording(int width, int height);

/* Capture current framebuffer. Call once per captured frame.
   pixels is RGBA8 (uint32_t packed 0xAABBGGRR). */
void sr_gif_capture_frame(const uint32_t *pixels);

/* Is the recorder currently capturing? */
bool sr_gif_is_recording(void);

/* Stop recording and save GIF to disk on a background thread.
   Non-blocking — encoding + file write happens off main thread. */
void sr_gif_stop_and_save(void);

#endif /* SR_GIF_H */
