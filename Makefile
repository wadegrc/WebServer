all: main

main: main.o treatRequest.o timer.o epoll.o util.o
	g++ $^ -lpthread -std=c++11 -o $@

clean:
	-rm main *.o

.PHONY: clean

sources = main.cpp epoll.cpp timer.cpp treatRequest.cpp util.cpp

include $(sources:.cpp=.d)

%.d: %.cpp
	set -e; rm -f $@; \
	$(CXX) -MM $(CXXFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

CFLAGS := -std=c++11 -g -Wall -O3
CXXFLAGS:= $(CFLAGS)
