#include "remove_duplicates.h"
#include <iterator>

template <typename Key, typename Value>
std::set<Key> ExtractKeysFromMap(const std::map<Key, Value> &container)
{
    std::set<Key> out;

    std::transform(container.begin(), container.end(),
                   std::insert_iterator(out, out.begin()),
                   [](const auto &key_value)
                   {
                       return key_value.first;
                   });

    return out;
}

void RemoveDuplicates(SearchServer &search_server)
{
    std::set<int> duplicate_for_remove;

    std::set<std::set<std::string_view>> verified;

    for (const int document_id : search_server)
    {
        std::set<std::string_view> wordsOfDocument = ExtractKeysFromMap(
            search_server.GetWordFrequencies(
                document_id));

        if (0 == verified.count(wordsOfDocument))
        {
            verified.insert(wordsOfDocument);
            continue;
        }

        duplicate_for_remove.insert(document_id);
    }

    for (const int id : duplicate_for_remove)
    {
        search_server.RemoveDocument(id);
        std::cout << "Found duplicate document id " << id << "\n";
    }
}