#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include <libv4l2.h>
#include <opencv2/core/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>  
#include <opencv2/videoio.hpp>

using namespace cv;

int main(int, char**) {
    VideoCapture cap;
    cap.cv::VideoCapture::open(0);

    if (!cap.isOpened()) {
        printf("Failed to open video camera\n");
        return -1;
    }

    Mat video;
    Mat grey;
    cap >> video;
    cvtColor(video,grey,cv::COLOR_BGRA2GRAY);
    imwrite("test.jpg", grey);
	return 0;
}
 