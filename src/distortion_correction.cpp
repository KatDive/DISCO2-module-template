#include "module.h"
#include "util.h"
#include <opencv2/opencv.hpp>
#include <xtensor/xarray.hpp>
#include <xtensor/xio.hpp>
#include <xtensor/xadapt.hpp>
#include <fstream>
#include <sstream>

using namespace std;
using std::ifstream;
using std::vector;
using std::string;

/* Define custom error codes */
enum ERROR_CODE {
    MALLOC_ERR = 1,
    PLACEHOLDER = 2,
};

// Function to read a matrix from a file and return it as an xtensor xarray
xt::xarray<double> read_matrix_from_file(const string& filename, int rows, int cols){
    ifstream file(filename); //Open the file for reading 
    vector<double> data;    //Vector to temporarily store matrix data
    double val;

    //Read value from file and store them in the vector
    while(file >> val){
        data.push_back(val); //Push each value into the vector
    }

    file.close(); // Close the file after reading

    //Convert the flat vector to an xtensor xarray with specified rows and columns
    return xt::adapt(data, {rows, cols});
}

// Function to convert xtensor xarray to OpenCV Mat
cv::Mat xtensor_to_cvmat(const xt::xarray<double>& arr){
    return cv::Mat(arr.shape(0), arr.shape(1), CV_64F, (void*)arr.data()).clone();
}

void load_calibration_data(cv::Mat &K, cv::Mat &D){
    //Initialize K as a 3x3 matrix and D as a 1x5 distortion coefficient matrix
    xt::xarray<double> K_xt = read_matrix_from_file("camera_matrix.txt", 3, 3); //Read 3x3 camera matrix
    xt::xarray<double> D_xt = read_matrix_from_file("distortion_coeffs.txt", 1, 5); //Read 3x3 camera matrix

    //Convert xtensor matrices to OpenCV cv::Mat objects
    K = xtensor_to_cvmat(K_xt); //Camera matrix
    D = xtensor_to_cvmat(D_xt); //Distortion coefficients
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