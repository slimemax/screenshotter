#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/*
 * screenshot_daemon.c
 * -------------------
 * A lightweight X11 screenshot daemon for Linux.
 *
 * Features:
 *   • Captures the root window every N milliseconds (default 1000 ms)
 *   • Creates a directory hierarchy: ~/Screenshots/YYYY/MM/DD/HH/
 *   • Saves each PNG with a random 8‑character hex filename
 *   • Uses libpng for lossless output
 *
 * Compile:
 *   gcc screenshot_daemon.c -o screenshot_daemon -lX11 -lpng
 *
 * Usage:
 *   ./screenshot_daemon            # 1 second interval
 *   ./screenshot_daemon 250        # 250 ms interval
 */

#define DEFAULT_INTERVAL_MS 1000
#define RANDOM_NAME_LEN     8

/* ------------------------------------------------------------ */
/* Utility: create directory path recursively (mkdir -p)        */
static int mkdir_p(const char *path, mode_t mode)
{
    char tmp[512];
    char *p = NULL;
    size_t len;

    /* Copy so we can mutate */
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (len == 0) return -1;
    if (tmp[len - 1] == '/')
        tmp[len - 1] = '\0';

    for (p = tmp + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, mode);
            *p = '/';
        }
    }
    return mkdir(tmp, mode);
}

/* Utility: generate random hexadecimal string */
static void random_hex(char *out, size_t len)
{
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < len; ++i)
        out[i] = hex[rand() % 16];
    out[len] = '\0';
}

/* Save an XImage to PNG file */
static int save_png(XImage *img, const char *filepath)
{
    FILE *fp = fopen(filepath, "wb");
    if (!fp) {
        perror("fopen");
        return -1;
    }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        fclose(fp);
        return -1;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_write_struct(&png_ptr, NULL);
        fclose(fp);
        return -1;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        return -1;
    }

    png_init_io(png_ptr, fp);

    png_set_IHDR(png_ptr, info_ptr, img->width, img->height, 8,
                 PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_ptr, info_ptr);

    png_bytep row = (png_bytep)malloc(3 * img->width);
    if (!row) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        return -1;
    }

    /* Masks depend on visual; assume TrueColor 24‑bit */
    for (int y = 0; y < img->height; ++y) {
        for (int x = 0; x < img->width; ++x) {
            unsigned long pixel = XGetPixel(img, x, y);
            row[3 * x]     = (pixel & img->red_mask)   >> 16;
            row[3 * x + 1] = (pixel & img->green_mask) >> 8;
            row[3 * x + 2] =  pixel & img->blue_mask;
        }
        png_write_row(png_ptr, row);
    }

    free(row);
    png_write_end(png_ptr, NULL);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
    return 0;
}

int main(int argc, char *argv[])
{
    /* Parse interval argument */
    int interval_ms = DEFAULT_INTERVAL_MS;
    if (argc > 1) {
        int v = atoi(argv[1]);
        if (v > 0) interval_ms = v;
    }

    /* Determine base directory (~/Screenshots) */
    const char *home = getenv("HOME");
    if (!home) home = ".";

    /* Open X display */
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "Cannot open X display\n");
        return 1;
    }

    Window root = DefaultRootWindow(dpy);
    XWindowAttributes wa;
    XGetWindowAttributes(dpy, root, &wa);
    int width  = wa.width;
    int height = wa.height;

    srand((unsigned)time(NULL));

    while (1) {
        /* Build directory path by date/time */
        time_t now = time(NULL);
        struct tm tm_now;
        localtime_r(&now, &tm_now);
        char dirpath[512];
        snprintf(dirpath, sizeof(dirpath), "%s/Screenshots/%04d/%02d/%02d/%02d", home,
                 tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday, tm_now.tm_hour);
        mkdir_p(dirpath, 0755);

        /* Random filename */
        char name[RANDOM_NAME_LEN + 1];
        random_hex(name, RANDOM_NAME_LEN);

        char filepath[1024];
        snprintf(filepath, sizeof(filepath), "%s/%s.png", dirpath, name);

        /* Capture screenshot */
        XImage *img = XGetImage(dpy, root, 0, 0, width, height, AllPlanes, ZPixmap);
        if (!img) {
            fprintf(stderr, "Screenshot failed\n");
        } else {
            if (save_png(img, filepath) == 0)
                printf("Saved %s\n", filepath);
            XDestroyImage(img);
        }

        usleep(interval_ms * 1000);
    }

    XCloseDisplay(dpy);
    return 0;
}

