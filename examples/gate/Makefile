MUDUO_DIRECTORY ?= ${HOME}/build/release
MUDUO_INCLUDE = ${HOME}/muduo
MUDUO_LIBRARY = $(MUDUO_DIRECTORY)/lib
SRC = ./

CXXFLAGS = -g -O0 -Wall -Wextra -Werror \
		          -Wconversion -Wno-unused-parameter \
				         -Wold-style-cast -Woverloaded-virtual \
						        -Wpointer-arith -Wshadow -Wwrite-strings \
								       -march=native -rdynamic
INCLUDES = -I$(MUDUO_INCLUDE) -I$(SRC)

LDFLAGS = -L$(MUDUO_LIBRARY) -lmuduo_net -lmuduo_base -lpthread -lrt

#SRCS = $(wildcard *.cc)
#OBJS = $(addsuffix .o, $(basename $(SRCS)))
CC = g++

APP=gate1 echo unix_client
all: $(APP)

gate1 : gate1.o
	    $(CC)  $^ -o $@  $(LDFLAGS)

echo : echo.o
	    $(CC)  $^ -o $@  $(LDFLAGS)

unix_client : unix_client.cc
	    $(CC)  $^ -o $@ 

%.o : %.cc
	    $(CC) -c $< -o $@ $(CXXFLAGS) $(INCLUDES)
clean:
	    rm *.o $(APP)

.PHONY: all clean
