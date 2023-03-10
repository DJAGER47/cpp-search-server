#pragma once

#include <string>
#include <vector>
#include "search_server.h"

void AddDocument(SearchServer &serv, int document_id, const std::string &document, DocumentStatus status, const std::vector<int> &ratings);