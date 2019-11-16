#ifndef UTIL_H_
#define UTIL_H_

#include "data_types.h"

void info_print(const char* str);

void warning_print(const char* str);

void error_print(const char* str);

int is_error(int error_code);

#endif // UTIL_H_