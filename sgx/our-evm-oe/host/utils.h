#pragma once

#include <string>

using namespace std;

void info_print(const string &str);

void warning_print(const string &str);

void error_print(const string &str);

int is_error(int error_code);

string to_hex_str(const unsigned char * _bytes, size_t cnt);
