#include "module.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum ERROR_CODE {
    MALLOC_ERR = 1,
    PLACEHOLDER = 2,
};

#define MAX_WIDTH 2464
#define MAX_HEIGHT 2056

/* Detect defective pixels by thresholding difference from local mean */
/* Outputs defect_mask: 1 = defective, 0 = good */
static void detect_defects(const unsigned char *image, int width, int height, unsigned char *defect_mask, int threshold)
{
    printf("[DEBUG] Starting defect detection with threshold %d\n", threshold);

    // Clear mask
    memset(defect_mask, 0, width * height);

    for (int y = 1; y < height - 1; y++)
    {
        for (int x = 1; x < width - 1; x++)
        {
            int sum = 0;
            int count = 0;
            // Compute 3x3 neighborhood mean excluding center pixel
            for (int ny = y - 1; ny <= y + 1; ny++)
            {
                for (int nx = x - 1; nx <= x + 1; nx++)
                {
                    if (nx == x && ny == y)
                        continue; // skip center pixel

                    sum += image[ny * width + nx];
                    count++;
                }
            }
            int mean = sum / count;
            int pixel_val = image[y * width + x];
            int diff = pixel_val - mean;
            if (diff < 0) diff = -diff;

            // Mark pixel defective if difference above threshold OR stuck at 0 or 255
            if (pixel_val == 0 || pixel_val == 255)
            {
                defect_mask[y * width + x] = 1;
                // Uncomment for per-pixel debug:
                // printf("[DEBUG] Defective pixel detected at (%d,%d): val=%d, mean=%d, diff=%d\n", x, y, pixel_val, mean, diff);
            }
        }
    }

    printf("[DEBUG] Defect detection complete.\n");
}

/* Interpolate defective pixels by replacing them with the mean of valid neighbors */
static void interpolate_defects(unsigned char *image, int width, int height, const unsigned char *defect_mask)
{
    printf("[DEBUG] Starting defect pixel interpolation\n");

    for (int y = 1; y < height - 1; y++)
    {
        for (int x = 1; x < width - 1; x++)
        {
            if (defect_mask[y * width + x] == 1)
            {
                int sum = 0;
                int count = 0;
                // average neighbors that are NOT defective
                for (int ny = y - 1; ny <= y + 1; ny++)
                {
                    for (int nx = x - 1; nx <= x + 1; nx++)
                    {
                        if (nx == x && ny == y)
                            continue; // skip center pixel
                        if (defect_mask[ny * width + nx] == 0)
                        {
                            sum += image[ny * width + nx];
                            count++;
                        }
                    }
                }
                if (count > 0)
                {
                    image[y * width + x] = (unsigned char)(sum / count);
                    // printf("[DEBUG] Pixel (%d,%d) interpolated with value %d\n", x, y, image[y * width + x]);
                }
                else
                {
                    // No valid neighbors found (rare), leave pixel as is
                    // printf("[DEBUG] Pixel (%d,%d) no valid neighbors for interpolation\n", x, y);
                }
            }
        }
    }

    printf("[DEBUG] Interpolation complete.\n");
}

/* START MODULE IMPLEMENTATION */
void module()
{
    int num_images = get_input_num_images();

    /* Parameters */
    int threshold = 50;  // Default threshold, could be made configurable

    for (int i = 0; i < num_images; ++i)
    {
        Metadata *input_meta = get_metadata(i);

        unsigned char *image_data;
        size_t size = get_image_data(i, &image_data);

        int width = input_meta->width;
        int height = input_meta->height;
        int channels = input_meta->channels;
        int bits_pixel = input_meta->bits_pixel;

        printf("[DEBUG] Processing image %d: %dx%d, channels=%d, bits_pixel=%d\n", i, width, height, channels, bits_pixel);

        // Only support single-channel 8-bit images for this module
        if (channels != 1 || bits_pixel != 8)
        {
            printf("[ERROR] Unsupported image format: channels=%d, bits_pixel=%d\n", channels, bits_pixel);
            append_result_image(image_data, size, input_meta); // pass unchanged
            free(image_data);
            continue;
        }

        // Allocate defect mask
        unsigned char *defect_mask = malloc(width * height);
        if (!defect_mask)
        {
            printf("[ERROR] Failed to allocate defect mask\n");
            append_result_image(image_data, size, input_meta);
            free(image_data);
            continue;
        }

        detect_defects(image_data, width, height, defect_mask, threshold);
        interpolate_defects(image_data, width, height, defect_mask);

        free(defect_mask);

        Metadata new_meta = METADATA__INIT;
        new_meta.size = size;
        new_meta.width = width;
        new_meta.height = height;
        new_meta.channels = channels;
        new_meta.timestamp = input_meta->timestamp;
        new_meta.bits_pixel = bits_pixel;
        new_meta.camera = input_meta->camera;

        append_result_image(image_data, size, &new_meta);

        free(image_data);
    }
}
/* END MODULE IMPLEMENTATION */

/* Main function of module (NO NEED TO MODIFY) */
ImageBatch run(ImageBatch *input_batch, ModuleParameterList *module_parameter_list, int *ipc_error_pipe)
{
    ImageBatch result_batch;
    result = &result_batch;
    input = input_batch;
    config = module_parameter_list;
    error_pipe = ipc_error_pipe;
    initialize();

    module();

    finalize();

    return result_batch;
}
