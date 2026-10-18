#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace publisher::protocol {

    std::vector<std::string> SplitTab(std::string_view line);

    std::string JoinTab(const std::vector<std::string>& fields);
    
}