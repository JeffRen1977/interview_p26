// Valid IP — pure pointer scan (no sscanf / regex).
// LeetCode-style: "IPv4" | "IPv6" | "Neither"
//
// IPv4: 4 dotted decimals, each 0..255, no leading zeros.
// IPv6 (full form): 8 colon-separated hex groups, each 1..4 digits.

#include <cassert>
#include <cctype>
#include <iostream>
#include <string>

namespace {

bool is_hex(char c) {
    return std::isdigit(static_cast<unsigned char>(c)) ||
           (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

bool valid_ipv4(const std::string& s) {
    const char* p = s.c_str();
    int parts = 0;

    while (true) {
        if (!std::isdigit(static_cast<unsigned char>(*p))) {
            return false;
        }

        int value = 0;
        int digits = 0;
        // Reject leading zeros: "0" ok, "01" / "00" not.
        if (*p == '0' && std::isdigit(static_cast<unsigned char>(*(p + 1)))) {
            return false;
        }

        while (std::isdigit(static_cast<unsigned char>(*p))) {
            value = value * 10 + (*p - '0');
            if (value > 255) {
                return false;
            }
            ++digits;
            ++p;
            if (digits > 3) {
                return false;
            }
        }

        if (digits == 0) {
            return false;
        }
        ++parts;

        if (*p == '\0') {
            return parts == 4;
        }
        if (*p != '.' || parts == 4) {
            return false;
        }
        ++p;  // skip '.'
    }
}

bool valid_ipv6(const std::string& s) {
    const char* p = s.c_str();
    int parts = 0;

    while (true) {
        int digits = 0;
        while (is_hex(*p)) {
            ++digits;
            ++p;
            if (digits > 4) {
                return false;
            }
        }
        if (digits == 0) {
            return false;  // no compressed "::" in this whiteboard version
        }
        ++parts;

        if (*p == '\0') {
            return parts == 8;
        }
        if (*p != ':' || parts == 8) {
            return false;
        }
        ++p;
    }
}

}  // namespace

std::string validIPAddress(const std::string& query) {
    if (valid_ipv4(query)) {
        return "IPv4";
    }
    if (valid_ipv6(query)) {
        return "IPv6";
    }
    return "Neither";
}

int main() {
    assert(validIPAddress("172.16.254.1") == "IPv4");
    assert(validIPAddress("192.168.01.1") == "Neither");  // leading zero
    assert(validIPAddress("256.1.1.1") == "Neither");
    assert(validIPAddress("1.1.1") == "Neither");
    assert(validIPAddress("2001:0db8:85a3:0000:0000:8a2e:0370:7334") ==
           "IPv6");
    assert(validIPAddress("2001:db8:85a3::8a2e:370:7334") == "Neither");
    assert(validIPAddress("hello") == "Neither");

    std::cout << "15_valid_ip: ok\n";
    return 0;
}
