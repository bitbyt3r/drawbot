all: build flash

build:
	arduino-cli compile -b arduino:avr:mega

flash:
	arduino-cli upload -p /dev/ttyACM0 -b arduino:avr:mega -v

install:
	arduino-cli lib install "Adafruit Motor Shield V2 Library"
	arduino-cli core install arduino:avr
