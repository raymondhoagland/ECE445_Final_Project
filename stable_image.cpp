#include <opencv2/highgui/highgui.hpp>
#include "opencv2/opencv.hpp"
#include <unistd.h>

#define DEBUG true

using namespace cv;

/* copy over frame1 to frame2 and re-populate (no longer necessary) */
void capture_img(VideoCapture cam, Mat *frame1, Mat *frame2, int firstFrame) {
    *frame2 = (*frame1).clone();
    cam >> *frame1;
}

int main(int argc, char *argv[]) {
    if ((!DEBUG) && (argc < 2)) {
        std::cout << "invalid args" << std::endl;
    }

    /* ODROID should only have one camera, ignore laptop webcam */
    VideoCapture cam;
    if (DEBUG) {
        cam = VideoCapture(1);
    }
    else {
        cam = VideoCapture(0);
    }

    /* make sure camera correctly opens */
    if(!cam.isOpened()) {
        std::cout << "Unable to open camera." << std::endl;
    }

    /* camera settings  (PNG) (512 x 512 image) (50% exposure) (20% brightness) */
    cam.set(CV_CAP_PROP_FOURCC,CV_FOURCC('M','P','N','G'));
    cam.set(CV_CAP_PROP_FRAME_WIDTH, 512);
    cam.set(CV_CAP_PROP_FRAME_HEIGHT, 512);
    cam.set(CV_CAP_PROP_EXPOSURE, 100);
    cam.set(CV_CAP_PROP_BRIGHTNESS, 0);

    Mat image1;
    Mat image2;
    int firstFrame = 0;
    int frameIdx = 0;

    /* start processing frames */
    while (1) {
        capture_img(cam, &image1, &image2, firstFrame);

        /* ignore first frame (no longer needed) */
        if (firstFrame == 0) {
            firstFrame = 1;
        }
        else {
            Mat blurred;

            /* borrowed from http://stackoverflow.com/questions/4993082/how-to-sharpen-an-image-in-opencv */
            /* apply blurring to image capture to account for small movements */
            double sigma = 5, threshold = 5, amount = 1;
            GaussianBlur(image2, blurred, Size(), sigma, sigma);
            Mat lowContrastMask = abs(image2 - blurred) < threshold;
            Mat sharpened = image2*(1+amount) + blurred*(-amount);
            image2.copyTo(sharpened, lowContrastMask);

            if (argc < 2) {
                imwrite("tpp"+std::to_string(frameIdx)+".png", sharpened);
            }
            else {
                imwrite(argv[1], sharpened);
            }
            frameIdx += 1;
        }
        sleep(2);
        /* small sleep to allow for scene to change */
        //usleep(500);
    }
    return 0;
}
