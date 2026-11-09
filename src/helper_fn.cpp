#include "../headers/helper_fn.hpp"
#include <sstream>

std::vector<std::string> split_string(const std::string &str) {
  std::vector<std::string> out;
  std::istringstream iss(str);
  std::string line;

  while (std::getline(iss, line)) {
    out.push_back(std::move(line));
  }

  return out;
}

void trim_string(std::string &str) {
  constexpr const char *to_remove = " \n\t\r";
  str.erase(str.find_last_not_of(to_remove) + 1);
  str.erase(0, str.find_first_not_of(to_remove));
}
