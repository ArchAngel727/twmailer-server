#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

typedef struct {
  std::string from;
  std::string to;
  std::string subject;
  std::string message;
} Mail;

namespace DataManager {
bool init_dir(const fs::path &);

std::optional<Mail> get_mail_deserialized(const fs::path &, const std::string &,
                                          const std::string &);
std::optional<std::string> get_mail_unserialized(const fs::path &,
                                                 const std::string &,
                                                 const std::string &);
bool write_mail(const fs::path &, Mail &);
bool delete_mail(const fs::path &root, const std::string &user,
                 const std::string &mail);

std::optional<std::vector<std::string>> list_mail_for_user(const fs::path &,
                                                           const std::string &);
std::optional<std::vector<std::string>> list_users(const fs::path &);

std::string serialize_mail(const Mail &);
std::optional<Mail> deserialize_mail(const std::string &);
}; // namespace DataManager
