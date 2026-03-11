CC      = cc
CXX     = c++
LDFLAGS = -g

# If you have the resampler library installed, add -DHAS_SRC to the CFLAGS line, and -lsamplerate to the LIBS line

CFLAGS  = -g -O3 -Wall -MMD -MD -pthread
LIBS    = -lpthread -lmd -lmosquitto

SRCS = $(wildcard *.cpp)
OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

all:		FMGateway

FMGateway:	$(OBJS)
		$(CXX) $(OBJS) $(CFLAGS) $(LIBS) -o FMGateway

%.o: %.cpp
		$(CXX) $(CFLAGS) -c -o $@ $<
-include $(DEPS)

FMGateway.o: GitVersion.h FORCE

.PHONY: GitVersion.h

FORCE:

clean:
		$(RM) FMGateway *.o *.d *.bak *~ GitVersion.h

install:
		install -m 755 FMGateway /usr/local/bin/

# Export the current git version if the index file exists, else 000...
GitVersion.h:
ifneq ("$(wildcard .git/index)","")
	echo "const char *gitversion = \"$(shell git rev-parse HEAD)\";" > $@
else
	echo "const char *gitversion = \"0000000000000000000000000000000000000000\";" > $@
endif
