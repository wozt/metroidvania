/*
 * aos_lab_frame.c — dump a worker's live framebuffer to PNG.
 *
 * Reads the POSIX shared-memory segment /dev/shm/aoslab_fb_<id>
 * published by aos_lab_worker and writes a PNG (optionally scaled)
 * so the emulated screen can be inspected visually.
 *
 * Usage: aos_lab_frame WORKER_ID OUT.png [SCALE]
 */
#include <fcntl.h>
#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define FB_W 240
#define FB_H 160
#define FB_HDR 32
#define FB_MAGIC 0x414F5346u
#define FB_PIXELS (FB_W * FB_H)

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s WORKER_ID OUT.png [SCALE]\n", argv[0]);
        return 2;
    }
    int wid = atoi(argv[1]);
    const char *out_path = argv[2];
    int scale = argc > 3 ? atoi(argv[3]) : 3;
    if (scale < 1) scale = 1;

    char shm_name[64];
    snprintf(shm_name, sizeof shm_name, "/aoslab_fb_%d", wid);
    int fd = shm_open(shm_name, O_RDONLY, 0600);
    if (fd < 0) {
        fprintf(stderr, "cannot open %s (worker %d running?)\n",
                shm_name, wid);
        return 1;
    }
    size_t sz = FB_HDR + FB_PIXELS * 4;
    uint8_t *map = mmap(NULL, sz, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    if (map == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    uint32_t *hdr = (uint32_t *)map;
    if (hdr[0] != FB_MAGIC) {
        fprintf(stderr, "bad shm magic in %s\n", shm_name);
        munmap(map, sz);
        return 1;
    }
    const uint32_t *px = (const uint32_t *)(map + FB_HDR);

    /* convert 0xRRGGBB -> RGB rows, optionally NEAREST-scaled */
    int ow = FB_W * scale, oh = FB_H * scale;
    uint8_t *rgb = malloc((size_t)ow * oh * 3);
    if (!rgb) { munmap(map, sz); return 1; }
    for (int y = 0; y < oh; ++y) {
        const uint32_t *src = px + (y / scale) * FB_W;
        uint8_t *d = rgb + (size_t)y * ow * 3;
        for (int x = 0; x < ow; ++x) {
            uint32_t c = src[x / scale];
            *d++ = (c >> 16) & 0xFF;
            *d++ = (c >> 8) & 0xFF;
            *d++ = c & 0xFF;
        }
    }
    munmap(map, sz);

    FILE *fp = fopen(out_path, "wb");
    if (!fp) { perror(out_path); free(rgb); return 1; }
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                              NULL, NULL, NULL);
    png_infop info = png_create_info_struct(png);
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        fclose(fp);
        free(rgb);
        return 1;
    }
    png_init_io(png, fp);
    png_set_IHDR(png, info, ow, oh, 8, PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    for (int y = 0; y < oh; ++y)
        png_write_row(png, rgb + (size_t)y * ow * 3);
    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);
    fclose(fp);
    free(rgb);
    printf("%s\n", out_path);
    return 0;
}
