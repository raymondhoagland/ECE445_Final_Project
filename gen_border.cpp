#include <iostream>
#include <math.h>

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>

#include <openssl/sha.h>

#include "gen_border.hpp"

/* defines for image handling */
#define IMG_HEIGHT 512
#define IMG_WIDTH 512
#define OFFSET_X 20
#define OFFSET_Y 20
#define ROW 10
#define COL 10

using namespace cv;

/* consts for border generation */
const short PADDING_BITS[] = {0, 1, 8, 9, 10, 11, 18, 19, 44, 45, 54, 55, 80, 81, 88, 89, 90, 91, 98, 99};
const short PAD_LEN_MID = 4;
const short PAD_LEN_END = 16;

/* encode blocks back to corresponding color */
char toHex(int c) {
    if (c < 10) {
        return '0'+c;
    }
    else {
        return 'A'+(c-10);
    }
}

/* percentage of matching characters (only equal length strings) */
float similarity(string s1, string s2) {
    int special_idx = 0;
    int total_wrong = 0;
	for (int i = 0; i < s1.length(); i++) {
        if (i == PADDING_BITS[special_idx]) {
            special_idx += 1;
        }
        else {
            if (s1[i] != s2[i]) {
                total_wrong += 1;
            }
        }
    }
    return 1-(float)total_wrong/(s1.length());
}

string generateBorder(const unsigned char *str, string output_file, bool create_img = false) {
    /* quad digits for now*/
    unsigned char hash[20];
    char quad_digs[ROW*COL+1];
    int special_idx = 0, idx = 0, hash_idx = 0, write_idx = 0, h1 = -1, h2 = -1, l1 = -1, l2 = -1;
    /* create SHA1 encoded string for security */
    SHA1(str, sizeof(str) - 1, hash);
    while (idx < ROW*COL) {
        /* ignore padding blocks */
        if (idx == PADDING_BITS[special_idx]) {
            special_idx += 1;
            quad_digs[idx] = '4';
        }
        else {
            /* take a hex digit and break into base 4 (4 primary colors) */
            if (h1 == -1) {
                int val = hash[hash_idx];
                h1 = val % 4;
                val /= 4;
                h2 = val % 4;
                val /= 4;
                l1  = val % 4;
                val /= 4;
                l2 = val % 4;
                val /= 4;
                hash_idx += 1;
            }
            if (l2 != -1) {
                quad_digs[idx] = l2+'0';
                l2 = -1;
            }
            else if (l1 != -1) {
                quad_digs[idx] = l1+'0';
                l1 = -1;
            }
            else if (h2 != -1) {
                quad_digs[idx] = h2+'0';
                h2 = -1;
            }
            else if (h1 != -1) {
                quad_digs[idx] = h1+'0';
                h1 = -1;
            }
        }
        idx += 1;
    }
    /* C-style string */
    quad_digs[100] = '\0';

    /* create the black image with white border */
    if (create_img) {
        Mat img(IMG_HEIGHT+OFFSET_Y*12, IMG_WIDTH+OFFSET_X*12, CV_8UC3, Scalar(0,0,0));
        rectangle(img, Point(0+OFFSET_Y*2, 0+OFFSET_X*2), Point(IMG_HEIGHT+OFFSET_Y*10, IMG_WIDTH+OFFSET_X*10), Scalar(255,255,255), OFFSET_Y*2);
        rectangle(img, Point(0+OFFSET_Y*4, 0+OFFSET_X*4), Point(IMG_HEIGHT+OFFSET_Y*8, IMG_WIDTH+OFFSET_X*8), Scalar(0,0,0), OFFSET_Y*2);
        /* color in the blocks */
        int spec_idx = 0, hex_idx = 0, block_idx = 0, digit;
        for (int k = 0; k < ROW; k++) {
            for (int i = 0; i < COL; i++) {
                digit = quad_digs[hex_idx]-'0';
                Scalar color = COLOR_TABLE[digit];
                hex_idx += 1;
                block_idx += 1;
                rectangle(img, Point(i*(IMG_HEIGHT/ROW)+OFFSET_Y*6, k*(IMG_WIDTH/COL)+OFFSET_X*6), Point((i+1)*(IMG_HEIGHT/ROW)+OFFSET_Y*6, (k+1)*(IMG_WIDTH/COL)+OFFSET_X*6), color, -1);
            }
        }

        /* save template */
        imwrite(output_file, img);
    }
    return quad_digs;
}

void decodeBorder(string input_file, string toFind) {
    /* load the image */
    Mat img = imread(input_file, CV_LOAD_IMAGE_COLOR);
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
                        if ((k >= b_x_left_mid) && (k <= b_x_right_mid) && (i >= b_y_top_mid) && (i <= b_y_bot_mid)) {
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

    /* determine best rotation string */
    float dist_score = 0.0;
    int idx = -1;
    for (int i = 0; i < 4; i++) {
        float new_score = similarity(results[i], toFind);
        if (new_score > dist_score) {
            dist_score = new_score;
            idx = i;
        }
    }
    std::cout << "result " << dist_score << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 6) {
        std::cout << "invalid args" << std::endl;
    }
    else {
        string should_encode = argv[1];
        string should_decode = argv[2];
        string em_addr_str = argv[3];
        const unsigned char *email_address = new unsigned char[em_addr_str.length()+1];
        strcpy((char *)email_address,em_addr_str.c_str());
        string encode_filename = argv[4];
        string decode_filename = argv[5];
        if (should_encode == "Y") {
            generateBorder(email_address, encode_filename, true);
        }
        if (should_decode == "Y") {
            string encoded_hash = generateBorder(email_address, encode_filename, false);
            decodeBorder(decode_filename, encoded_hash);
        }
    }
    return 0;
}
