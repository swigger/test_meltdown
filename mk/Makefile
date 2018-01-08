
all: testmd

testmd:main.o meltreader.o
	gcc -o $@ $^ -lc++

vpath %.cpp ../src
