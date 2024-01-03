import sys
import os
import site
import sysconfig

print("sys.path Components Analysis\n")

# 1. Standard Library and Site-packages
print("Standard Library and Site-packages Directories:")
for path in sys.path:
    if 'site-packages' in path or 'dist-packages' in path:
        print(f"  - Site-packages: {path}")
    else:
        print(f"  - Standard Lib: {path}")

# 2. Current Working Directory
print("\nCurrent Working Directory:")
print(f"  - {os.getcwd()}")

# 3. PYTHONPATH Environment Variable
print("\nPYTHONPATH Environment Variable:")
pythonpath = os.environ.get('PYTHONPATH')
if pythonpath:
    print(f"  - {pythonpath}")
else:
    print("  - Not set")

# 4. Site-Specific Directories
print("\nSite-Specific Directories (added by the site module):")
site_dirs = site.getsitepackages()
for site_dir in site_dirs:
    print(f"  - {site_dir}")

# 5. Python Executable Location
print("\nPython Executable Location:")
print(f"  - {os.path.dirname(sys.executable)}")

# 6. .pth Files
print("\n.pth Files:")
for site_dir in site_dirs + [sysconfig.get_path('purelib'), sysconfig.get_path('platlib')]:
    if os.path.isdir(site_dir):
        pth_files = [f for f in os.listdir(site_dir) if f.endswith('.pth')]
        for file in pth_files:
            print(f"  - {file} in {site_dir}")

# 7. Compile-time Configuration Variables
print("\nCompile-Time Configuration Variables:")
config_vars = sysconfig.get_config_vars()
for key, value in config_vars.items():
    if 'dir' in key.lower() or 'path' in key.lower() or 'lib' in key.lower():
        print(f"  - {key}: {value}")

# 8. Complete sys.path
print("\nComplete sys.path:")
print(sys.path)
