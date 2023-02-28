#include <stdexcept>
#include <algorithm>
#include <numeric>
#include <cmath>
#include "search_server.h"
#include "string_processing.h"

SearchServer::SearchServer(const std::string &text)
{
    if (!IsValidWord(text))
        throw std::invalid_argument("Stop-words invalid char");

    auto trim_text = regex_replace(text, std::regex("^ +| +$|( ) +"), "$1");
    for (const std::string &word : SplitIntoWords(trim_text))
    {
        stop_words_.insert(word);
    }
}

/// @brief Добавить документ в поисковую базу
void SearchServer::AddDocument(int document_id, const std::string &document, DocumentStatus status, const std::vector<int> &ratings)
{
    // Попытка добавить документ с отрицательным id. Или уже такой id есть
    if (document_id < 0)
        throw std::invalid_argument("document_id must be positive");

    if (documents_.count(document_id))
        throw std::invalid_argument("document_id already exists");

    const std::vector<std::string> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (const std::string &word : words)
    {
        word_to_document_freqs_[word][document_id] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    index2id.push_back(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string &raw_query, DocumentStatus status_query) const
{
    return FindTopDocuments(
        raw_query, [status_query](int document_id, DocumentStatus status, int rating)
        { return status == status_query; });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string &raw_query) const
{
    return FindTopDocuments(
        raw_query, [](int document_id, DocumentStatus status, int rating)
        { return status == DocumentStatus::ACTUAL; });
}

std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(const std::string &raw_query, int document_id) const
{
    if (document_id < 0)
        throw std::invalid_argument("document_id must be positive");

    const Query query = ParseQuery(raw_query);
    std::vector<std::string> matched_words;
    for (const std::string &word : query.plus_words)
    {
        if (word_to_document_freqs_.count(word) == 0)
        {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id))
        {
            matched_words.push_back(word);
        }
    }
    for (const std::string &word : query.minus_words)
    {
        if (word_to_document_freqs_.count(word) == 0)
        {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id))
        {
            matched_words.clear();
            break;
        }
    }

    return {matched_words, documents_.at(document_id).status};
}

/// @brief Наличие спецсимволов — то есть символов с кодами в диапазоне от 0 до 31 включительно — в тексте документов и поискового запроса.
bool SearchServer::IsValidWord(const std::string &word)
{
    // A valid word must not contain special characters
    return std::none_of(word.begin(), word.end(), [](char c)
                        { return c >= '\0' && c < ' '; });
}

std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string &text) const
{
    std::vector<std::string> words;
    for (const std::string &word : SplitIntoWords(text))
    {
        if (!IsValidWord(word))
            throw std::invalid_argument("document invalid char");

        if (!IsStopWord(word))
        {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int> &ratings)
{
    if (ratings.empty())
    {
        return 0;
    }

    int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string text) const
{
    if (text.empty())
        throw std::invalid_argument("word is empty");

    if (!IsValidWord(text))
        throw std::invalid_argument("word invalid char");

    bool is_minus = false;
    // Word shouldn't be empty
    if (text[0] == '-')
    {
        is_minus = true;
        text = text.substr(1);

        if (text.empty())
            throw std::invalid_argument("minus-words is empty");

        if (text[0] == '-')
            throw std::invalid_argument("incorrect query minus-words");
    }
    return {text, is_minus, IsStopWord(text)};
}

SearchServer::Query SearchServer::ParseQuery(const std::string &text) const
{
    Query query;
    for (const std::string &word : SplitIntoWords(text))
    {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop)
        {
            if (query_word.is_minus)
            {
                query.minus_words.insert(query_word.data);
            }
            else
            {
                query.plus_words.insert(query_word.data);
            }
        }
    }
    return query;
}

// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(const std::string &word) const
{
    return std::log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}
