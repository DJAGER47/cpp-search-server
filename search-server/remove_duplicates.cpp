#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer &search_server)
{
    std::set<int> duplicates;
    for (auto i = search_server.begin(); i != search_server.end(); ++i)
    {
        auto words1 = search_server.GetWordFrequencies(*i);
        for (auto j = i; j != search_server.end(); ++j)
        {
            if (i == j)
                continue;

            auto words2 = search_server.GetWordFrequencies(*j);
            if (words1.size() == words2.size() &&
                equal(words1.begin(), words1.end(), words2.begin(),
                      [](auto p1, auto p2)
                      { return p1.first == p2.first; }))
            {
                duplicates.insert(*j);
            }
        }
    }
    for (auto id : duplicates)
    {
        std::cout << "Found duplicate document id " << id << std::endl;
        search_server.RemoveDocument(id);
    }
}
