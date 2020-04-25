#include "test.h"
////////////////////////////////////////////////////////////////////////////////
Icecream::Icecream(const char* flavor)
    : _theflavor(flavor) {

  int on_stack = 0;
  int* on_heap = new int(5);
  printf("addr-this <%p>\n", this);
  printf("addr-flavor <%p>\n", &flavor);
  printf("addr-_theflavor <%p>\n", &_theflavor);
  printf("addr-on_stack <%p>\n", &on_stack);
  printf("addr-on_heap <%p>\n", on_heap);
  printf("Whats your flavor <%s>\n", flavor);
  printf("Whats your flavor <%s>\n", _theflavor.c_str());
}
////////////////////////////////////////////////////////////////////////////////
const std::string& Icecream::getFlavor() const {
  return _theflavor;
}
////////////////////////////////////////////////////////////////////////////////
