#include "00Names.hpp"

String format(const char* format, ...) {
  va_list args;

  va_start(args, format);
  char buf[vsnprintf(NULL, 0, format, args)];
  vsprintf(buf, format, args);
  va_end(args);

  return String(buf);
}
