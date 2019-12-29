#pragma once

#include <string>

using namespace std;

void info_print(const string& str);

void warning_print(const string& str);

void error_print(const string& str);

void debug_print(const string& str, bool endline = true);

void debug_print(const char* str, bool endline = true);

int is_error(int error_code);

string to_hex_str(const unsigned char* _bytes, size_t cnt);
