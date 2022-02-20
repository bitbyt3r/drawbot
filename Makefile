all: camera motor lidar webserver

camera: camera.cpp
	g++ -I/usr/local/include/opencv4 -L/usr/local/lib/arm-linux-gnueabihf -o camera camera.cpp -lopencv_imgcodecs -lopencv_core -lopencv_imgproc -lopencv_videoio

motor: motor.c serial.c
	gcc -Wall -o motor motor.c -lzmq

lidar: lidar.c serial.c
	gcc -Wall -o lidar lidar.c -lpigpio -lzmq

webserver: webserver.c
	gcc -Wall -o webserver -pthread webserver.c -lzmq -lws -I/usr/local/include/wsserver

clean:
	rm lidar motor