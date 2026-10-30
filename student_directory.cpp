#include "student_directory.hpp"

#include <fstream>
#include <string>
#include <utility>

bool StudentDirectory::load(const std::string& path) {
    std::ifstream file(path);
    if (!file) return false;

    std::unordered_map<std::string, std::string> next;
    std::string id, email;
    while (file >> id >> email) {
        next[id] = email;
    }
    if (!file.eof()) return false;

    students_ = std::move(next);
    return true;
}

std::optional<std::string> StudentDirectory::lookup(const std::string& id) const {
    const auto it = students_.find(id);
    if (it == students_.end()) return std::nullopt;
    return it->second;
}
