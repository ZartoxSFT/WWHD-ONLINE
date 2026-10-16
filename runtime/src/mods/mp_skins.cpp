#include "mp_skins.h"

#include <filesystem>
#include <memory>
#include <mutex>

#include "../runtime.h"

namespace mp_skins {
namespace {
std::mutex g_mutex;
std::shared_ptr<const std::string> g_directory;

char lower(char c) { return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c; }
}  // namespace

void set_directory(const std::string& data_dir) {
    auto next = data_dir.empty() ? nullptr : std::make_shared<const std::string>(data_dir);
    std::lock_guard guard(g_mutex);
    g_directory = next;
}

std::string skin_id(const std::string& guest) {
    static const std::string kFolder = "object/mpsk_", kExtension = ".szs";
    std::string path;
    path.reserve(guest.size());
    for (char c : guest) path += c == '\\' ? '/' : lower(c);
    auto slash = path.rfind('/');
    if (slash == std::string::npos) return {};
    auto folder = path.rfind('/', slash - 1);
    std::string tail = path.substr(folder == std::string::npos ? 0 : folder + 1);
    if (tail.size() <= kFolder.size() + kExtension.size() || tail.compare(0, kFolder.size(), kFolder) != 0 ||
        tail.compare(tail.size() - kExtension.size(), kExtension.size(), kExtension) != 0)
        return {};
    std::string id = tail.substr(kFolder.size(), tail.size() - kFolder.size() - kExtension.size());
    if (id.empty() || id.size() > 12) return {};
    for (char c : id)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return {};
    return id;
}

std::string archive_for(const std::string& guest) {
    std::shared_ptr<const std::string> directory;
    {
        std::lock_guard guard(g_mutex);
        directory = g_directory;
    }
    if (!directory) return {};
    auto id = skin_id(guest);
    if (id.empty()) return {};
    auto file = std::filesystem::path(*directory) / ("skin_" + id + ".szs");
    std::error_code ec;
    if (!std::filesystem::is_regular_file(file, ec)) return {};
    LOG("[multiplayer] skin %s is read from %s", id.c_str(), file.string().c_str());
    return file.string();
}

}  // namespace mp_skins
