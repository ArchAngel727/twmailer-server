#include <cstdint>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

typedef struct {
  uint16_t port;
  fs::path dir;
  std::string buffer;
} App;
