#include "test_example_functions.h"

void AddDocument(SearchServer &serv, int document_id, const std::string &document, DocumentStatus status, const std::vector<int> &ratings)
{
    serv.AddDocument(document_id, document, status, ratings);
}
