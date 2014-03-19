CXXFLAGS =	-O2 -g -Wall -fmessage-length=0 

OBJS =		cloudbroker-bcl.o entities.o pathgenerator.o

LIBS =		libjson/libjson.a

TARGET =	cloudbroker-bcl

$(TARGET):	$(OBJS)
	$(CXX) -o $(TARGET) $(OBJS) $(LIBS)

all:	$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
