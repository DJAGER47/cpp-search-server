#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer &search_server)
{
    std::set<int> duplicates;
    std::set<std::set<std::string>> docs;

    for (auto i = search_server.begin(); i != search_server.end(); ++i)
    {
        const auto &doc = search_server.GetWordFrequencies(*i);
        std::set<std::string> doc_words;

        for (const auto &[word, _] : doc)
        {
            doc_words.insert(word);
        }

        if (docs.count(doc_words))
        {
            duplicates.insert(*i);
        }
        else
        {
            docs.insert(doc_words);
        }
    }
    for (auto id : duplicates)
    {
        std::cout << "Found duplicate document id " << id << std::endl;
        search_server.RemoveDocument(id);
    }
}
