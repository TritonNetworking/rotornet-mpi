CFLAGS= -std=c++11 -Wall -Werror -pedantic -O3 -Wno-deprecated

default: rotor_test

rotor_test: src_rotor_test.cpp
	${CXX} -o rotor_test ${CFLAGS} src_rotor_test.cpp

clean:
	rm -rf rotor_test
