INCPATHS=-I../stage/include -I../stage/include/OpenEXR
LIBPATHS=-L../stage/lib
LIBS=-lIECore -lIECoreRI -lboost_system -ltbb

ridpytest.exe: ridpytest.cpp rihelper.obj variant.h rihelper.h
	clang++ -g ridpytest.cpp rihelper.obj -o ridpytest.exe --std=c++0x $(INCPATHS) $(LIBPATHS) $(LIBS)

rihelper.obj: rihelper.cpp variant.h rihelper.h
	clang++ -g -c rihelper.cpp -o rihelper.obj --std=c++0x $(INCPATHS)

clean:
	rm *.obj
	rm *.exe