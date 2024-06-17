from setuptools import setup, find_packages, Command
from setuptools.command.install import install

import os, sys
import stat

version = "0.0.1"

if "OBT_STAGE" in os.environ:
  print( "ERROR: do not run setup.py in a staging environment")
  sys.exit(-1)

# Read the long description from the README file
with open("README.md", "r", encoding="utf-8") as fh:
    long_description = fh.read()

def package_files(directory):
    data_files = []
    for root, dirs, files in os.walk(directory):
        for filename in files:
            if not (filename.endswith('.pyc') or '.egg-info' in root or '__pycache__' in root):
                filepath = os.path.join(root, filename)
                dest_dir = os.path.join('ork', root)
                data_files.append((dest_dir, [filepath]))
    return data_files

#########################################
# these are tricky
#  bin_priv has to be data so the files can have the x bit set
#  bin_pub goes in scripts (since they are in the venv's path)
#########################################

module_files = package_files('ork.data') 
dox_files = package_files('ork.dox') 
#example_files = package_files('examples')
#test_files = package_files('tests')
#binpub_files = [f[1][0] for f in package_files("bin_pub")]
#binpriv_files = package_files('bin_priv')
data_files = dox_files #module_files# + example_files + test_files + binpriv_files

#########################################

setup(
    name="orkid",
    version=version,
    author="Michael T. Mayers",
    author_email="michael@tweakoz.com",
    description="Orkid Media Engine",
    long_description=long_description,
    long_description_content_type='text/markdown',
    url="https://github.com/tweakoz/orkid",
    packages=find_packages(where="scripts"),
    package_dir={"": "obt.project/scripts"},
    include_package_data=True,
    data_files=data_files,
    #scripts=binpub_files,
    classifiers=[
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: MIT License",
        "Operating System :: OS Independent",
    ],
    python_requires='>=3.9',
    install_requires=[
        'pip>=24.0',
        'ork.build>=0.0.198',
    ]
)
