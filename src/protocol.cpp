#include "../include/protocol.hpp"

namespace publisher::protocol {

    std::vector<std::string> SplitTab(std::string_view line) {
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
            line.remove_suffix(1);
        }

        std::vector<std::string> out;
        size_t start = 0;

        for (size_t i = 0; i <= line.size(); ++i) {
            if (i == line.size() || line[i] == '\t') {
                out.emplace_back(line.substr(start, i - start));
                start = i + 1;
            }
        }

        return out;
    }

    std::string JoinTab(const std::vector<std::string>& fields) {
        std::string out;

        for (size_t i = 0; i < fields.size(); ++i) {
            if (i > 0) {
                out += '\t';
            }

            out += fields[i];
        }

        return out;
    }

}  // namespace publisher::protocol