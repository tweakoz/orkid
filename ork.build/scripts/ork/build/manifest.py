import os, sys, common

deco = common.deco

###############################################################################

class Manifest:
	def __init__(self,name):
		self.name = name
		self.depends = list()
		self.scripts_folder = None
		self.manifest_root= None

	def add_dep(self,depname):
		self.depends.append(depname)

class ManifestsContainer:
	def __init__(self):
		self.manifests = dict()
	def add_project(self,name):
		found = self.manifests.get(name)
		if found==None:
			found = self.manifests[name]=Manifest(name)
		else:
			print deco("err","ERROR:") + deco("path", " project<%s> already found" % name )
		return found

	def depends(self,name,depname):
		prj = self.manifests.get(name)
		dep = self.manifests.get(depname)
		if (prj!=None) and (dep!=None):
			prj.add_dep(dep)
		#else:
		#	print deco("err","ERROR:") +deco("path","prj<%s>"%name) + " or " + deco( "path", "dep<%s>"%depname)

manifests = ManifestsContainer()
