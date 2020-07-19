#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cassert>

using namespace std;

int main(int argc, char** argv) {

  // xilinx-ams device path
  // https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/18842163/Zynq+UltraScale+MPSoC+AMS

  std::string XILINXAMSBASE = "/sys/bus/iio/devices/iio:device0/";

  auto getv = [](std::string fname) -> double {
    ifstream the_file(fname);
    assert(the_file.is_open());
    string line;
    bool ok = (bool)getline(the_file, line);
    // printf("line<%s>\n", line.c_str());
    assert(ok);
    the_file.close();
    return atof(line.c_str());
  };

  auto do_item = [&](string senspath, string desc) {
    double raw     = getv(XILINXAMSBASE + senspath + "_raw");
    double scale   = getv(XILINXAMSBASE + senspath + "_scale");
    double offset  = getv(XILINXAMSBASE + senspath + "_offset");
    double celcius = scale * (raw + offset) * 0.001;
    double farenh  = 32.0 + celcius * 9.0 / 5.0;
    printf("desc<%s> celcius<%g> fahrenheit<%g>\n", desc.c_str(), celcius, farenh);
  };

  do_item("in_temp0_ps_temp", "Temp_FPD(APU)");
  do_item("in_temp1_remote_temp", "Temp_LPD(RPU)");
  do_item("in_temp2_pl_temp", "Temp_PL(PL)");

  return 0;
}
