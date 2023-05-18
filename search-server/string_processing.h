#pragma once
#include <string>
#include <set>
#include <vector>

std::vector<std::string> SplitIntoWords(const std::string_view text);
std::vector<std::string_view> SplitIntoWordsView(const std::string_view str);

template <typename StringContainer>
std::set<std::string, std::less<>> MakeUniqueNonEmptyStrings(const StringContainer &strings)
{
    std::set<std::string, std::less<>> non_empty_strings;
    for (const std::string &str : strings)
    {
        if (!str.empty())
        {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}
