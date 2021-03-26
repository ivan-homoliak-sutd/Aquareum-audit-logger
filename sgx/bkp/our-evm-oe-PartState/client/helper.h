#pragma once

#include <iostream>
#include <stdio.h>
#include <string.h>
#include <boost/tokenizer.hpp>
#include <set>
#include <unordered_map>
#include <string>
#include <fmt/format_header_only.h>

typedef boost::char_separator<char> separator;

const std::string expand_var(const std::string token_text, std::unordered_map<std::string, std::string>& sh_vars);
std::string expand_vars(const char* command, std::unordered_map<std::string, std::string>& sh_vars);
bool correct_token_cnt(std::string& command, std::set<unsigned> allowedCnts, boost::tokenizer<separator>** tokens, uint* cnt);
