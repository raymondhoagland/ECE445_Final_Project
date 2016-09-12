#include <iostream>
#include <math.h>

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>

#include <openssl/sha.h>

#include "gen_border.hpp"

/* defined for image handling */
#define IMG_HEIGHT 512
#define IMG_WIDTH 512

using namespace cv;

int outlineBorder(string input_file) {
    Mat img;
    try {
        img = imread(input_file, CV_LOAD_IMAGE_COLOR);
        resize(img, img, Size(IMG_WIDTH, IMG_HEIGHT));
    }
    catch (cv::Exception) {
        std::cout << "failed" << std::endl;
        return -1;
    }

    Mat hsv_color;
    hsv_color.create(1,1,CV_8UC(3));
    Mat hsv_img(IMG_HEIGHT, IMG_WIDTH, CV_LOAD_IMAGE_COLOR, Scalar(0,0,0));
    Mat mask(IMG_HEIGHT, IMG_WIDTH, CV_8UC3, Scalar(0,0,0));
    Mat or_res(IMG_HEIGHT, IMG_WIDTH, CV_8UC3, Scalar(0,0,0));

    unsigned long long num_pixels = 0;
    unsigned long long x_val = 0;
    unsigned long long y_val = 0;

    Point avg_ll = Point(-1, -1), avg_lr = Point(-1, -1), avg_ul = Point(-1, -1), avg_ur = Point(-1, -1);
    for (int bgr_idx = 4; bgr_idx < COLOR_TABLE_LEN; bgr_idx++) {
        /* convert img and color to hsv, get inRange and bitwise AND with mask*/
        cvtColor(img, hsv_img, CV_BGR2HSV);
        Scalar bgr = COLOR_TABLE[bgr_idx];
        Mat color(1, 1, CV_8UC(3), bgr);
        cvtColor(color, hsv_color, CV_BGR2HSV);
        inRange(hsv_img, Scalar(hsv_color.at<Vec3b>(0,0)[0]-10,90,90), Scalar(hsv_color.at<Vec3b>(0,0)[0]+10,255,255), mask);
        Mat res;
        bitwise_and(img,img,res,mask=mask);
        bitwise_or(res, or_res, or_res);

        for (int i = 0; i < res.rows; i++) {
            for (int k = 0; k < res.cols; k++) {
                int b = (int)res.at<Vec3b>(i, k)[0];
                int g = (int)res.at<Vec3b>(i, k)[1];
                int r = (int)res.at<Vec3b>(i, k)[2];
                if ((b > 0) || (g > 0) || (r > 0)) {
                    x_val += k;
                    y_val += i;
                    num_pixels += 1;
                }
            }
        }
    }
    Point center = Point(-1, -1);
    center.x = x_val/((float)num_pixels);
    center.y = y_val/((float)num_pixels);
    std::cout << center.x << " " << center.y << std::endl;

    if ((center.x >= 0) && (center.x <=156)) {
        std::cout << "LEFT" << std::endl;
    }
    else if ((center.x >= 356) && (center.x <= 512)){
        std::cout << "RIGHT" << std::endl;
    }
    else if ((center.y >= 0) && (center.y <= 156)) {
        std::cout << "UP" << std::endl;
    }
    else if ((center.y >= 356) && (center.y <= 512)) {
        std::cout << "DOWN" << std::endl;
    }
    else {
        std::cout << "CENTER" << std::endl;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cout << "invalid args." << std::endl;
    }
    else {
        string input_file = argv[1];
        outlineBorder(input_file);
    }
    return 0;
}
