XPRESSDIR =/home/sindremo/xpress-bcl
INCLUDEDIRS = -I$(XPRESSDIR)/include
LIBDIRS = -L$(XPRESSDIR)/lib

CXXFLAGS =	-O2 -g -Wall -fmessage-length=0 $(INCLUDEDIRS)

OBJS =		cloudbroker-bcl.o entities.o pathgenerator.o CloudBrokerOptimisation.o CloudBrokerModel.o ColumnGeneration.o spprc.o

LIBS =		libjson/libjson.a -lxprb $(XPRESSDIR)/lib/libxprl.so.x7.6 $(XPRESSDIR)/lib/libxprs.so.25.01

TARGET =	cloudbroker-bcl

$(TARGET):	$(OBJS)
	$(CXX) $(LIBDIRS) -o $(TARGET) $(OBJS) $(LIBS)

all:	$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
