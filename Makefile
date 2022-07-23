all: main.cpp
	g++ main.cpp -std=c++20 -lSDL2 -lpthread -lOpenCL -g3 -ggdb -O0
