#include "module.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "yaml_parser.h"
#include "metadata.pb-c.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define FILENAME_INPUT "real_images/buffer_dump.bin"
#define FILENAME_OUTPUT "real_images/output"
#define FILENAME_CONFIG "config.yaml"

// Utility to get file extension (lowercase, no dot)
const char *get_file_extension(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) return "";
    return dot + 1;
}

void save_images(const char *filename_base, const ImageBatch *batch)
{
    uint32_t offset = 0;
    int image_index = 0;

    while (image_index < batch->num_images && offset < batch->batch_size)
    {
        uint32_t meta_size = *((uint32_t *)(batch->data + offset));
        offset += sizeof(uint32_t);

        Metadata *metadata = metadata__unpack(NULL, meta_size, batch->data + offset);
        printf("Image %d: %dx%d, channels=%d, camera=%s\n",
        image_index, metadata->width, metadata->height,
        metadata->channels,
        metadata->camera ? metadata->camera : "NULL");

        offset += meta_size;

        const char *ext = "png"; // default
        if (metadata->camera && strcmp(metadata->camera, "bin") == 0) {
            ext = "bin";
        }

        char filename[128];
        sprintf(filename, "%s%d.%s", filename_base, image_index, ext);

        if (strcmp(ext, "bin") == 0)
        {
            // Save raw as binary file
            FILE *fp = fopen(filename, "wb");
            if (!fp)
            {
                fprintf(stderr, "Error: Unable to open file %s for writing\n", filename);
                metadata__free_unpacked(metadata, NULL);
                break;
            }
            size_t written = fwrite(batch->data + offset, 1, metadata->size, fp);
            fclose(fp);
            if (written != metadata->size)
            {
                fprintf(stderr, "Error: Failed to write raw BIN data to %s\n", filename);
            }
            else
            {
                printf("Raw BIN image saved as %s\n", filename);
            }
        }
        else
        {
            // Save as PNG
            int stride = metadata->width * metadata->channels * sizeof(uint8_t);
            int success = stbi_write_png(filename, metadata->width, metadata->height, metadata->channels, batch->data + offset, stride);
            if (!success)
            {
                fprintf(stderr, "Error writing PNG image to %s\n", filename);
            }
            else
            {
                printf("PNG image saved as %s\n", filename);
            }
        }

        offset += metadata->size;
        metadata__free_unpacked(metadata, NULL);
        image_index++;
    }
}


// Load raw image manually
void load_raw_bin(const char *filename, ImageBatch *batch, int width, int height, int channels, int num_images)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp)
    {
        fprintf(stderr, "[test] Error: Unable to open raw BIN file %s\n", filename);
        exit(EXIT_FAILURE);
    }

    size_t image_size = width * height * channels;
    size_t total_size = image_size * num_images;
    unsigned char *data = (unsigned char *)malloc(total_size);

    if (!data)
    {
        fprintf(stderr, "[test] Error: Unable to allocate memory for raw BIN image\n");
        fclose(fp);
        exit(EXIT_FAILURE);
    }

    size_t read = fread(data, 1, total_size, fp);
    fclose(fp);
    if (read != total_size)
    {
        fprintf(stderr, "[test] Error: Could not read full raw BIN image data from %s\n", filename);
        free(data);
        exit(EXIT_FAILURE);
    }

    Metadata new_meta = METADATA__INIT;
    new_meta.size = image_size;
    new_meta.width = width;
    new_meta.height = height;
    new_meta.channels = channels;
    new_meta.timestamp = 0; // example
    new_meta.bits_pixel = 8;
    new_meta.camera = "bin";

    size_t meta_size = metadata__get_packed_size(&new_meta);
    uint8_t meta_buf[meta_size];
    metadata__pack(&new_meta, meta_buf);

    uint32_t batch_size = (image_size + sizeof(uint32_t) + meta_size) * num_images;
    batch->data = (unsigned char *)malloc(batch_size);
    if (!batch->data)
    {
        fprintf(stderr, "[test] Error: Unable to allocate memory for batch data\n");
        free(data);
        exit(EXIT_FAILURE);
    }

    batch->num_images = num_images;
    batch->batch_size = batch_size;

    int offset = 0;
    for (int i = 0; i < num_images; i++)
    {
        memcpy(batch->data + offset, &meta_size, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        memcpy(batch->data + offset, meta_buf, meta_size);
        offset += meta_size;
        memcpy(batch->data + offset, data + i * image_size, image_size);
        offset += image_size;
    }

    free(data);
}

// Load PNG or other supported formats using stb_image
void load_png(const char *filename, ImageBatch *batch, int num_images)
{
    int image_width, image_height, image_channels;
    unsigned char *image_data = stbi_load(filename, &image_width, &image_height, &image_channels, STBI_rgb_alpha);
    if (!image_data)
    {
        fprintf(stderr, "[test] Error: Unable to load image %s\n", filename);
        exit(EXIT_FAILURE);
    }
    // Note: image_channels here is forced to 4 (STBI_rgb_alpha)
    image_channels = 4;

    batch->num_images = num_images;
    uint32_t image_size = image_height * image_width * image_channels;

    Metadata new_meta = METADATA__INIT;
    new_meta.size = image_size;
    new_meta.width = image_width;
    new_meta.height = image_height;
    new_meta.channels = image_channels;
    new_meta.timestamp = 0; // example time
    new_meta.bits_pixel = 8;
    new_meta.camera = "rgb";

    size_t meta_size = metadata__get_packed_size(&new_meta);
    uint8_t meta_buf[meta_size];
    metadata__pack(&new_meta, meta_buf);

    uint32_t batch_size = (image_size + sizeof(uint32_t) + meta_size) * num_images;
    batch->data = (unsigned char *)malloc(batch_size);

    if (!batch->data)
    {
        fprintf(stderr, "[test] Error: Unable to allocate memory.\n");
        stbi_image_free(image_data);
        exit(EXIT_FAILURE);
    }

    batch->batch_size = batch_size;

    int offset = 0;
    for (uint32_t i = 0; i < num_images; i++)
    {
        memcpy(batch->data + offset, &meta_size, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        memcpy(batch->data + offset, meta_buf, meta_size);
        offset += meta_size;
        memcpy(batch->data + offset, image_data, image_size);
        offset += image_size;
    }

    stbi_image_free(image_data);
}

void load_image(const char *filename, ImageBatch *batch, int num_images)
{
    const char *ext = get_file_extension(filename);
    if (strcmp(ext, "bin") == 0)
    {
        // Specify your raw image properties (width, height, channels) here for .bin files too
        int width = 640;    
        int height = 480;   
        int channels = 1;   // probably 1 for raw Bayer
        load_raw_bin(filename, batch, width, height, channels, num_images);
    }
    else
    {
        load_png(filename, batch, num_images);
    }
}


int main(int argc, char *argv[])
{
    int num_images = 1;
    if (argc > 1)
    {
        num_images = atoi(argv[1]);
    }
    ImageBatch batch;
    load_image(FILENAME_INPUT, &batch, num_images);

    ModuleParameterList module_parameter_list;
    if (parse_module_yaml_file(FILENAME_CONFIG, &module_parameter_list) < 0)
    {
        free(batch.data);
        return -1;
    }

    ImageBatch result = run(&batch, &module_parameter_list, NULL);

    save_images(FILENAME_OUTPUT, &result);

    free(module_parameter_list.parameters);
    free(batch.data);

    if (result.data != NULL && result.data != batch.data)
    {
        free(result.data);
    }
    return 0;
}
