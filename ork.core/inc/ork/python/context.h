#pragma once
#include <string>

extern "C" {
  #include <Python.h>
}
#include <pybind11/pybind11.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <pybind11/embed.h>

namespace ork::python {

void init();

struct Context
{
public:
	Context();
	~Context();
	void call(const std::string& cmdstr);
  pybind11::module orkidModule();
};
Context& context();

bool isPythonEnabled();

}
