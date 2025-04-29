#include "module.h"
#include "util.h"
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <string.h>
#include <time.h>
#include "metadata.pb-c.h"
#include "module.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "yaml_parser.h"


#define FILENAME_INPUT "input.png"
#define FILENAME_OUTPUT "output"
#define FILENAME_CONFIG "config.yaml"

extern ImageBatch run(ImageBatch *input_batch, ModuleParameterList *module_parameter_list, int *ipc_error_pipe);

// Imported this funtion from firsttest (also exists in main func in DIPP), currently the function fails do a segmentation error 
// that happens on line 61 due to some pointer behaviour that I havent figured out yet
//I tried to debug the error by printing out different data to check if it fails because some variable is NULL (it is not, so far as I debugged)
//Idk what to do now... 
// vale variable and pointers before it were just created for tests (cross check original code with firsttest file)
void save_images(const char *filename_base, const ImageBatch *batch)
{
    printf("Starting save image\n");
    uint32_t offset = 0;
    int image_index = 0;

    while (image_index < batch->num_images && offset < batch->batch_size)
    {
        
        printf("Entering while loop\n");

        printf("Batch data:=%p\n", (void*)batch->data);
        printf("Batch size:=%u\n", batch->batch_size);
        
        if (batch->batch_size < sizeof(uint32_t)) {
            fprintf(stderr, "batch->batch_size (%u) is too small\n", batch->batch_size);
            return;
        }else{
            printf("condition not matched\n");

        }

        printf("Batch size + offset:=%p\n", batch->data + offset);

        uint8_t *base = batch->data;       // or (uint8_t *)some_void_ptr;
        uint8_t *offset_ptr = base + offset;

        printf("Offset pointer:=%p\n", (void *)offset_ptr);

        if ((uintptr_t)offset_ptr % sizeof(uint32_t) != 0) {
            fprintf(stderr, "Offset pointer is not aligned for uint32_t access\n");
            return;
        }else{
            printf("condition not matched for offset type\n");
        }

        uint32_t value = *((uint32_t *)offset_ptr);

        printf("Value:=%p\n", (void *)value);

        uint32_t meta_size = *((uint32_t *)(batch->data + offset));

        printf("Reading metdata: size=%u\n", meta_size);

        offset += sizeof(uint32_t); // Move the offset to the start of metadata


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


int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        printf("Missing arguments: Expected <num_images> <pipeline_id> <image_name>");
        return -1;
    }

    char * image_name = argv[3];

    printf("image name\r\n");

    // Get timestamp (used for SHM key)
    struct timespec time;
    if (clock_gettime(CLOCK_MONOTONIC, &time) < 0)
    {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }

    printf("Got time\r\n");

    printf("Opening file...");

    FILE *fh = fopen(image_name, "r");

    if (!fh) {
        perror("fopen failed");
        return -1;
    }

    // get size of the file in bytes
    fseek(fh, 0, SEEK_END);
    long fsize = ftell(fh);
    fseek(fh, 0, SEEK_SET);

    printf("Opened file\r\n");

    // Prepare the data
    ImageBatch data;
    data.mtype = 1;
    data.num_images = atoi(argv[1]);
    data.pipeline_id = atoi(argv[2]);

    // Hardcoded bayer image specs
    uint32_t image_height = 2056;
    uint32_t image_width = 2464; 
    uint32_t bits_per_pixel = 12;
    uint32_t image_channels = 1;

    uint32_t image_size = fsize;
    Metadata new_meta = METADATA__INIT;
    new_meta.size = image_size;
    new_meta.width = image_width;
    new_meta.height = image_height;
    new_meta.channels = image_channels;
    new_meta.timestamp = 0; // example time (should be using unix timestamp)
    new_meta.bits_pixel = bits_per_pixel;
    new_meta.camera = "rgb";

    printf("Created meta\r\n");

    size_t meta_size = metadata__get_packed_size(&new_meta);
    uint8_t meta_buf[meta_size];
    metadata__pack(&new_meta, meta_buf);

    printf("Packed meta\r\n");

    uint32_t batch_size = (image_size + sizeof(uint32_t) + meta_size) * data.num_images;

    int shmid = shmget(time.tv_nsec, batch_size, IPC_CREAT | 0666);
    data.shmid = shmid;
    char *shmaddr = shmat(shmid, NULL, 0);
    data.batch_size = batch_size;
    int offset = 0;

    printf("Attached to the shared memory\r\n");

    for (size_t i = 0; i < data.num_images; i++)
    {
        // Insert metadata size before metadata
        memcpy(shmaddr + offset, &meta_size, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        printf("Copied size of meta\r\n");
        memcpy(shmaddr + offset, &meta_buf, meta_size);
        offset += meta_size;
        printf("Copied meta\r\n");
        // insert image
        fseek(fh, 0, SEEK_SET);
        fread(shmaddr + offset, 1, image_size, fh);
        offset += image_size;
        printf("Copied image\r\n");
        
    }

       // create msg queue
       int msg_queue_id;
       if ((msg_queue_id = msgget(71, 0666 | IPC_CREAT)) == -1)
       {
           perror("msgget error");
       }
   
       printf("Got queue\r\n");
   
       // send msg to queue
       if (msgsnd(msg_queue_id, &data, sizeof(data), 0) == -1)
       {
           perror("msgsnd error");
       }
   
       printf("Image sent!\n");
   
        // Create dummy module parameters and error pipe
        ModuleParameterList param_list;
        param_list.n_parameters = 0;
        param_list.parameters = NULL;
    
        int dummy_error_pipe[2] = {-1, -1};
    
        // Create input batch
        ImageBatch input_batch;
        input_batch.shmid = shmid;
        input_batch.batch_size = batch_size;
        input_batch.num_images = data.num_images;
        input_batch.pipeline_id = data.pipeline_id;
        input_batch.mtype = 1;
    
        // Call the distortion module directly
        ImageBatch result = run(&input_batch, &param_list, dummy_error_pipe);
        save_images(FILENAME_OUTPUT, &result);
        // Example output handling (optional)
        printf("Distortion module executed successfully\n");
    
    // detach from the shared memory segment
    shmdt(shmaddr);
}