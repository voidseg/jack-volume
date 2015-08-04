CXXFLAGS = -Ofast -std=c++11 -Wall -Wno-unused-function
LIBS = `pkg-config --libs jack` `pkg-config --libs libpulse` -pthread
INCLUDE = -I/usr/local/include/ -I/usr/include `pkg-config --cflags jack` `pkg-config --cflags libpulse`

GCC = g++

JV_SOURCES = jack_volume.cpp
JV_OBJS = $(JV_SOURCES:.cpp=.o) /usr/local/lib/libOSC++.a
JV_TARGET = jack-volume
PAV_SOURCES = pa_volume.cpp
PAV_OBJS = $(PAV_SOURCES:.cpp=.o) /usr/local/lib/libOSC++.a
PAV_TARGET = pa-volume

all: $(JV_OBJS) $(PAV_OBJS)
	$(GCC) -o $(JV_TARGET) $(JV_OBJS) $(LIBS)
	$(GCC) -o $(PAV_TARGET) $(PAV_OBJS) $(LIBS)

install: $(JV_TARGET) $(PAV_TARGET)
	cp -f $(JV_TARGET) /usr/local/bin/
	cp -f $(PAV_TARGET) /usr/local/bin/
	cp -f jvctl.py /usr/local/bin/jvctl
	cp -f udp_dispatcher.py /usr/local/bin/udp_dispatcher
	chmod a+rx /usr/local/bin/$(JV_TARGET)
	chmod a+rx /usr/local/bin/$(PAV_TARGET)
	chmod a+rx /usr/local/bin/jvctl
	chmod a+rx /usr/local/bin/udp_dispatcher

clean:
	rm -f $(JV_SOURCES:.cpp=.o) $(JV_TARGET)
	rm -f $(PAV_SOURCES:.cpp=.o) $(PAV_TARGET)

%.o:%.cpp
	$(GCC) $(CXXFLAGS) $(INCLUDE) -c $< -o $@
