
EXECUTABLE := cudaSaxpy

CU_FILES   := saxpy.cu

CU_DEPS    :=

CC_FILES   := main.cpp

###########################################################
ARCH := $(shell uname -m)

ifeq ($(ARCH), aarch64)
    CXX = g++ -g
else
    CXX = g++ -g -m64
endif

OBJDIR=objs
CXXFLAGS=-O0 -Wall
ifeq ($(ARCH), Darwin)
# Building on mac
LDFLAGS=-L/usr/local/depot/cuda-8.0/lib/ -lcudart
else
# Building on Linux
LDFLAGS=-L/usr/local/cuda/lib64/ -lcudart
endif
NVCC=nvcc
NVCCFLAGS=-O0 -G -g -m64


OBJS=$(OBJDIR)/main.o  $(OBJDIR)/saxpy.o


.PHONY: dirs clean

default: $(EXECUTABLE)

dirs:
		mkdir -p $(OBJDIR)/

clean:
		rm -rf $(OBJDIR) *.ppm *~ $(EXECUTABLE)

$(EXECUTABLE): dirs $(OBJS)
		$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)

$(OBJDIR)/%.o: %.cpp
		$(CXX) $< $(CXXFLAGS) -c -o $@

$(OBJDIR)/%.o: %.cu
		$(NVCC) $< $(NVCCFLAGS) -c -o $@
