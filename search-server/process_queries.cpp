#include "process_queries.h"
#include <algorithm>
#include <execution>

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer &search_server,
    const std::vector<std::string> &queries)

{
    std::vector<std::vector<Document>> result(queries.size());
    std::transform(std::execution::par,
                   queries.begin(), queries.end(),
                   result.begin(),
                   [&search_server](const auto &query)
                   { return search_server.FindTopDocuments(query); });
    return result;
}

std::vector<Document> ProcessQueriesJoined(
    const SearchServer &search_server,
    const std::vector<std::string> &queries)
{
    return std::transform_reduce(
        std::execution::par,
        queries.begin(), queries.end(),
        std::vector<Document>(),
        [](std::vector<Document> lhs, std::vector<Document> rhs)
        {
            lhs.insert(lhs.end(), rhs.begin(), rhs.end());
            return lhs;
        },
        [&search_server](const auto &query)
        {
            return search_server.FindTopDocuments(query);
        });
}