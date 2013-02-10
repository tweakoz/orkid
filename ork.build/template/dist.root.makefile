all:
	scons -f root.sconstruct 

env:
	./ork.build/bin/setenv.py

get:
	tozkit_deps_get.py all

toz:
	tozkit_deps_build.py all

pristine:
	make clean
	rm -rf stage/ext_build
	tozkit_deps_build.py clean

clean:
	scons -c -f root.sconstruct 
	rm -rf stage/include/ork
	rm -rf stage/include/orktool
	rm -rf stage/include/pkg
	rm -rf stage/include/bullet

install:
	scons -f root.sconstruct install
