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
