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
	gcc -o $@ $^ $(LIBS)

vpath %.cpp ../src
