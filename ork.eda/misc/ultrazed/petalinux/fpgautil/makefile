CXX = aarch64-linux-gnu-g++-10

all:
	$(CXX) fpgautil.c --std=c++20 -Os -o fpgautil.exe

upl:
	scp fpgautil.exe root@$(ULTRAZED_IP):~/fpgautil.exe
