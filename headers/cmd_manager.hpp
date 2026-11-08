#include "../headers/app.hpp"
#include <functional>
#include <string>
#include <unordered_map>

class CmdManager {
  std::unordered_map<std::string, std::move_only_function<bool(int, App &)>>
      cmds;

public:
  void register_command(std::string, std::move_only_function<bool(int, App &)>);
  std::move_only_function<bool(int, App &)> *get_callback(std::string cmd);
};
