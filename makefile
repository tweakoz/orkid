SITEDIR = --site-dir ./ork.build/site_scons
SCONSFILE = -f root.sconstruct
all:
	scons $(SCONSFILE) $(SITEDIR)

j1:
	scons $(SCONSFILE) --jobs=1 $(SITEDIR)

debug:
	scons $(SCONSFILE) debug=1 $(SITEDIR)

fast:
	scons $(SCONSFILE) fast $(SITEDIR)

prep:
	scons $(SCONSFILE) prep $(SITEDIR)

env:
	./ork.build/bin/ork.build.init_env.py

get:
	mkdir -p ./stage/downloads
	scons $(SCONSFILE) get $(SITEDIR)

boost:
	scons $(SCONSFILE) boost $(SITEDIR)

ilm:
	scons $(SCONSFILE) ilm $(SITEDIR)

oiio:
	scons $(SCONSFILE) oiio $(SITEDIR)

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
	scons -c $(SCONSFILE) $(SITEDIR)
	rm -rf stage/include/ork
	rm -rf stage/include/orktool
	rm -rf stage/include/pkg
	rm -rf stage/include/bullet
	make prep

assets:
	./do_assets.py
	
install:
	scons $(SCONSFILE) install
