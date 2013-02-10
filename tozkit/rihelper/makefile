INCPATHS=-I$(TOZ_STAGE)/include -I$(TOZ_STAGE)/include/OpenEXR
LIBPATHS=-L$(TOZ_STAGE)/lib
CXXFLAGS = --std=c++0x $(INCPATHS) -g
LIBS=-lIECore -lIECoreRI -lboost_system -ltbb

OBJS = rihelper.os

all: rihelpertest.exe install

%.os: %.cpp
	clang++ -fPIC $(CXXFLAGS) -c $< -o $@ 

librihelper.so: $(OBJS)
	clang++ -shared -Wl,-soname,librihelper.so -o librihelper.so $(OBJS) $(LIBPATHS) $(LIBS)

rihelpertest.exe: librihelper.so
	clang++ rihelpertest.cpp $(CXXFLAGS) -L. -lm -lc -lrihelper $(LIBPATHS) $(LIBS)

install: librihelper.so
	mkdir -p $(TOZ_STAGE)/include/tozkit
	install -D *.h $(TOZ_STAGE)/include/tozkit/
	install -D librihelper.so $(TOZ_STAGE)/lib/librihelper.so

clean:
	rm *.obj
	rm *.so
