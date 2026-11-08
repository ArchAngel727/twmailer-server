#include <functional>
#include <string>

typedef struct {
  std::string command;
  std::move_only_function<void(int)> callback;
} Command;
