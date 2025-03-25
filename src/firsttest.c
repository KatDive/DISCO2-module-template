#include "module.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "yaml_parser.h"
#include "metadata.pb-c.h"
#include <sys/msg.h>
#include <sys/shm.h>

#define FILENAME_INPUT "input.png"
#define FILENAME_OUTPUT "output"
#define FILENAME_CONFIG "config.yaml"

void save_images(const char *filename_base, const ImageBatch *batch)
{
    uint32_t offset = 0;
    int image_index = 0;

    while (image_index < batch->num_images && offset < batch->batch_size)
    {
        uint32_t meta_size = *((uint32_t *)(batch->data + offset));
        offset += sizeof(uint32_t); // Move the offset to the start of metadata

        printf("Reading metdata: size=%u\n", meta_size);

        Metadata *metadata = metadata__unpack(NULL, meta_size, batch->data + offset);
        if (!metadata)
        {
            fprintf(stderr, "Metadata unpacking failed\n");
            return;
        }
        offset += meta_size; // Move offset to start of image

        printf("Metadata: width=%d, height=%d, channels=%d\n",
            metadata->width, metadata->height, metadata->channels);

        char filename[20];
        sprintf(filename, "%s%d.png", filename_base, image_index);

        int stride = metadata->width * metadata->channels * sizeof(uint8_t);
        int success = stbi_write_png(filename, metadata->width, metadata->height, metadata->channels, batch->data + offset, stride);
        if (!success)
        {
            fprintf(stderr, "Error writing image to %s\n", filename);
        }
        else
        {
            printf("Image saved as %s\n", filename);
        }

        offset += metadata->size; // Move the offset to the start of the next image block
        

        image_index++;
    }
}

#include <sys/msg.h>

void load_image(const char *filename, ImageBatch *batch, int num_images)
{
    // Retrieve shared memory ID from message queue
    int msg_queue_id = msgget(71, 0666);
    if (msg_queue_id == -1)
    {x
        perror("msgget error");
        exit(EXIT_FAILURE);
    }

    if (msgrcv(msg_queue_id, batch, sizeof(ImageBatch), 0, 0) == -1)
    {
        perror("msgrcv error");
        exit(EXIT_FAILURE);
    }

    // Now batch->shmid is set correctly
    int shmid = batch->shmid;
    void *shmaddr = shmat(shmid, NULL, 0);
    if (shmaddr == (void *)-1)
    {
        perror("shmat error");
        exit(EXIT_FAILURE);
    }

    //uint32_t image_size = 2056 * 2464 * 1; // Image height * width * channels
    batch->num_images = num_images;

    uint32_t batch_size = batch->batch_size;
    batch->data = (unsigned char *)malloc(batch_size);

    memcpy(batch->data, shmaddr, batch_size);

    // Detach from shared memory
    shmdt(shmaddr);
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

    for (int i = 0; i < 100; i++) // Print first 100 bytes
    {
        printf("%02X ", batch.data[i]);
    }
    printf("\n");

	ModuleParameterList module_parameter_list;
	if (parse_module_yaml_file(FILENAME_CONFIG, &module_parameter_list) < 0)
		return -1;

    ImageBatch result = run(&batch, &module_parameter_list, NULL);

    save_images(FILENAME_OUTPUT, &result);
    free(module_parameter_list.parameters);
    free(result.data);
    return 0;
}
