#include "../headers/cmd_manager.hpp"
#include "../headers/helper_fn.hpp"
#include <functional>
#include <iostream>
#include <print>
#include <utility>

void CmdManager::register_command(
    std::string cmd, std::move_only_function<bool(int, App &)> callback) {
  if (this->cmds.contains(cmd)) {
    std::println(std::cerr, "Command already exists");
    return;
  }

  this->cmds.emplace(std::move(cmd), std::move(callback));
}

std::move_only_function<bool(int, App &)> *
CmdManager::get_callback(std::string cmd) {
  std::transform(cmd.cbegin(), cmd.cend(), cmd.begin(),
                 [](char c) { return std::tolower(c); });

  trim_string(cmd);

  if (this->cmds.contains(cmd)) {
    return &this->cmds.at(cmd);
  }

  return nullptr;
}
