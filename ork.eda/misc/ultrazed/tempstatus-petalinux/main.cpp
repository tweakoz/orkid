#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cassert>

using namespace std;

int main(int argc, char** argv) {

  std::string LPDAPU = "/sys/bus/iio/devices/iio:device0/in_temp0_ps_temp_raw";
  std::string FPDRPU = "/sys/bus/iio/devices/iio:device0/in_temp1_remote_temp_raw";
  std::string PL     = "/sys/bus/iio/devices/iio:device0/in_temp2_pl_temp_raw";

  auto getv = [](std::string fname) -> int {
    ifstream the_file(fname);
    assert(the_file.is_open());
    string line;
    bool ok = (bool)getline(the_file, line);
    assert(ok);
    the_file.close();
    return atoi(line.c_str());
  };

  int lpdval = getv(LPDAPU);
  int fpdval = getv(FPDRPU);
  int plval  = getv(PL);

  double lpd_temp_celcius = (double(lpdval) * 509.314 / 65536.0) - 280.23;
  double fpd_temp_celcius = (double(fpdval) * 509.314 / 65536.0) - 280.23;
  double pl_temp_celcius  = (double(plval) * 509.314 / 65536.0) - 280.23;

  printf("lpdval<%d> lpd_temp_celcius<%g>\n", lpdval, lpd_temp_celcius);
  printf("fpdval<%d> fpd_temp_celcius<%g>\n", fpdval, fpd_temp_celcius);
  printf("plval<%d> pl_temp_celcius<%g>\n", plval, pl_temp_celcius);
  return 0;
}
