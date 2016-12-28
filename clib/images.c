
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_image.h>

#include "particle.h"
#include "simulation.h"

static uint32_t format = SDL_PIXELFORMAT_RGBA8888;

SDL_Surface *images_simulation_image = NULL;
SDL_Texture *images_simulation_3d_image = NULL;
static int simulation_screen_width = 0;
static int simulation_screen_height = 0;

double renderOffsetX = 0;
double renderOffsetY = 0;

void images_addSimulationImageRenderOffset(double x, double y) {
    renderOffsetX += x;
    renderOffsetY += y;
}

void images_init_simulation_image(int screen_width, int screen_height) {
    if (images_simulation_image) {
        if (screen_width == simulation_screen_width &&
                screen_height == simulation_screen_height) {
            return;
        }
        SDL_FreeSurface(images_simulation_image);
        images_simulation_image = NULL;
    }
    simulation_screen_width = screen_width;
    simulation_screen_height = screen_height;
    images_simulation_image = SDL_CreateRGBSurfaceWithFormat(
        0, screen_width, screen_height, 0, format);
    assert(images_simulation_image->format->BitsPerPixel == 32);
    assert(images_simulation_image->w == screen_width &&
        images_simulation_image->h == screen_height);
    images_simulation_3d_image = SDL_CreateTexture(
        simulation_getRenderer(), format,
        SDL_TEXTUREACCESS_TARGET, screen_width, screen_height); 
}

void images_simulation_3d_clear() {
    assert(images_simulation_3d_image != NULL);
    SDL_SetRenderTarget(simulation_getRenderer(), 
        images_simulation_3d_image);
    SDL_RenderClear(simulation_getRenderer());
}

static SDL_Texture* upload_image = NULL;
void images_simulation_2d_to_3d_upload() {
    assert(images_simulation_3d_image != NULL);
    SDL_Texture *oldTarget = SDL_GetRenderTarget(simulation_getRenderer());
    if (SDL_UpdateTexture(
            images_simulation_3d_image, NULL,
            images_simulation_image->pixels,
            images_simulation_image->pitch) != 0) {
        fprintf(stderr, "SDL error with SDL_UpdateTexture: %s\n",
            SDL_GetError());
        return;
    }

    // Create upload_image and unlock pixel access:
    if (!upload_image) {
        upload_image = SDL_CreateTexture(
            simulation_getRenderer(), format,
            SDL_TEXTUREACCESS_STREAMING, images_simulation_image->w,
            images_simulation_image->h);
    }
    void *pixels;
    int pitch;
    if (SDL_LockTexture(upload_image, NULL, &pixels, &pitch) != 0) {
        fprintf(stderr, "SDL error with SDL_LockTexture: %s\n",
            SDL_GetError());
        return;
    }

    // Copy pixels:
    simulation_lockSurface();
    int _imgh = images_simulation_image->h;
    int _imgw = images_simulation_image->w;
    if (pitch != 0) {
        // Copy line-wise:
        int y = 0;
        while (y < _imgh) {
            int pitch = y * pitch;
            int pix_pos_to = (y * _imgw) * 4 + pitch;
            int pix_pos_from = (y * _imgw) * 4;
            memcpy(pixels + pix_pos_to,
                images_simulation_image->pixels + pix_pos_from, 4 * _imgw);
            y++;
        }
    } else {
        // Copy as a whole:
        memcpy(pixels, images_simulation_image->pixels, _imgw * _imgh * 4);
    }
    SDL_UnlockTexture(upload_image);
    simulation_unlockSurface();

    // Render upload_image to images_simulation_3d_image:
    SDL_SetRenderTarget(simulation_getRenderer(),
        images_simulation_3d_image);
    SDL_Rect dst = {0, 0,
        images_simulation_image->w,
        images_simulation_image->h};
    SDL_RenderCopy(simulation_getRenderer(), upload_image, 
        NULL, &dst);

    // Done! Reset everything:
    SDL_SetRenderTarget(simulation_getRenderer(), oldTarget);
}

/// Download 3d image and replace current 2d contents:
void images_simulation_3d_to_2d_download() {
    assert(images_simulation_3d_image != NULL);
    SDL_Rect dst = {0, 0,
        images_simulation_image->w,
        images_simulation_image->h};
    SDL_Texture *oldTarget = SDL_GetRenderTarget(simulation_getRenderer()); 
    
    SDL_SetRenderTarget(simulation_getRenderer(), NULL);
    SDL_RenderCopy(simulation_getRenderer(), images_simulation_3d_image,
        NULL, &dst);
    SDL_RenderPresent(simulation_getRenderer());

    simulation_lockSurface();
    int result = SDL_RenderReadPixels(simulation_getRenderer(),
        &dst, format,
        images_simulation_image->pixels,
        images_simulation_image->pitch); 
    simulation_unlockSurface();
    if (result != 0) {
        fprintf(stderr, "SDL error in SDL_RenderReadPixels: %s\n",
            SDL_GetError());
        return;
    }
    SDL_RenderPresent(simulation_getRenderer());
    
    SDL_SetRenderTarget(simulation_getRenderer(), oldTarget);
}

/// Download 3d image, and immediately blit it on top of current 2d contents:
static SDL_Surface *_blitOnTopTempImage = NULL;
void images_simulation_3d_to_2d_blit_ontop() {
    return;
    assert(images_simulation_3d_image != NULL);
    if (!_blitOnTopTempImage) {
        _blitOnTopTempImage = SDL_CreateRGBSurfaceWithFormat(
            0, simulation_screen_width,
            simulation_screen_height, 0, format);
    }
    SDL_Rect dst = {0, 0,
        images_simulation_image->w,
        images_simulation_image->h};
    SDL_Texture *oldTarget = SDL_GetRenderTarget(simulation_getRenderer());
    SDL_SetRenderTarget(simulation_getRenderer(), NULL);
    SDL_RenderCopy(simulation_getRenderer(), images_simulation_3d_image,
        NULL, &dst);
    void *data = NULL;
    int result = SDL_RenderReadPixels(simulation_getRenderer(),
        &dst, SDL_PIXELFORMAT_RGBA8888, &data, 0);
    if (result != 0) {
        fprintf(stderr, "SDL error in SDL_RenderReadPixels: %s\n",
            SDL_GetError());
        return;
    }
    assert(images_simulation_image->format->BitsPerPixel == 32);

    SDL_LockSurface(_blitOnTopTempImage);
    assert(_blitOnTopTempImage->pixels != NULL);
    memcpy(_blitOnTopTempImage->pixels, data,
        images_simulation_image->w *
        images_simulation_image->h *
        4);
    SDL_UnlockSurface(_blitOnTopTempImage);
 
    SDL_SetRenderTarget(simulation_getRenderer(), oldTarget);
    SDL_BlitSurface(_blitOnTopTempImage, NULL, images_simulation_image,
        NULL);
}

static SDL_Surface *renderTempSurface = NULL;
void image_applyRenderOffset() {
    if (!renderTempSurface) {
        renderTempSurface = SDL_CreateRGBSurfaceWithFormat(
            0, simulation_screen_width,
            simulation_screen_height, 0, format);
    }

    assert(!simulation_isSurfaceLocked());

    SDL_Rect rect;
    memset(&rect, 0, sizeof(rect));
    rect.x = -((int)(renderOffsetX + 0.5));
    rect.y = -((int)(renderOffsetY + 0.5));

    if (SDL_BlitSurface(images_simulation_image,
            NULL, renderTempSurface, &rect) != 0) {
        fprintf(stderr, "SDL error in SDL_BlitSurface: %s\n",
            SDL_GetError());
        return;
    }
    SDL_FillRect(images_simulation_image, NULL, 0x000000);
    if (SDL_BlitSurface(renderTempSurface,
            NULL, images_simulation_image, NULL) != 0) {
        fprintf(stderr, "SDL error in SDL_BlitSurface: %s\n",
            SDL_GetError());
        return;
    }
}

SDL_Surface *image_load_converted(const char *path, int alpha) {
    SDL_Surface *original = IMG_Load(path);
    if (!original) {
        return NULL;
    }
    SDL_Surface *converted;
    if (alpha) {
        // convert to 32bit argb:
        converted = SDL_ConvertSurfaceFormat(
            original, SDL_PIXELFORMAT_RGBA8888, 0);
        if (!converted) {
            SDL_FreeSurface(original);
            return NULL;
        }
        assert(SDL_ISPIXELFORMAT_ALPHA(converted->format->format));
        assert(converted->format->BitsPerPixel == 32);
    } else {
        // convert to 24bit rgb:
        converted = SDL_ConvertSurfaceFormat(
            original, SDL_PIXELFORMAT_RGB888, 0);
        if (!converted) {
            SDL_FreeSurface(original);
            return NULL;
        }
        assert(!SDL_ISPIXELFORMAT_ALPHA(converted->format->format));

        // Sadly, SDL seems to fail this conversion in current versions.
        // If SDL failed us, correct the epic failure >:-( and do it manually:
        if (converted->format->BitsPerPixel == 32) {
            SDL_FreeSurface(original);
            char *fixed_pixels = malloc(3 * converted->w * converted->h);
            if (!fixed_pixels) {
                SDL_FreeSurface(converted);
                return NULL;
            }
            if (SDL_LockSurface(converted)) {
                free(fixed_pixels);
                SDL_FreeSurface(converted);
                return NULL;
            }
            char *p = (converted->pixels);
            for (int i = 0; i < converted->w * converted->h; i++) {
                int target_index = 3 * i;
                int source_index = 4 * i;
                fixed_pixels[target_index + 0] =
                    p[source_index + 0];
                fixed_pixels[target_index + 1] =
                    p[source_index + 1];
                fixed_pixels[target_index + 2] =
                    p[source_index + 2];
            }
            SDL_UnlockSurface(converted);
            SDL_Surface *converted2 = SDL_CreateRGBSurface(
                0, converted->w, converted->h, 24,
                0xff000000,
                0x00ff0000,
                0x0000ff00,
                0x00000000
            );
            SDL_FreeSurface(converted);
            if (!converted2) {
                free(fixed_pixels);
                return NULL;
            }
            if (SDL_LockSurface(converted2)) {
                SDL_FreeSurface(converted2);
                free(fixed_pixels);
                return NULL;
            }
            memcpy(converted2->pixels, fixed_pixels, 3 * converted2->w *
                converted2->h);
            SDL_UnlockSurface(converted2);
            return converted2;
        }
    }
    SDL_FreeSurface(original);
    return converted;
}
char *image_load_raw(const char *path, int alpha, int *width, int *height) {
    SDL_Surface *srf = image_load_converted(path, alpha);
    if (!srf) {
        return NULL;
    }
    SDL_UnlockSurface(srf);
    char *raw_data = malloc((3 + alpha) * srf->w * srf->h);
    memcpy(raw_data, srf->pixels, (3 + alpha) * srf->w * srf->h);
    SDL_LockSurface(srf);
    *width = srf->w;
    *height = srf->h;
    SDL_FreeSurface(srf);
    return raw_data;
}

SDL_Surface *images_duplicate(SDL_Surface *old) {
    return SDL_ConvertSurface(old, old->format, 0);
}

SDL_Surface *grass;

char *raw_gradient_data = NULL;
int gradient_x = 0;
int gradient_y = 0;
static int images_initialized = 0;
void images_init() {
    if (images_initialized) return;

    IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG);

    // Load gradient image:
    raw_gradient_data = image_load_raw("images/gradient.png", 0, &gradient_x,
        &gradient_y);
    if (!raw_gradient_data) goto images_init_error;

    grass = image_load_converted("images/grass.png", 1);

    // Load particle images:
    if (!particle_loadImage(PARTICLE_GRASS, "images/grass.png")) {
        goto images_init_error;
    }
    if (!particle_loadImage(PARTICLE_CAR, "images/car.png")) {
        goto images_init_error;
    }

    images_initialized = 1;
    return;
images_init_error:
    printf("[images] loading images failed. are you in the correct folder?\n");
    exit(1);
}

