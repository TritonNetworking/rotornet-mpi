CFLAGS= -std=c++11 -Wall -Werror -pedantic -O3 -Wno-deprecated

default: hellocomet

hellocomet: src_hellocomet.cpp
	${CXX} -o hellocomet ${CFLAGS} src_hellocomet.cpp

clean:
	rm -rf hellocomet
