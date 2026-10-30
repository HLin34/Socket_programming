#pragma once

#include <optional>
#include <string>
#include <unordered_map>

// Stores a simple ID -> email lookup table loaded from a text file.
class StudentDirectory {
public:
    bool load(const std::string& path);
    std::optional<std::string> lookup(const std::string& id) const;

private:
    std::unordered_map<std::string, std::string> students_;
};
