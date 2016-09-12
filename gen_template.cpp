#include <iostream>
#include <math.h>
#include <limits>
#include <sstream>

#include <opencv2/highgui/highgui.hpp>
#include "opencv2/opencv.hpp"

#include "gen_border.hpp"

#define IMG_HEIGHT 512
#define IMG_WIDTH 512
#define COLOR_TABLE_LEN 5

using namespace cv;

const float pi = (float)(22/7);

Mat sub_img(Mat img, Point2f center, float theta,int width, int height){
	Mat out;
	Point pt(width/2, height/2);

	/* rotates image by theta(radians) */
	Mat M = getRotationMatrix2D(pt,theta*180/pi,1);
	Size s_1 (height,width);
	warpAffine(img,out,M,s_1);

    return out;
}

void white_out(Mat img,Point top_l,Point bottom_r, vector<Point> contour){
	/* white out unwanted pixels in bounded rectangle but outside of contour */
	for(int x = top_l.x; x< bottom_r.x; x++){
		for(int y = top_l.y; y<bottom_r.y; y++){
			Point test (x,y);
			double dist = pointPolygonTest(contour,test,true);
			if(dist < 1) {
				img.at<Vec3b>(y,x) = Vec3b(255,255,255);
			}
		}
	}
}

void find_matching_contour(Mat im, vector< vector<Point> >* contours, int* best_cnt_idx )
{
	Mat imgray;
	Mat thresh;
	vector<Vec4i> hierarchy;
	double parent_area, child_area;
	float ratio, min_ratio = -1;

	/* convert to grayscale, find contours */
	cvtColor(im,imgray,COLOR_BGR2GRAY);
	threshold(imgray,thresh,127,255,0);
	findContours(thresh,*contours,hierarchy,CV_RETR_TREE,CV_CHAIN_APPROX_SIMPLE);

	*(best_cnt_idx) = -1;
	for(int x = 0; x<hierarchy.size(); x++){
		if((int)hierarchy[x][3] == -1) {
			continue;
		}

		parent_area = contourArea((*contours)[hierarchy[x][3]]);
		child_area = contourArea((*contours)[x]);

		/* child insignificant */
		if(child_area<200) {
			continue;
		}

		/* desired ratio range between white outline and color border */
		ratio = (float)parent_area/child_area;
		if(ratio > .85 && ratio < 2.0) {
			//if (ratio > min_ratio) {
				*(best_cnt_idx) = x;
				min_ratio = ratio;
				break;
			//}
		}
	}

}

void find_contour_rect(vector< vector<Point> > contours, int best_cnt_idx, Point* fst, Point* snd){
	int fst_x = std::numeric_limits<int>::max();
	int fst_y = std::numeric_limits<int>::max();
	int snd_x = 0;
	int snd_y = 0;

	/* setup coordinates to crop (top left, bottom right) */
	for (int x = 0; x < contours[best_cnt_idx].size(); x++){
		if(contours[best_cnt_idx][x].x < fst_x)
			fst_x = contours[best_cnt_idx][x].x;
		if(contours[best_cnt_idx][x].x > snd_x)
			snd_x = contours[best_cnt_idx][x].x;
		if(contours[best_cnt_idx][x].y < fst_y)
			fst_y = contours[best_cnt_idx][x].y;
		if(contours[best_cnt_idx][x].y > snd_y)
			snd_y = contours[best_cnt_idx][x].y;
	}

	fst->x = fst_x;
	fst->y = fst_y;
	snd->x = snd_x;
	snd->y = snd_y;
}

void find_contour_rotated(Mat im, vector< vector<Point> >* contours, int* child, int* bst_idx){
	Mat imgray;
	Mat thresh;
	vector<Vec4i> hierarchy;
	int bst_area = -1;
	*bst_idx = -1;

	/* convert to grayscale and find contours */
	cvtColor(im,imgray,COLOR_BGR2GRAY);
	threshold(imgray,thresh,127,255,0);
	findContours(thresh,*contours,hierarchy,CV_RETR_TREE,CV_CHAIN_APPROX_SIMPLE);

	/* find largest contour area */
	for(int x = 0 ; x<contours->size(); x++){
		double area = contourArea((*contours)[x]);
		if(area > bst_area){
			bst_area = area;
			*bst_idx = x;
		}
	}

	*child = hierarchy[*bst_idx][2];
}

Mat recrop(Mat img, int lower_bound, int upper_bound) {
	Point fst(IMG_WIDTH*2, IMG_HEIGHT*2);
	Point snd(-1, -1);

	Mat hsv_color;
    hsv_color.create(1,1,CV_8UC(3));
    Mat hsv_img(IMG_HEIGHT, IMG_WIDTH, CV_LOAD_IMAGE_COLOR, Scalar(0,0,0));
    Mat mask(IMG_HEIGHT, IMG_WIDTH, CV_8UC3, Scalar(0,0,0));

	/* find purple borders */
	for (int i = COLOR_TABLE_LEN-1; i < COLOR_TABLE_LEN; i++) {
		cvtColor(img, hsv_img, CV_BGR2HSV);
        Scalar bgr = COLOR_TABLE[i];
        Mat color(1, 1, CV_8UC(3), bgr);
        cvtColor(color, hsv_color, CV_BGR2HSV);
        inRange(hsv_img, Scalar(hsv_color.at<Vec3b>(0,0)[0]-10,lower_bound,lower_bound), Scalar(hsv_color.at<Vec3b>(0,0)[0]+10,upper_bound,upper_bound), mask);
        Mat res;
        bitwise_and(img,img,res, mask=mask);

		for (int i = 0; i < res.rows; i++) {
            for (int k = 0; k < res.cols; k++) {
				int b = (int)res.at<Vec3b>(i, k)[0];
                int g = (int)res.at<Vec3b>(i, k)[1];
                int r = (int)res.at<Vec3b>(i, k)[2];
                if ((b > 0) || (g > 0) || (r > 0)) {
					if (k < fst.x) {
						fst.x = k;
					}
					if (k > snd.x) {
						snd.x = k;
					}
					if (i < fst.y) {
						fst.y = i;
					}
					if (i > snd.y) {
						snd.y = i;
					}
                }
			}
		}
	}

	/* crop around purple borders and resize again --> img should be rotated already so should be okay */
	Mat crop_img  = img(Rect(fst.x, fst.y,snd.x-fst.x,snd.y-fst.y));
	resize(crop_img, crop_img, Size(IMG_HEIGHT, IMG_WIDTH));
	return crop_img;
}

int process(string input_filename, string output_filename, int lower_bound, int upper_bound){
	Mat img = imread(input_filename, CV_LOAD_IMAGE_COLOR);
	try {
		resize(img, img, Size(IMG_HEIGHT*1.2, IMG_WIDTH*1.2));
	}
	catch (cv::Exception) {
		std::cout << "file being modified..failed" << std::endl;
		return -1;
	}
	vector< vector<Point> > contours;
	int best_cnt_idx, mult = 0, divis = 30, best_x, best_y, best_w, best_h;
	float min_ratio = std::numeric_limits<float>::max();
	Point fst;
	Point snd;
	vector<int> bst_dims;
	Mat bst_img;

	/* find the contour we want to crop */
	find_matching_contour(img, &contours, &best_cnt_idx);
	if(best_cnt_idx == -1) {
		std::cout << "missing contour.." << std::endl;
		return -1;
	}

	/* find bounding rect coords and whiteout extra pixels */
	find_contour_rect(contours,best_cnt_idx, &fst, &snd);
	Point new_fst(std::max(0, fst.x-5),std::max(0, fst.y-5));
	Point new_snd(std::min(snd.x+5, IMG_WIDTH),std::min(snd.y+5, IMG_HEIGHT));

	white_out(img,new_fst,new_snd,contours[best_cnt_idx]);

	/* crop out the desirect rect and resize up */
	Mat crop_img = img(Rect(new_fst.x, new_fst.y,new_snd.x-new_fst.x,new_snd.y-new_fst.y));
	resize(crop_img, crop_img, Size(IMG_HEIGHT, IMG_WIDTH));

	/* try different rotations in 5 degree increments */
	Point center(IMG_HEIGHT/2,IMG_WIDTH/2);
	while(mult <= 15) {
		Mat rotated = sub_img(crop_img,center,pi/divis*mult,512,512);

		vector< vector<Point> > contours;
		int child;
		int bst_idx;
		find_contour_rotated(rotated, &contours, &child, &bst_idx);
		if (child == -1) {
			mult += 1;
			continue;
		}
		Rect curr_rect = boundingRect(contours[child]);

		double contour_area = contourArea(contours[child]);
		int x = curr_rect.x;
		int y = curr_rect.y;
		int w = curr_rect.width;
		int h = curr_rect.height;
		double bounded_area = w*h;
		double ratio = bounded_area/contour_area;

		/* ratio between bounding rect and contour --> MIN for 90 degree angles */
		if(ratio < min_ratio){
			min_ratio = ratio;
			best_x = x;
			best_y = y;
			best_w = w;
			best_h = h;
			bst_img = rotated;
		}
		mult++;
	}

	/* if able to rotate within reasonable bounds, resize and finish */
	if(min_ratio < 1.2){
		Mat im = bst_img(Rect(best_x, best_y, best_w, best_h));
		resize(im, im, Size(IMG_HEIGHT, IMG_WIDTH));
		Mat crop_final = recrop(im, lower_bound, upper_bound);
		imwrite(output_filename, crop_final);
	}

	else {
		std::cout << "rotate failed" << std::endl;
		return -1;
	}
	return 0;
}

int main(int argc, char* argv[]){
	if (argc < 5) {
		std::cout << "invalid args" << std::endl;
	}
	else {
		string input_filename = argv[1];
		string output_filename = argv[2];

		std::istringstream s3(argv[3]);
		int lower_bound, upper_bound;
		if (!(s3 >> lower_bound)) {
			std::cout << "invalid args" << std::endl;
		}
		std::istringstream s4(argv[4]);
		if (!(s4 >> upper_bound)) {
			std::cout << "invalid args" << std::endl;
		}

		if (process(input_filename, output_filename, lower_bound, upper_bound) == 0) {
			std::cout << "completed" << std::endl;
		}
	}
	return 0;
}
