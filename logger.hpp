#pragma once
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

class Logger {
public:
    static void info(const std::string& event, int client_id = 0,
                     const std::string& detail = "") {
        write("INFO", event, client_id, detail);
    }

    static void warn(const std::string& event, int client_id = 0,
                     const std::string& detail = "") {
        write("WARN", event, client_id, detail);
    }

private:
    static std::string escape(const std::string& input) {
        std::string out;
        for (unsigned char ch : input) {
            switch (ch) {
                case '\\': out += "\\\\"; break;
                case '"':  out += "\\\""; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (ch < 0x20) {
                        const char* digits = "0123456789abcdef";
                        out += "\\u00";
                        out += digits[ch >> 4];
                        out += digits[ch & 15];
                    } else out += static_cast<char>(ch);
            }
        }
        return out;
    }

    static void write(const char* level, const std::string& event,
                      int client_id, const std::string& detail) {
        using namespace std::chrono;
        const auto now = system_clock::now();
        const auto secs = system_clock::to_time_t(now);
        const auto millis = duration_cast<milliseconds>(
            now.time_since_epoch()).count() % 1000;
        std::tm utc{};
        gmtime_r(&secs, &utc);
        std::ostringstream row;
        row << "{\"timestamp\":\"" << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << millis
            << "Z\",\"level\":\"" << level
            << "\",\"event\":\"" << escape(event)
            << "\",\"client_id\":" << client_id;
        if (!detail.empty()) row << ",\"detail\":\"" << escape(detail) << '"';
        row << "}\n";
        std::lock_guard<std::mutex> guard(mutex_);
        std::cerr << row.str() << std::flush;
    }

    inline static std::mutex mutex_{};
};
