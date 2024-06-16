CXX = g++

CXXFLAGS = -std=c++11 -Wall -Wextra -pedantic

SRCS = uthreads.cpp thread.cpp
OBJS = $(SRCS:.cpp=.o)

#Define the target library file
TARGET = libuthreads.a

# Define the tar command and flags
TAR = tar
TARFLAGS = -cvf
TARNAME = ex2.tar
TARSRCS = $(SRCS) README Makefile thread.h

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJS)
	ar rcs $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: tar
tar:
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)

.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET) 
