CXX = aarch64-linux-gnu-g++-10

all:
	$(CXX) main.cpp --std=c++20 -Os -o tempstatus.exe

upl:
	scp tempstatus.exe root@$(ULTRAZED_IP):~/tempstatus.exe
