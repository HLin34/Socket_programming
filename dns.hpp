#pragma once
#include <string>

// Resolves an IPv4 hostname and returns a human-readable response.
std::string lookup_dns(const std::string& hostname);
