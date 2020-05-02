#pragma once

#include <algorithm>
#include <cmath>
#include <numeric>
#include <string>
#include <vector>

#include "common.h"

using namespace std;

void info_print(const string& str);

void warning_print(const string& str);

void error_print(const string& str);

void debug_print(const string& str, bool endline = true);

void debug_print(const char* str, bool endline = true);

int is_error(int error_code);

template <class T>
T sumVect(std::vector<T>& v);

size_t sumVectST(std::vector<size_t>& v);

string to_hex_str(const unsigned char* _bytes, size_t cnt);

double inline stddev(const std::vector<double>& vec)
{
    size_t sz = vec.size();
    if (sz == 1)
        return 0.0;

    // Calculate the mean
    double mean = std::accumulate(vec.begin(), vec.end(), 0.0) / sz;

    double var = 0;
    for (auto& item : vec) {
        var += (item - mean) * (item - mean);
    }
    var /= sz;
    return sqrt(var);
}