#pragma once
#include <Geode/Geode.hpp>
#include <algorithm>
#include <map>
#include <regex>
#include <string>
#include <unordered_set>
#include <vector>

using namespace geode::prelude;

// XDBot Layout Mode depends on Utils::splitByChar. Keep the same semantics as
// the pinned upstream helper, including empty fields and a trailing empty field.
namespace Utils {
inline std::vector<std::string> splitByChar(std::string const& input, char delimiter) {
    std::vector<std::string> parts;
    parts.reserve(static_cast<size_t>(std::count(input.begin(), input.end(), delimiter)) + 1u);

    size_t start = 0;
    for (;;) {
        auto end = input.find(delimiter, start);
        if (end == std::string::npos) {
            parts.emplace_back(input.substr(start));
            break;
        }
        parts.emplace_back(input.substr(start, end - start));
        start = end + 1;
    }
    return parts;
}
}
