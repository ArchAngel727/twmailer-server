#include "../headers/cmd_manager.hpp"
#include "../headers/data_manager.hpp"
#include "../headers/helper_fn.hpp"
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <cerrno>
#include <charconv>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <getopt.h>
#include <iostream>
#include <netinet/in.h>
#include <optional>
#include <print>
#include <ranges>
#include <signal.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

volatile sig_atomic_t is_running = 1;

int send_str(int fd, const std::string &str) {
  std::string out = str;

  // NOTE: Spec doc requires '\n' as line terminator
  if (str.empty() || str.back() != '\n') {
    out.push_back('\n');
  }

  const char *p = out.data();
  size_t left = out.size();

  // NOTE: Loop because sometimes send() does not send everything
  while (left > 0) {
    ssize_t n = send(fd, p, left, MSG_NOSIGNAL);

    if (n == -1) {
      std::perror("Failed to send str to server");
      return EXIT_FAILURE;
    }

    p += n;
    left -= static_cast<size_t>(n);
  }

  return EXIT_SUCCESS;
}

std::optional<int> parse_int(std::string_view str) {
  int value = 0;

  auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);

  // NOTE: Check if parse was sucessful
  if (ec != std::errc{}) {
    return std::nullopt;
  }

  // NOTE: Check if the whole string was consumed
  if (ptr != str.data() + str.size()) {
    return std::nullopt;
  }

  return value;
}

std::optional<App> read_args(int argc, char *argv[]) {
  // NOTE: Return if not enough args
  if (argc < 3) {
    return std::nullopt;
  }

  uint16_t port = 0;
  fs::path path;
  std::string buffer;
  buffer.reserve(4096);

  // NOTE: Parse port
  auto parsed_int = parse_int(argv[1]);

  if (!parsed_int) {
    std::println(std::cerr, "Invalid port: {}", argv[1]);
    return std::nullopt;
  }

  if (*parsed_int > 65535) {
    std::println(std::cerr, "Invalid port range: {}", *parsed_int);
    return std::nullopt;
  }

  port = (uint16_t)*parsed_int;

  // NOTE: Get cwd
  std::error_code ec;
  path = fs::absolute(argv[2], ec).lexically_normal();

  if (ec) {
    std::println(std::cerr, "Could not get cwd: {}", ec.message());
    return std::nullopt;
  }

  // NOTE: Create new app instance
  return App{
      .port = port,
      .dir = path,
      .buffer = buffer,
  };
}

std::optional<std::string> recv_line(int fd, std::string &buffer) {
  while (true) {
    size_t pos = buffer.find('\n');

    if (pos != std::string::npos) {
      std::string line = buffer.substr(0, pos);
      buffer.erase(0, pos + 1);

      if (!line.empty() && line.back() == '\r') {
        line.pop_back();
      }

      return line;
    }

    char buf[4096];
    ssize_t n = recv(fd, buf, sizeof(buf), 0);

    if (n == 0) {
      return std::nullopt;
    }

    if (n == -1) {
      if (errno == EINTR) {
        if (is_running) {
          continue;
        }

        return std::nullopt;
      }

      if (errno != EINTR) {
        std::perror("recv");
        return std::nullopt;
      }
    }

    buffer.append(buf, static_cast<size_t>(n));
  }
}

void register_cmds(CmdManager &cmd_manager) {
  cmd_manager.register_command(std::string("send"), [](int fd, App &app) {
    std::vector<std::string> lines;
    std::string return_msg = "ERR";

    while (auto str = recv_line(fd, app.buffer)) {
      if (!str->empty()) {
        if (*str == ".") {
          break;
        }

        trim_string(*str);
        lines.push_back(std::move(*str));
      }
    }

    std::string message = lines | std::views::drop(3) |
                          std::views::join_with('\n') |
                          std::ranges::to<std::string>();

    Mail mail = Mail{
        .from = lines[0],
        .to = lines[1],
        .subject = lines[2],
        .message = message,
    };

    if (DataManager::write_mail(app.dir, mail)) {
      return_msg = "OK";
    }

    send_str(fd, return_msg);

    return true;
  });

  cmd_manager.register_command(std::string("list"), [](int fd, App &app) {
    std::string user, response_msg = "ERR";

    while (auto line = recv_line(fd, app.buffer)) {
      if (!line->empty()) {
        user = *line;
        break;
      }
    }

    auto mails = DataManager::list_mail_for_user(app.dir, user);

    if (!mails.has_value()) {
      send_str(fd, response_msg);
      return true;
    }

    response_msg = std::format("{}\n", mails->size());

    for (auto [i, mail] : std::views::enumerate(*mails)) {
      response_msg += std::format("{}: {}", i + 1, mail);
    }

    std::println("Sending response: '{}'", response_msg);

    send_str(fd, response_msg);

    return true;
  });

  cmd_manager.register_command(std::string("read"), [](int fd, App &app) {
    size_t msg_number;
    std::string user, response_msg = "ERR";
    std::vector<std::string> lines;

    while (auto line = recv_line(fd, app.buffer)) {
      if (!line->empty()) {
        lines.push_back(std::move(*line));

        if (lines.size() >= 2) {
          break;
        }
      }
    }

    auto parsed_int = parse_int(lines.at(1));

    if (!parsed_int) {
      std::println(std::cerr, "Could not parse int");
      send_str(fd, response_msg);
      return true;
    }

    user = lines.at(0);
    msg_number = static_cast<size_t>(*parsed_int);

    auto mails = DataManager::list_mail_for_user(app.dir, user);

    if (!mails.has_value()) {
      std::println(std::cerr, "Could not get mails");
      send_str(fd, response_msg);
      return true;
    }

    if (msg_number == 0 || msg_number > mails->size()) {
      std::println(std::cerr, "Invalid index: {}", msg_number);
      send_str(fd, response_msg);
      return true;
    }

    std::string mail_name = (*mails)[msg_number - 1];
    auto mail = DataManager::get_mail_unserialized(app.dir, user, mail_name);

    if (!mail.has_value()) {
      std::println(std::cerr, "Invalid mail");
      send_str(fd, response_msg);
      return true;
    }

    response_msg = *mail;

    send_str(fd, response_msg);

    return true;
  });

  cmd_manager.register_command(std::string("del"), [](int fd, App &app) {
    size_t msg_number;
    std::string user, response_msg = "ERR";
    std::vector<std::string> lines;

    while (auto line = recv_line(fd, app.buffer)) {
      if (!line->empty()) {
        lines.push_back(std::move(*line));

        if (lines.size() >= 2) {
          break;
        }
      }
    }

    auto parsed_int = parse_int(lines.at(1));

    if (!parsed_int) {
      send_str(fd, "Err");
      return true;
    }

    user = lines.at(0);
    msg_number = static_cast<size_t>(*parsed_int);

    auto mails = DataManager::list_mail_for_user(app.dir, user);

    if (!mails.has_value()) {
      std::println(std::cerr, "Could not get mails");
      send_str(fd, response_msg);
      return true;
    }

    if (msg_number == 0 || msg_number > mails->size()) {
      std::println(std::cerr, "Invalid index: {}", msg_number);
      send_str(fd, response_msg);
      return true;
    }

    std::string mail_name = (*mails)[msg_number - 1];

    if (!DataManager::delete_mail(app.dir, user, mail_name)) {
      send_str(fd, response_msg);
      std::println(std::cerr, "Failed to delete mail {} for user {}", mail_name,
                   user);
      return true;
    }

    std::println("Deleting message {} from user {}", msg_number, user);

    send_str(fd, "OK");

    return true;
  });

  cmd_manager.register_command(std::string("quit"), [](int, App &) {
    std::println("User disconnected");
    return false;
  });
}

void signal_callback(int sig) {
  if (sig == SIGINT || sig == SIGTERM) {
    is_running = false;
  }
}

void init_sig_handler() {
  struct sigaction sa{};
  sa.sa_handler = signal_callback;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  sigaction(SIGINT, &sa, nullptr);
  sigaction(SIGTERM, &sa, nullptr);
}

int init_server(App &app) {
  // NOTE: Opening socket
  int server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in addr;

  // NOTE: Data for registering socket with the kernel
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(app.port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  // NOTE: Setting socket options
  int opt = 1;
  if (setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt,
                 sizeof(opt)) == -1) {
    std::perror("setsockopt");
    close(server_socket_fd);
    return EXIT_FAILURE;
  }

  // NOTE: Bind socket to address and port
  if (bind(server_socket_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1 ||
      listen(server_socket_fd, 4) == -1) {
    std::perror("bind/listen error");
    close(server_socket_fd);
    return EXIT_FAILURE;
  }

  return server_socket_fd;
}

int main(int argc, char *argv[]) {
  auto app = read_args(argc, argv);
  CmdManager cmd_manager;

  // NOTE: Checking args and constructing app
  if (!app.has_value()) {
    std::println(stderr, "Invalid args");
    return EXIT_FAILURE;
  }

  // NOTE: Initialise SIG_INT and SIG_TERM handler
  init_sig_handler();

  // NOTE: Register callback lambdas for each command
  register_cmds(cmd_manager);

  std::println("Starting server...");

  // NOTE: Initialise server and get file descriptor
  int server_socket_fd = init_server(*app);

  std::println("Server started");

  // NOTE: This loop is where client connections spawn
  while (is_running) {
    // NOTE: Accept client connection
    int client_fd = accept(server_socket_fd, nullptr, nullptr);

    if (client_fd == -1) {
      if (errno == EINTR) {
        if (!is_running) {
          break;
        }

        continue;
      }

      std::perror("client fd");
      close(server_socket_fd);
      return EXIT_FAILURE;
    }

    while (auto line = recv_line(client_fd, app->buffer)) {
      // NOTE: Get pointer to callback fn for given command
      auto callback_ptr = cmd_manager.get_callback(line.value());

      // NOTE: Check if ptr is valid
      if (callback_ptr == nullptr) {
        std::println(std::cerr, "Invalid function pointer for: {}",
                     line.value());
        send_str(client_fd, "ERR");
        continue;
      }

      // NOTE: Execute callback fn
      if (!(*callback_ptr)(client_fd, *app)) {
        break;
      }
    }

    // NOTE: Closing client connection
    close(client_fd);
  }

  // NOTE: Closing server socket
  close(server_socket_fd);

  return EXIT_SUCCESS;
}
