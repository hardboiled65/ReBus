VERSION=0.1.0

default:
	mkdir -p build
	cd build ; qmake ../ReBus.pro
	cd build ; make

install:
	strip build/rebus-server
	cp build/rebus-server /usr/bin/

clean:
	rm -rf build
