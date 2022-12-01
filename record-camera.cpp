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
#include <sys/time.h>

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
    struct timeval tp;
    for (;;) {
        cap >> video;
	gettimeofday(&tp, NULL);
	std::string name = "./capture/color_" + std::to_string(tp.tv_sec) + "." + std::to_string(tp.tv_usec / 1000) + ".jpg";
	try {
            imwrite(name, video);
	} catch (...) {
	    printf("Failed to save frame\n");
	}

        auto now = high_resolution_clock::now();
        auto diff = duration_cast<microseconds>(now - start);
        frames = frames + 1;
        float fps = frames / ((float)diff.count() / 1000000);
        printf("FPS: %f\n", fps);

    }
	return 0;
}
 
