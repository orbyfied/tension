#include "util.hh"

namespace tc {

std::vector<std::string> split_str_by_whitespace(std::string const &input) { 
    std::istringstream buffer(input);
    std::vector<std::string> ret;

    std::copy(std::istream_iterator<std::string>(buffer), 
                std::istream_iterator<std::string>(),
                std::back_inserter(ret));
    return ret;
}

}