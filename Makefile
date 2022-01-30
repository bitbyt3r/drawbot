all: motor lidar

motor: motor.c serial.c
	gcc -Wall -o motor motor.c -lzmq

lidar: lidar.c serial.c
	gcc -Wall -o lidar lidar.c -lpigpio -lzmq
