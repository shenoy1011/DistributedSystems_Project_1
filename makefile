all: crc crsd

crc: crc.cpp interface.h
	g++ -g -w -std=c++11 -o crc crc.cpp -lpthread

crsd: crsd.cpp interface.h
	g++ -g -w -std=c++11 -o crsd crsd.cpp -lpthread

clean:
	rm -rf *.o crc crsd