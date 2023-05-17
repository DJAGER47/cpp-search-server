#include <stdexcept>
#include <algorithm>
#include <numeric>
#include <cmath>
#include "search_server.h"

///
/// public
///

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
        id_to_wordfreqs_[document_id][word] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    index2id_.insert(document_id);
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

std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(const std::string raw_query, int document_id) const
{
    if (document_id < 0)
        throw std::out_of_range("document_id must be positive");

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

std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(std::execution::sequenced_policy, const std::string raw_query, int document_id) const
{
    return MatchDocument(raw_query, document_id);
}

std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(std::execution::parallel_policy, const std::string raw_query, int document_id) const
{
    if (document_id < 0)
        throw std::out_of_range("document_id must be positive");

    const std::map<std::string, double> &words_freqs{id_to_wordfreqs_.at(document_id)};
    if (words_freqs.empty())
        return {std::vector<std::string>{}, documents_.at(document_id).status};

    const QueryPar query{ParseQueryPar(std::execution::par, raw_query)};

    auto checker = [&](const std::string &word)
    {
        return words_freqs.find(word) != words_freqs.end();
    };

    if (any_of(std::execution::par, query.minus_words.begin(), query.minus_words.end(), checker))
        return {std::vector<std::string>{}, documents_.at(document_id).status};

    std::vector<std::string> matched_words(query.plus_words.size());
    auto words_end = copy_if(std::execution::par, query.plus_words.begin(), query.plus_words.end(), matched_words.begin(), checker);

    std::sort(std::execution::par, matched_words.begin(), words_end);
    words_end = unique(std::execution::par, matched_words.begin(), words_end);
    matched_words.resize(std::distance(matched_words.begin(), words_end));

    return {matched_words, documents_.at(document_id).status};
}

const std::map<std::string, double> &SearchServer::GetWordFrequencies(int document_id) const
{
    if (!id_to_wordfreqs_.count(document_id))
    {
        static const std::map<std::string, double> ret;
        return ret;
    }

    return id_to_wordfreqs_.at(document_id);
}

void SearchServer::RemoveDocument(int document_id)
{
    RemoveDocument(std::execution::seq, document_id);
}

///
/// private
///

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
