#---------------------------------------------------------------------------------
# TARGET is the name of the output
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing extra header files
#---------------------------------------------------------------------------------
TARGET   := 005tools
SOURCES  := source
INCLUDES ?= -I./include `pkg-config libusb-1.0 --cflags`

#---------------------------------------------------------------------------------
# Options 
#---------------------------------------------------------------------------------
CC       ?= gcc
CFLAGS   ?= -Wall -g 

CXX      ?= g++
CXXFLAGS ?= -Wall -g -std=c++0x -o $(TARGET)

COBJS     = source/hid.o
CPPOBJS   = source/main.o source/tools.o
OBJS      = $(COBJS) $(CPPOBJS)

#---------------------------------------------------------------------------------
# Any additional libraries
#---------------------------------------------------------------------------------
LIBS      = `pkg-config libusb-1.0 libudev --libs`

all: $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LIBS) -o $(TARGET)

$(COBJS): %.o: %.c
	$(CC) $(CFLAGS) -c $(INCLUDES) $< -o $@

$(CPPOBJS): %.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $(INCLUDES) $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: clean
