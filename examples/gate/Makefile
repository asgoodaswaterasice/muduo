MUDUO_DIRECTORY ?= $(HOME)/build/release-install
MUDUO_INCLUDE = $(MUDUO_DIRECTORY)/include
MUDUO_LIBRARY = $(MUDUO_DIRECTORY)/lib
SRC = ./

CXXFLAGS = -g -O0 -Wall -Wextra -Werror \
		          -Wconversion -Wno-unused-parameter \
				         -Wold-style-cast -Woverloaded-virtual \
						        -Wpointer-arith -Wshadow -Wwrite-strings \
								       -march=native -rdynamic
INCLUDES = -I/$(MUDUO_INCLUDE) -I$(SRC)

LDFLAGS = -L$(MUDUO_LIBRARY) -lmuduo_net -lmuduo_base -lpthread -lrt

SRCS = $(wildcard *.cc)
OBJS = $(SRCS:.cc = .o)
	CC = g++

APP=gate

all: $(APP)

$(APP) : $(OBJS)
	    $(CC)  $^ -o $@  $(LDFLAGS)

%.o : %.cc
	    $(CC) -c $<  $(CXXFLAGS) $(INCLUDES)
clean:
	    rm *.o $(APP)

.PHONY: all clean
