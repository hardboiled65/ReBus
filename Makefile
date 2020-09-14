VERSION=0.1.0

default:
	mkdir -p build
	cd build ; qmake ../ReBus.pro
	cd build ; make

clean:
	rm -rf build
