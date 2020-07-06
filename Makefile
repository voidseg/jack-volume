CXXFLAGS = -Ofast -std=c++11 -Wall -Wno-unused-function
LIBS = `pkg-config --libs jack` `pkg-config --libs libpulse` -pthread
INCLUDE = -I/usr/local/include/ -I/usr/include `pkg-config --cflags jack` `pkg-config --cflags libpulse`

CXX = c++

JV_SOURCES = jack_volume.cpp
JV_OBJS = $(JV_SOURCES:.cpp=.o) /usr/local/lib/libOSC++.a
JV_TARGET = jack-volume
PAV_SOURCES = pa_volume.cpp
PAV_OBJS = $(PAV_SOURCES:.cpp=.o) /usr/local/lib/libOSC++.a
PAV_TARGET = pa-volume

all: $(JV_OBJS) $(PAV_OBJS)
	$(CXX) -o $(JV_TARGET) $(JV_OBJS) $(LIBS)
	$(CXX) -o $(PAV_TARGET) $(PAV_OBJS) $(LIBS)

install: $(JV_TARGET) $(PAV_TARGET)
	install -d $(DESTDIR)/usr/local/bin
	install $(JV_TARGET) $(DESTDIR)/usr/local/bin/
	install	$(PAV_TARGET) $(DESTDIR)/usr/local/bin/
	install jvctl.py $(DESTDIR)/usr/local/bin/jvctl
	install udp_dispatcher.py $(DESTDIR)/usr/local/bin/udp_dispatcher
	install generic_connector.py $(DESTDIR)/usr/local/bin/generic_connector

clean:
	rm -f $(JV_SOURCES:.cpp=.o) $(JV_TARGET)
	rm -f $(PAV_SOURCES:.cpp=.o) $(PAV_TARGET)

%.o:%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@
