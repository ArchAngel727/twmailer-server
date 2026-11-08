#include "../headers/data_manager.hpp"
#include "../headers/helper_fn.hpp"
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <iostream>
#include <iterator>
#include <optional>
#include <print>
#include <ranges>
#include <string>
#include <system_error>
#include <vector>

namespace DataManager {
std::string create_filename() {
  auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count();

  return std::format("{:20}", ns);
}

bool check_if_dir_exists(const fs::path &path) {
  std::error_code ec;
  bool exists = fs::exists(path, ec);

  if (ec) {
    std::println(std::cerr, "Failed while getting user mails {}: {}",
                 path.string(), ec.message());
    return false;
  }

  if (!exists) {
    fs::create_directories(path, ec);

    if (ec) {
      std::println(std::cerr, "Failed to create dir {}: {}", path.string(),
                   ec.message());
      return false;
    }
  }

  return true;
}

std::string serialize_mail(const Mail &mail) {
  std::string str;

  str.append(mail.from);
  str.push_back('\n');
  str.append(mail.to);
  str.push_back('\n');
  str.append(mail.subject);
  str.push_back('\n');
  str.append(mail.message);
  str.push_back('\n');

  return str;
}

std::optional<Mail> deserialize_mail(const std::string &str) {
  auto lines = split_string(str);

  if (lines.size() < 4) {
    return std::nullopt;
  }

  auto message = lines | std::views::drop(3) | std::views::join_with('\n') |
                 std::ranges::to<std::string>();

  return Mail{
      .from = lines[0],
      .to = lines[1],
      .subject = lines[2],
      .message = message,
  };
}

bool init_dir(const fs::path &path) {
  std::error_code ec;

  fs::create_directory(path, ec);

  if (ec) {
    std::println(std::cerr, "Failed to created directory {}: {}", path.string(),
                 ec.message());
    return false;
  }

  return true;
}

std::optional<Mail> get_mail(const fs::path &root, const std::string &user,
                             const std::string &filename) {
  fs::path path = root / user / filename;
  std::ifstream in(path, std::ios::binary);

  if (!in) {
    return std::nullopt;
  }

  std::string data(std::istream_iterator<char>(in), {});

  if (in.bad()) {
    return std::nullopt;
  }

  return deserialize_mail(data);
}

bool write_mail(const fs::path &root, Mail &mail) {
  std::string filename = create_filename();
  std::string str = serialize_mail(mail);
  std::error_code ec;

  fs::path dir = root / mail.to;

  fs::create_directory(dir, ec);

  if (ec) {
    return false;
  }

  fs::path file = dir / filename;

  std::ofstream out(file, std::ios::binary);

  if (!out) {
    return false;
  }

  std::println(out, "{}", str);

  return true;
}

bool delete_mail(const fs::path &root, const std::string &user,
                 const std::string &mail) {
  fs::path dir = root / user;

  if (!fs::exists(dir)) {
    return true;
  }

  fs::path file = dir / mail;

  if (!fs::exists(file)) {
    return true;
  }

  std::error_code ec;

  bool removed = fs::remove(file, ec);

  if (ec) {
    std::println(std::cerr, "Failed to delete file {}: {}", file.string(),
                 ec.message());
    return false;
  }

  return removed;
}

std::optional<std::vector<std::string>>
list_mail_for_user(const fs::path &root, const std::string &user) {
  std::vector<std::string> mails;
  auto path = root;
  path.append(user);

  if (!check_if_dir_exists(path)) {
    return std::nullopt;
  }

  for (const auto &entry : fs::directory_iterator(path)) {
    mails.emplace_back(entry.path().filename().string());
  }

  return mails;
}

std::optional<std::vector<std::string>> list_users(const fs::path &root) {
  std::vector<std::string> users;

  if (!check_if_dir_exists(root)) {
    return std::nullopt;
  }

  for (const auto &entry : fs::directory_iterator(root)) {
    users.emplace_back(entry.path().filename().string());
  }

  return users;
}

}; // namespace DataManager
/*
  std::vector<Mail> mails;

  return mails;
  */
