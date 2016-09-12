#include <algorithm>
#include <vector>

#include <opencv2/core/core.hpp>
#include "opencv2/opencv.hpp"

#include "gen_border.hpp"

#define IMG_HEIGHT 512
#define IMG_WIDTH 512
#define ROW 10
#define COL 10
#define PURPLE 4

using namespace cv;

/* averages RGB channels for normalization */
void rgbNormalize(Mat *bgr_image) {
    for (int i = 0; i < bgr_image->rows; i++) {
       for (int k = 0; k < bgr_image->cols; k++) {
           float reVal = (float)bgr_image->at<cv::Vec3b>(i,k)[2];
           float blVal = (float)bgr_image->at<cv::Vec3b>(i,k)[1];
           float grVal = (float)bgr_image->at<cv::Vec3b>(i,k)[0];
           float sum = reVal + blVal + grVal;

           bgr_image->at<cv::Vec3b>(i,k)[2] = (int)((reVal/sum)*255.0);
           bgr_image->at<cv::Vec3b>(i,k)[1] = (int)((blVal/sum)*255.0);
           bgr_image->at<cv::Vec3b>(i,k)[0] = (int)((grVal/sum)*255.0);
       }
    }
}

/* borrowed from http://stackoverflow.com/questions/24341114/simple-illumination-correction-in-images-opencv-c */
/* https://en.wikipedia.org/wiki/Adaptive_histogram_equalization#Contrast_Limited_AHE */
/* convert image to lab space and evaluate CLAHE to spread colors over entire color spectrum */
void claheNormalize(Mat *bgr_image) {
    Mat lab_image;
    cvtColor(*bgr_image, lab_image, CV_BGR2Lab);

    /* split original image into 3 color planes */
    std::vector<Mat> lab_planes(3);
    split(lab_image, lab_planes);

    /* create histogram CLAHE to normalize */
    Ptr<CLAHE> clahe = createCLAHE();
    clahe->setClipLimit(4);
    Mat dst;
    clahe->apply(lab_planes[0], dst);

    /* merge planes and convert back to normal bgr */
    dst.copyTo(lab_planes[0]);
    merge(lab_planes, lab_image);

   Mat image_clahe;
   cvtColor(lab_image, *bgr_image, CV_Lab2BGR);
}

/* leverage known color to scale pixels back to original color */
void referenceNormalize(Mat *bgr_image) {
    unsigned int rul = 0, gul = 0, bul = 0;
    unsigned int rur = 0, gur = 0, bur = 0;
    unsigned int rll = 0, gll = 0, bll = 0;
    unsigned int rlr = 0, glr = 0, blr = 0;
    float avg_red = 0.0, avg_blue = 0.0, avg_green = 0.0;
    float num_pixels = ((2.0*(IMG_HEIGHT/ROW))*(2.0*(IMG_WIDTH/COL)));
    float factor_r = 0.0, factor_b = 0.0, factor_g = 0.0;

    /* upper left */
    for (int i = 0; i < (IMG_HEIGHT/ROW)*2; i++) {
       for (int k = 0; k < (IMG_WIDTH/COL)*2; k++) {
           bul += bgr_image->at<cv::Vec3b>(i,k)[0];
           gul += bgr_image->at<cv::Vec3b>(i,k)[1];
           rul += bgr_image->at<cv::Vec3b>(i,k)[2];
       }
    }

    /* upper right */
    for (int i = 0; i < (IMG_HEIGHT/ROW)*2; i++) {
        for (int k = (IMG_WIDTH-(2*(IMG_WIDTH/COL))); k < IMG_WIDTH; k++) {
            bur += bgr_image->at<cv::Vec3b>(i,k)[0];
            gur += bgr_image->at<cv::Vec3b>(i,k)[1];
            rur += bgr_image->at<cv::Vec3b>(i,k)[2];
        }
    }

    /* lower left */
    for (int i = (IMG_HEIGHT-(2*(IMG_HEIGHT/ROW))); i < IMG_HEIGHT; i++) {
        for (int k = 0; k < (IMG_WIDTH/COL)*2; k++) {
            bll += bgr_image->at<cv::Vec3b>(i,k)[0];
            gll += bgr_image->at<cv::Vec3b>(i,k)[1];
            rll += bgr_image->at<cv::Vec3b>(i,k)[2];
        }
    }

    /* lower right */
    for (int i = (IMG_HEIGHT-(2*(IMG_HEIGHT/ROW))); i < IMG_HEIGHT; i++) {
        for (int k = (IMG_WIDTH-(2*(IMG_WIDTH/COL))); k < IMG_WIDTH; k++) {
            blr += bgr_image->at<cv::Vec3b>(i,k)[0];
            glr += bgr_image->at<cv::Vec3b>(i,k)[1];
            rlr += bgr_image->at<cv::Vec3b>(i,k)[2];
        }
    }

    /* average each corner */
    rul /= num_pixels;
    gul /= num_pixels;
    bul /= num_pixels;

    rur /= num_pixels;
    gur /= num_pixels;
    bur /= num_pixels;

    rll /= num_pixels;
    gll /= num_pixels;
    bll /= num_pixels;

    rlr /= num_pixels;
    glr /= num_pixels;
    blr /= num_pixels;

    /* average over four corners */
    avg_red = (rul+rur+rll+rlr)/4.0;
    avg_blue = (bul+bur+bll+blr)/4.0;
    avg_green = (gul+gur+gll+glr)/4.0;

    /* scaling factor to apply to all pizels in image */
    factor_b = (COLOR_TABLE[PURPLE][0])/avg_blue;
    factor_g = (COLOR_TABLE[PURPLE][1])/avg_green;
    factor_r = (COLOR_TABLE[PURPLE][2])/avg_red;

    /* scale each pixel */
    for (int i = 0; i < bgr_image->rows; i++) {
        for (int k = 0; k < bgr_image->cols; k++) {
            bgr_image->at<cv::Vec3b>(k,i)[0] = std::min(255, int(bgr_image->at<cv::Vec3b>(k,i)[0]*factor_b));
            bgr_image->at<cv::Vec3b>(k,i)[1] = std::min(255, int(bgr_image->at<cv::Vec3b>(k,i)[1]*factor_g));
            bgr_image->at<cv::Vec3b>(k,i)[2] = std::min(255, int(bgr_image->at<cv::Vec3b>(k,i)[2]*factor_r));
        }
    }
}

/* based on http://docs.opencv.org/2.4/doc/tutorials/core/basic_linear_transform/basic_linear_transform.html */
/* brighten/darken image using saturation */
void brighten(Mat image, Mat *bgr_image, int beta=-50) {
    double alpha = 1.0;

    /* saturate each pixel to brighten/darken image */
    for(int y = 0; y < image.rows; y++ ){
        for(int x = 0; x < image.cols; x++ ) {
            /* apply to each color channel */
            for(int c = 0; c < 3; c++ ) {
                bgr_image->at<Vec3b>(y,x)[c] =
                saturate_cast<uchar>(alpha*(image.at<Vec3b>(y,x)[c] ) + beta);
            }
       }
    }
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::cout << "invalid args" << std::endl;
    }

    else {
        Mat image;
        Mat bgr_image;

        int idx = 0;

        /* darken the image and then rgb normalize (last iter will brighten) */
        while (idx < 2) {
            image = imread( argv[1]);
            bgr_image = Mat::zeros( image.size(), image.type() );
            if (idx < 1) {
                brighten(image, &bgr_image, 0);
                rgbNormalize(&bgr_image);
            }
            else {
                /* clahe normalize and then final rgb */
                brighten(image, &bgr_image, 0);
                claheNormalize(&bgr_image);
                //rgbNormalize(&bgr_image);
            }
            imwrite(argv[2], bgr_image);
            idx += 1;
       }
       std::cout << "success" << std::endl;
    }
    return 0;
}
