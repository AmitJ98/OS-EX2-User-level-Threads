# Define the compiler
CXX = g++

# Define the compiler flags
CXXFLAGS = -std=c++11 -Wall -Wextra -pedantic

# Define the source files and object files
SRCS = uthreads.cpp thread.cpp
OBJS = $(SRCS:.cpp=.o)

#Define the target library file
TARGET = libuthreads.a

# Define the tar command and flags
TAR = tar
TARFLAGS = -cvf
TARNAME = ex2.tar
TARSRCS = $(SRCS) README Makefile thread.h

# Default target
.PHONY: all
all: $(TARGET)

# Rule to create the static library
$(TARGET): $(OBJS)
	ar rcs $@ $^

# Rule to compile .cpp files into .o files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Rule to create the tar file
.PHONY: tar
tar:
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)

# Clean rule
.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET) $(TARNAME)
