#include "module.h"
#include "util.h"
#include <opencv2/opencv.hpp>
#include <fstream>


/* Define custom error codes */
enum ERROR_CODE {
    MALLOC_ERR = 1,
    PLACEHOLDER = 2,
};

void load_calibration_data(cv::Mat &K, cv::Mat &D){
    //Initialize K as a 3x3 matrix and D as a 1x5 distortion coefficient matrix
    K = cv::Mat::zeros(3, 3, CV_64F);
    D = cv::Mat::zeros(1, 5, CV_64F);

    //Read camera matrix (K) from file
    std::ifstream K_file("camera_matrix.txt"); // ifstream = input file stream from the std = standard library used to read from files
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            K_file >> K.at<double>(i, j); //read a double value from the files and stores it in matrix K at row i and column j
    K_file.close();

    //Read distortion coefficients (D) from file
    std::ifstream D_file("distortion_coeffs.txt");
    for (int i = 0; i < 5; ++i)
        D_file >> D.at<double>(0, i);
    D_file.close();

}

/* START MODULE IMPLEMENTATION */
void module()
{
    cv::Mat K, D;
    load_calibration_data(K, D);
    /* Get number of images in input batch */
    int num_images = get_input_num_images();

    /* Retrieve module parameters by name (defined in config.yaml) */
    /* int param_1 = get_param_bool("param_name_1");
    int param_2 = get_param_int("param_name_2");
    float param_3 = get_param_float("param_name_3");
    char *param_4 = get_param_string("param_name_4"); */

    /* Example code for iterating a pixel value at a time */
    for (int i = 0; i < num_images; ++i)
    {
        Metadata *input_meta = get_metadata(i);
        int height = input_meta->height;
        int width = input_meta->width;
        int channels = input_meta->channels;
        int timestamp = input_meta->timestamp;
        int bits_pixel = input_meta->bits_pixel;
        char *camera = input_meta->camera;

        /* Get custom metadata values */
        // int example_bool = get_custom_metadata_bool(input_meta, "example_bool");
        // int int_example = get_custom_metadata_int(input_meta, "example_int");
        // float example_float = get_custom_metadata_float(input_meta, "example_float");
        // char *example_string = get_custom_metadata_string(input_meta, "example_string");
        
        unsigned char *input_image_data;
        size_t size = get_image_data(i, &input_image_data);

        //Convert raw image data to OpenCV format
        cv::Mat input_image(height, width, (channels==3) ? CV_8UC3 : CV_8UC1, input_image_data);
        cv::Mat undistorted_image;

        //Apply distortion correction
        cv::undistort(input_image, undistorted_image, K, D);

        /* Define temporary output image */
        unsigned char *output_image_data = (unsigned char *)malloc(size);

        /* Check for malloc error */
        if (output_image_data == NULL)
        {
            signal_error_and_exit(MALLOC_ERR);
        }

        memcpy(output_image_data, undistorted_image.data, size);
        
        /* Create image metadata before appending */
        Metadata new_meta = METADATA__INIT;
        new_meta.size = size;
        new_meta.width = width;
        new_meta.height = height;
        new_meta.channels = channels;
        new_meta.timestamp = timestamp;
        new_meta.bits_pixel = bits_pixel;
        new_meta.camera = camera;

        /* Add custom metadata key-value */
        char key[] = "distortion_corrected";
        add_custom_metadata_bool(&new_meta, key, true);
/*        add_custom_metadata_int(&new_meta, "example_int", 20);
        add_custom_metadata_float(&new_meta, "example_float", 20.5);
        add_custom_metadata_string(&new_meta, "example_string", "TEST"); */

        /* Append the image to the result batch */
        append_result_image(output_image_data, size, &new_meta);

        /* Remember to free any allocated memory */
        free(input_image_data);
        free(output_image_data);
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