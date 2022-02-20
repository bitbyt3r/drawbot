#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <unistd.h>
#include <linux/videodev2.h>
#include <libv4l2.h>
#include <opencv2/core/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>  
#include <opencv2/videoio.hpp>
#include <chrono>

using namespace std::chrono;
using namespace cv;
using namespace std;

int main(int, char**) {
    VideoCapture cap;
    cap.cv::VideoCapture::open(0);
    cap.cv::VideoCapture::set(CAP_PROP_FRAME_WIDTH, 1920);
    cap.cv::VideoCapture::set(CAP_PROP_FRAME_HEIGHT, 1080);

    if (!cap.isOpened()) {
        printf("Failed to open video camera\n");
        return -1;
    }
    
    RNG rng(12345);
    Mat video;
    Mat grey;
    auto start = high_resolution_clock::now();
    int frames = 0;
    for (;;) {
        cap >> video;
        cvtColor(video, grey, COLOR_BGRA2GRAY);
        //imwrite("grey.jpg", grey);

        Mat thresh;
        threshold(grey, thresh, 128, 255, THRESH_BINARY);
        //imwrite("threshold.jpg", thresh);
        Mat canny;
        Canny(grey, canny, 128, 255);
        //imwrite("canny.jpg", canny);

        vector<vector<Point> > contours;
        findContours(canny, contours, RETR_TREE, CHAIN_APPROX_SIMPLE, Point(0, 0));
        vector<RotatedRect> minRect( contours.size() );
        vector<RotatedRect> minEllipse( contours.size() );
        for( size_t i = 0; i < contours.size(); i++ )
        {
            minRect[i] = minAreaRect( contours[i] );
            if( contours[i].size() > 5 )
            {
                minEllipse[i] = fitEllipse( contours[i] );
            }
        }
        Mat drawing = video;
        for( size_t i = 0; i< contours.size(); i++ )
        {
            Scalar color = Scalar( rng.uniform(0, 256), rng.uniform(0,256), rng.uniform(0,256) );
            // contour
            drawContours( drawing, contours, (int)i, color );
            // ellipse
            ellipse( drawing, minEllipse[i], color, 2 );
            // rotated rectangle
            Point2f rect_points[4];
            minRect[i].points( rect_points );
            for ( int j = 0; j < 4; j++ )
            {
                line( drawing, rect_points[j], rect_points[(j+1)%4], color );
            }
        }
        imwrite("output.jpg", drawing);
        auto now = high_resolution_clock::now();
        auto diff = duration_cast<microseconds>(now - start);
        frames = frames + 1;
        float fps = frames / ((float)diff.count() / 1000000);
        printf("FPS: %f, Ellipses: %d\n", fps, contours.size());
    }
	return 0;
}
 