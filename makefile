all:
	scons -f root.sconstruct --site-dir ./ork.build/site_scons

j1:
	scons -f root.sconstruct --jobs=1 --site-dir ./ork.build/site_scons

fast:
	scons -f root.sconstruct fast --site-dir ./ork.build/site_scons

prep:
	scons -f root.sconstruct prep --site-dir ./ork.build/site_scons

env:
	./ork.build/bin/ork.build.init_env.py

get:
	mkdir -p ./stage/downloads
	scons -f root.sconstruct get --site-dir ./ork.build/site_scons

boost:
	scons -f root.sconstruct boost --site-dir ./ork.build/site_scons

ilm:
	scons -f root.sconstruct ilm --site-dir ./ork.build/site_scons

oiio:
	scons -f root.sconstruct oiio --site-dir ./ork.build/site_scons

ext:
	make boost
	make ilm
	make oiio

toz:
	make pristine
	tozkit_deps_build.py all

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
	make prep

assets:
	./do_assets.py
	
install:
	scons -f root.sconstruct install
