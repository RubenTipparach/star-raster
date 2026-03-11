#include "sr_texture.h"
#include <stdio.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb/stb_image_write.h"

sr_texture sr_texture_load(const char *path) {
    sr_texture tex = {0};
    int w, h, channels;
    unsigned char *data = stbi_load(path, &w, &h, &channels, 4); /* force RGBA */
    if (!data) {
        fprintf(stderr, "Failed to load texture: %s\n", path);
        return tex;
    }
    tex.width  = w;
    tex.height = h;
    tex.pixels = (uint32_t *)data;
    return tex;
}

void sr_texture_free(sr_texture *tex) {
    if (tex->pixels) {
        stbi_image_free(tex->pixels);
        tex->pixels = NULL;
    }
}
