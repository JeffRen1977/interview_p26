#include "grid.h"

#include <cctype>
#include <cstring>
#include <sstream>
#include <stdexcept>

std::vector<std::string> parse_grid(const std::string& text) {
    std::vector<std::string> rows;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        if (line.find_first_not_of(" \t") == std::string::npos) continue;
        rows.push_back(line);
    }
    if (rows.empty()) throw std::invalid_argument("empty maze");
    const std::size_t width = rows.front().size();
    for (const std::string& row : rows) {
        if (row.size() != width) throw std::invalid_argument("maze is not rectangular");
    }
    return rows;
}

int key_bit(char ch) {
    return static_cast<int>(std::tolower(static_cast<unsigned char>(ch))) - 'a';
}

bool is_key(char ch) { return std::strchr(kKeys, ch) != nullptr && ch != '\0'; }
bool is_gate(char ch) { return std::strchr(kGates, ch) != nullptr && ch != '\0'; }
