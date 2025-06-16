#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Load raw binary image
unsigned char *load_raw_bin(const char *filename, int width, int height, int channels)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open raw file %s\n", filename);
        return NULL;
    }
    size_t size = width * height * channels;
    unsigned char *data = malloc(size);
    if (!data) {
        fprintf(stderr, "Error: Cannot allocate memory\n");
        fclose(fp);
        return NULL;
    }
    size_t read = fread(data, 1, size, fp);
    fclose(fp);
    if (read != size) {
        fprintf(stderr, "Error: Unexpected file size for %s\n", filename);
        free(data);
        return NULL;
    }
    return data;
}

// Load PNG image using stb_image
unsigned char *load_png(const char *filename, int *width, int *height, int *channels)
{
    unsigned char *data = stbi_load(filename, width, height, channels, 0);
    if (!data) {
        fprintf(stderr, "Error: Cannot load PNG %s\n", filename);
        return NULL;
    }
    return data;
}

// Compare two images pixel-by-pixel and print summary
void compare_images(const unsigned char *img1, const unsigned char *img2,
                    int width, int height, int channels)
{
    size_t num_pixels = width * height;
    size_t size = num_pixels * channels;

    int differing_channels = 0;
    int max_diff = 0;
    long total_diff = 0;

    FILE *logfile = fopen("pixel_differences.txt", "w");
    if (!logfile) {
        fprintf(stderr, "Error: Cannot open log file for writing.\n");
        return;
    }

    fprintf(logfile, "pixel_index channel old_value new_value difference\n");

    for (size_t i = 0; i < size; i++) {
        int diff = abs(img1[i] - img2[i]);
        if (diff != 0) {
            differing_channels++;
            if (diff > max_diff) max_diff = diff;
            total_diff += diff;

            size_t pixel_idx = i / channels;
            int channel = i % channels;
            fprintf(logfile, "%zu %d %u %u %d\n",
                    pixel_idx, channel, img1[i], img2[i], diff);
        }
    }

    fclose(logfile);

    printf("Image comparison:\n");
    printf("Total pixels: %zu\n", num_pixels);
    printf("Differing pixels: %d\n", differing_channels / channels);
    printf("Max pixel channel difference: %d\n", max_diff);
    if (differing_channels > 0) {
        printf("Average difference per differing pixel channel: %.2f\n", (double)total_diff / differing_channels);
        printf("Detailed differences saved to pixel_differences.txt\n");
    } else {
        printf("Images are identical.\n");
    }
}


int main(int argc, char *argv[])
{
    if (argc < 6) {
        printf("Usage:\n");
        printf("  %s <before_image> <after_image> <width> <height> <channels> [raw|png]\n", argv[0]);
        printf("Example for raw: %s before.bin after.bin 640 480 1 raw\n", argv[0]);
        printf("Example for PNG: %s before.png after.png 640 480 3 png\n", argv[0]);
        return 1;
    }

    const char *before_file = argv[1];
    const char *after_file = argv[2];
    int width = atoi(argv[3]);
    int height = atoi(argv[4]);
    int channels = atoi(argv[5]);
    const char *format = argc > 6 ? argv[6] : "raw";

    unsigned char *before_data = NULL;
    unsigned char *after_data = NULL;

    if (strcmp(format, "raw") == 0) {
        before_data = load_raw_bin(before_file, width, height, channels);
        after_data = load_raw_bin(after_file, width, height, channels);
    } else if (strcmp(format, "png") == 0) {
        before_data = load_png(before_file, &width, &height, &channels);
        after_data = load_png(after_file, &width, &height, &channels);
    } else {
        fprintf(stderr, "Error: Unsupported format %s\n", format);
        return 1;
    }

    if (!before_data || !after_data) {
        free(before_data);
        free(after_data);
        return 1;
    }

    compare_images(before_data, after_data, width, height, channels);

    free(before_data);
    free(after_data);

    return 0;
}
