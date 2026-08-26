#include "optimizer_utils.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {

std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

bool is_operator(const std::string& token) {
    return token == "+" || token == "-" || token == "*" || token == "/";
}

}  // namespace

Case load_case(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::invalid_argument("cannot open case file: " + path);
    }

    Case result;
    std::map<std::string, int> expected;
    std::string section;

    std::string raw;
    while (std::getline(in, raw)) {
        std::string line = trim(raw);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            section = trim(line.substr(1, line.size() - 2));
            std::transform(section.begin(), section.end(), section.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            continue;
        }

        if (section == "costs") {
            const auto pos = line.find('=');
            if (pos == std::string::npos) {
                throw std::invalid_argument("malformed cost entry: " + line);
            }
            result.costs[line.substr(0, pos)] = std::stoi(line.substr(pos + 1));
        } else if (section == "expected") {
            const auto pos = line.find('=');
            if (pos == std::string::npos) {
                throw std::invalid_argument("malformed expected entry: " + line);
            }
            expected[trim(line.substr(0, pos))] = std::stoi(line.substr(pos + 1));
        } else if (section == "program") {
            result.program.push_back(line);
        } else {
            throw std::invalid_argument("line outside any section: " + line);
        }
    }

    if (expected.count("time") == 0 || expected.count("memory") == 0) {
        throw std::invalid_argument(path + ": [expected] needs both time and memory");
    }
    result.expected_time = expected.at("time");
    result.expected_memory = expected.at("memory");

    const std::filesystem::path fs_path(path);
    result.name = fs_path.stem().string();
    return result;
}

std::vector<Case> load_all_cases(const std::string& directory) {
    std::vector<std::string> paths;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        const std::string name = entry.path().filename().string();
        if (name.rfind("case_", 0) == 0 && entry.path().extension() == ".txt") {
            paths.push_back(entry.path().string());
        }
    }
    std::sort(paths.begin(), paths.end());

    std::vector<Case> cases;
    cases.reserve(paths.size());
    for (const auto& path : paths) {
        cases.push_back(load_case(path));
    }
    return cases;
}

// ---------------------------------------------------------------------------
// Given and correct -- do not change the rest of this file.
// ---------------------------------------------------------------------------
bool is_literal(const std::string& token) {
    if (token.empty()) return false;
    std::size_t i = (token[0] == '-') ? 1 : 0;
    if (i >= token.size()) return false;
    for (; i < token.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(token[i]))) return false;
    }
    return true;
}

std::vector<Instr> parse_program(const std::vector<std::string>& lines) {
    std::vector<Instr> out;
    out.reserve(lines.size());
    for (const auto& line : lines) {
        const auto pos = line.find('=');
        if (pos == std::string::npos) {
            throw std::invalid_argument("not an assignment: " + line);
        }
        const std::string target = trim(line.substr(0, pos));

        std::vector<std::string> tokens;
        std::istringstream rhs(line.substr(pos + 1));
        for (std::string token; rhs >> token;) {
            tokens.push_back(token);
        }

        if (tokens.size() == 1) {
            out.push_back(Instr{target, tokens[0], "", ""});
        } else if (tokens.size() == 3 && is_operator(tokens[1])) {
            out.push_back(Instr{target, tokens[0], tokens[1], tokens[2]});
        } else {
            throw std::invalid_argument("cannot parse instruction: " + line);
        }
    }
    return out;
}
