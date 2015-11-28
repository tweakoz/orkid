all:
	scons -f root.sconstruct --site-dir ./ork.build/site_scons

fast:
	scons -f root.sconstruct fast --site-dir ./ork.build/site_scons

prep:
	scons -f root.sconstruct prep --site-dir ./ork.build/site_scons

env:
	./ork.build/bin/ork.build.int_env.py

get:
	tozkit_deps_get.py all

toz:
	make pristine
	tozkit_deps_build.py all

oexr:
	tozkit_deps_build.py oexr

oiio:
	tozkit_deps_build.py oiio

osl:
	tozkit_deps_build.py osl

bundle:
	./ork.build/bin/ork.bundle.make.py

.PHONY: docs

docs: .
	rm -rf docs/ork.*
	doxygen ork.core/doc/doxyfile

pristine:
	rm -rf stage/ext_build
	tozkit_deps_build.py clean

clean:
	scons -c -f root.sconstruct --site-dir ./ork.build/site_scons
	rm -rf stage/include/ork
	rm -rf stage/include/orktool
	rm -rf stage/include/pkg
	rm -rf stage/include/bullet

assets:
	./do_assets.py
	
install:
	scons -f root.sconstruct install
