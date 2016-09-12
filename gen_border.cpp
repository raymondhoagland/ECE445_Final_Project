#include <iostream>
#include <math.h>

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>

#include <openssl/sha.h>

/* defines for image handling */
#define IMG_HEIGHT 512
#define IMG_WIDTH 512
#define OFFSET_X 20
#define OFFSET_Y 20
#define ROW 10
#define COL 10
#define SAVE_TEMPLATE false
#define PROCESS_SHOT_BEST_FIT true

using namespace cv;

/* consts for border generation */
const Scalar COLOR_TABLE[] = {Scalar(0,34,255), Scalar(64,247,2), Scalar(255,30,0), Scalar(255,0,174), Scalar(3, 251, 255)};
const short COLOR_TABLE_LEN = 5;
const short PADDING_BITS[] = {44,45,54,55};
const short PAD_LEN_MID = 4;
const short PAD_LEN_END = 16;

char toHex(int c) {
    if (c < 10) {
        return '0'+c;
    }
    else {
        return 'A'+(c-10);
    }
}

string generateBorder(bool create_img = false) {
    /* quad digits for now*/
    const unsigned char str[] = "hoaglan2@ilkinois.edu";
    unsigned char hash[20];
    char quad_digs[ROW*COL+1];
    int special_idx = 0, idx = 0, hash_idx = 0, write_idx = 0, val;
    SHA1(str, sizeof(str) - 1, hash);
    while (idx < ROW*COL) {
        if (idx >= (ROW*COL-PAD_LEN_END)) {
            quad_digs[idx] = '4';
            idx += 1;
            continue;
        }
        else if (idx == PADDING_BITS[special_idx]) {
            quad_digs[idx] = '4';
            idx += 1;
            special_idx += 1;
            continue;
        }
        else {
            write_idx = 3;
            val = (int)hash[hash_idx];
            while (write_idx >= 0) {
                if (val == 0) {
                    quad_digs[idx+write_idx] = '0';
                }
                else {
                    quad_digs[idx+write_idx] = (val%4)+'0';
                    val/=4;
                }
                write_idx -= 1;
            }
            hash_idx += 1;
            idx += 4;
        }
    }
    quad_digs[100] = '\0';

    /* create the black image with white border */
    if (create_img) {
        Mat img(IMG_HEIGHT+OFFSET_Y*2, IMG_WIDTH+OFFSET_X*2, CV_8UC3, Scalar(0,0,0));
        rectangle(img, Point(0, 0), Point(OFFSET_Y*2+IMG_HEIGHT, OFFSET_X*2+IMG_WIDTH), Scalar(255,255,255), OFFSET_Y);

        /* color in the blocks */
        int spec_idx = 0, hex_idx = 0, block_idx = 0, digit;
        for (int k = 0; k < ROW; k++) {
            for (int i = 0; i < COL; i++) {
                digit = quad_digs[hex_idx]-'0';
                Scalar color = COLOR_TABLE[digit];
                hex_idx += 1;
                block_idx += 1;
                rectangle(img, Point(i*(IMG_HEIGHT/ROW)+OFFSET_Y, k*(IMG_WIDTH/COL)+OFFSET_X), Point((i+1)*(IMG_HEIGHT/ROW)+OFFSET_Y, (k+1)*(IMG_WIDTH/COL)+OFFSET_X), color, -1);
            }
        }

        /* save template */
        imwrite("./bbk.png", img);
    }
    return quad_digs;
}

bool decodeBorder(string toFind) {
    /* load the image */
    Mat img = imread("./testBorder10-fixed.png", CV_LOAD_IMAGE_COLOR);//imread("./testBorder10-fixed.png", CV_LOAD_IMAGE_COLOR);
    resize(img, img, Size(IMG_WIDTH, IMG_HEIGHT));

    /* result strings */
    char default_z[101];
    for (int i = 0; i < ROW*COL; i++) {
        default_z[i] = 'Z';
    }
    default_z[100] = '\0';
    string results[] = {default_z, default_z, default_z, default_z};

    /* threshold the values */
    Mat hsv_color;
    hsv_color.create(1,1,CV_8UC(3));
    Mat hsv_img(IMG_HEIGHT, IMG_WIDTH, CV_LOAD_IMAGE_COLOR, Scalar(0,0,0));
    Mat mask(IMG_HEIGHT, IMG_WIDTH, CV_8UC3, Scalar(0,0,0));
    Mat or_res(IMG_HEIGHT+OFFSET_Y, IMG_WIDTH+OFFSET_X, CV_8UC3, Scalar(0,0,0));
    for (int bgr_idx = 0; bgr_idx < COLOR_TABLE_LEN; bgr_idx++) {
        /* convert img and color to hsv, get inRange and bitwise AND with mask*/
        cvtColor(img, hsv_img, CV_BGR2HSV);
        Scalar bgr = COLOR_TABLE[bgr_idx];
        Mat color(1, 1, CV_8UC(3), bgr);
        cvtColor(color, hsv_color, CV_BGR2HSV);
        inRange(hsv_img, Scalar(hsv_color.at<Vec3b>(0,0)[0]-10,0,0), Scalar(hsv_color.at<Vec3b>(0,0)[0]+10,255,255), mask);
        Mat res;
        bitwise_and(img,img,res, mask=mask);

        /* decode blocks from information */
        vector<int> l_blocks[4];
        for (int i = 0; i < res.rows; i++) {
            for (int k = 0; k < res.cols; k++) {
                /* check if value > 0 */
                int b = (int)res.at<Vec3b>(i, k)[0];
                int g = (int)res.at<Vec3b>(i, k)[1];
                int r = (int)res.at<Vec3b>(i, k)[2];
                if ((b > 0) || (g > 0) || (r > 0)) {
                    int n_row = floor(i*(float)ROW/IMG_HEIGHT);
                    int n_col = floor(k*(float)COL/IMG_WIDTH);
                    int blocks[4] = {int(n_row*ROW+n_col), (ROW*COL-1)-int(n_col*COL+(ROW-1-n_row)), (ROW*COL-1)- int(n_row*ROW+n_col), int(n_col*COL+(ROW-1-n_row))};

                    /* check if block already used for this rotation angle */
                    for (int b_idx = 0; b_idx < 4; b_idx++) {
                        bool flag = false;
                        for (int lb_idx = 0; lb_idx < l_blocks[b_idx].size(); lb_idx++) {
                            if (l_blocks[b_idx][lb_idx] == blocks[b_idx]) {
                                flag = true;
                                break;
                            }
                        }
                        if (flag == true) {
                            continue;
                        }

                        /* determine boundaries */
                        float b_x_left_boundary = n_col*((float)IMG_WIDTH/COL);
                        float b_x_right_boundary = ((n_col+1)*((float)IMG_WIDTH/COL));
                        float b_x_left_mid = b_x_left_boundary + ((float)IMG_HEIGHT/ROW*.4);
                        float b_x_right_mid = b_x_left_boundary + ((float)IMG_HEIGHT/ROW*.6);

                        float b_y_top_boundary = n_row*((float)IMG_HEIGHT/ROW);
                        float b_y_bot_boundary = ((n_row+1)*((float)IMG_HEIGHT/ROW));
                        float b_y_top_mid = b_y_top_boundary + ((float)IMG_WIDTH/COL*.4);
                        float b_y_bot_mid = b_y_top_boundary + ((float)IMG_WIDTH/COL*.6);

                        /* if in middle of boundary, check if right color or first guess */
                        if ((k >= b_x_left_mid) && (k <= b_x_right_mid) && (i >= b_y_top_mid) and (i <= b_y_bot_mid)) {
                            l_blocks[b_idx].push_back(blocks[b_idx]);
                            bool block_set_flag = false;
                            if (results[b_idx][blocks[b_idx]] == 'Z') {
                                block_set_flag = true;
                            }
                            else if (toFind[b_idx] == toHex(bgr_idx)) {
                                block_set_flag = true;
                            }
                            if (block_set_flag) {
                                rectangle(or_res, Point(int(b_x_left_boundary), int(b_y_top_boundary)), Point(int(b_x_right_boundary), int(b_y_bot_boundary)), COLOR_TABLE[bgr_idx], -1);
                                results[b_idx][blocks[b_idx]] = toHex(bgr_idx);
                            }
                        }
                    }
                }
            }
        }
    }
    for (int i = 0; i < 4; i++) {
        std::cout << results[i].length() << std::endl;
        std::cout << results[i] << std::endl;
    }
    std::cout << toFind << std::endl;
    imshow("result", or_res);
    waitKey(0);
    return false;
}

int main() {
    if (SAVE_TEMPLATE) {
        generateBorder(true);
    }
    if (PROCESS_SHOT_BEST_FIT) {
        decodeBorder(generateBorder(false));
    }
    return 0;
}
