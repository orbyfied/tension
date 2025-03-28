#pragma once

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <iterator>

namespace tc {

/* Splits a string by whitespace */
std::vector<std::string> split_str_by_whitespace(std::string const &input);

inline void skip_whitespace(std::istream_iterator<char>& it) {
    while (*it == ' ' || *it == '\t') {
        it++;
    }
}

inline int parse_int(std::istream_iterator<char>& it, const std::istream_iterator<char>& end) {
    int res = 0;
    while (it != end && isdigit(*it)) {
        res *= 10;
        res += *it - '0';
        it++;
    }

    return res;
}

}