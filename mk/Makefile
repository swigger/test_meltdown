CXXFLAGS=-O2 -g
LIBS=

HOST=$(shell uname -s)
ifeq ($(HOST), Linux)
    LIBS:=-lstdc++
endif
ifeq ($(HOST), Darwin)
	LIBS:=-lc++
endif

all: testmd

testmd:main.o meltreader.o
	gcc -g -o $@ $^ $(LIBS)

clean:
	-@rm -f testmd *.o

vpath %.cpp ../src
