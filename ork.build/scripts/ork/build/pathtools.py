import os, sys
import fnmatch

###############################################################################

def posixpath(path):
	return '/'.join(os.path.normpath(path).split(os.sep))

###############################################################################

def recursive_glob_get_dirs(path):
	d=[]
	try:
		for i in os.listdir(path):
			if os.path.isdir(path+i):
				d.append(os.path.basename(i))
	except:pass
	return d

###############################################################################

def recursive_patglob(path,pattern):
	l=[]
	if path[-1]!='/':
		path=path+'/'
	for i in recursive_glob_get_dirs(path):
		#print path+i
		l=l+recursive_patglob(path+i,pattern)
	try:
		dirlist = os.listdir(path)
		for i in dirlist:
			ii=i
			i=path+i
			#print i
			if os.path.isfile(i):
				if fnmatch.fnmatch(i,pattern):
					l.append(i)
	except:
		pass
	
	return l
	
###############################################################################

def recursive_glob(path):
	l=[]
	if path[-1]!='/':
		path=path+'/'
	for i in recursive_glob_get_dirs(path):
		#print path+i
		l=l+recursive_glob(path+i)
	try:
		dirlist = os.listdir(path)
		for i in dirlist:
			ii=i
			i=path+i
			if os.path.isfile(i):
				#print i
				#if fnmatch.fnmatch(ii,pattern):
				#print "Matched %s" % (i)
				l.append(i)
	except:
		pass
	
	return l
