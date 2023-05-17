#pragma once
#include <regex>
#include <string>
#include <set>
#include <map>
#include <vector>
#include <set>
#include "document.h"
#include <execution>
#include "string_processing.h"

class SearchServer
{
    const int MAX_RESULT_DOCUMENT_COUNT = 5;

public:
    explicit SearchServer() = default;
    explicit SearchServer(const std::string &text);
    template <typename T>
    SearchServer(const T &text);

    /// @brief Добавить документ в поисковую базу
    void AddDocument(int document_id, const std::string &document, DocumentStatus status, const std::vector<int> &ratings);

    std::vector<Document> FindTopDocuments(const std::string &raw_query, DocumentStatus status_query) const;

    std::vector<Document> FindTopDocuments(const std::string &raw_query) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string &raw_query, DocumentPredicate document_predicate) const;

    int GetDocumentCount() const { return static_cast<int>(documents_.size()); }
    auto begin() { return index2id_.begin(); }
    auto end() { return index2id_.end(); }

    using type = std::tuple<std::vector<std::string>, DocumentStatus>;
    type MatchDocument(const std::string raw_query, int document_id) const;
    type MatchDocument(std::execution::sequenced_policy, const std::string raw_query, int document_id) const;
    type MatchDocument(std::execution::parallel_policy, const std::string raw_query, int document_id) const;

    /// @brief Метод получения частот слов по id документа
    /// @param document_id
    /// @return Если документа не существует, возвратите ссылку на пустой map
    const std::map<std::string, double> &GetWordFrequencies(int document_id) const;

    /// @brief Метод удаления документов из поискового сервера
    /// @param document_id
    void RemoveDocument(int document_id);

    /// @brief Метод удаления документов из поискового сервера
    /// @param policy std::execution
    /// @param document_id
    template <typename ExecutionPolicy>
    void RemoveDocument(ExecutionPolicy policy, int document_id);

private:
    struct DocumentData
    {
        int rating;
        DocumentStatus status;
    };

    std::set<std::string> stop_words_;
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string, double>> id_to_wordfreqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> index2id_;

    /// @brief Наличие спецсимволов — то есть символов с кодами в диапазоне от 0 до 31 включительно — в тексте документов и поискового запроса.
    static bool IsValidWord(const std::string &word);

    bool IsStopWord(const std::string &word) const { return stop_words_.count(word) > 0; }

    std::vector<std::string> SplitIntoWordsNoStop(const std::string &text) const;

    static int ComputeAverageRating(const std::vector<int> &ratings);

    struct QueryWord
    {
        std::string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string text) const;

    struct Query
    {
        std::set<std::string> plus_words;
        std::set<std::string> minus_words;
    };

    struct QueryPar
    {
        std::vector<std::string> plus_words;
        std::vector<std::string> minus_words;
    };

    Query ParseQuery(const std::string &text) const;

    template <typename ExecutionPolicy>
    SearchServer::QueryPar ParseQueryPar(ExecutionPolicy policy, const std::string &text) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(const std::string &word) const;

    template <typename KeyMapper>
    std::vector<Document> FindAllDocuments(const Query &query, KeyMapper keymapper) const;
};

///
/// public
///

template <typename T>
SearchServer::SearchServer(const T &text)
{
    for (const std::string &word : text)
    {
        if (!IsValidWord(word))
            throw std::invalid_argument("Stop-words invalid char");

        if (word.length() && (stop_words_.count(word) == 0))
            stop_words_.insert(word);
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string &raw_query, DocumentPredicate document_predicate) const
{
    const double calculation_accuracy = 1e-6;
    const Query query = ParseQuery(raw_query);
    std::vector<Document> result = FindAllDocuments(query, document_predicate);
    std::sort(result.begin(), result.end(),
              [calculation_accuracy](const Document &lhs, const Document &rhs)
              {
                  if (std::abs(lhs.relevance - rhs.relevance) < calculation_accuracy)
                  {
                      return lhs.rating > rhs.rating;
                  }
                  else
                  {
                      return lhs.relevance > rhs.relevance;
                  }
              });
    if (result.size() > MAX_RESULT_DOCUMENT_COUNT)
    {
        result.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return result;
}

template <typename ExecutionPolicy>
void SearchServer::RemoveDocument(ExecutionPolicy policy, int document_id)
{
    const std::map<std::string, double> &words_freqs{
        id_to_wordfreqs_.at(document_id)};

    if (!words_freqs.empty())
    {
        std::vector<std::string> words{words_freqs.size()};

        transform(policy,
                  words_freqs.begin(), words_freqs.end(),
                  words.begin(),
                  [](const auto &wf)
                  { return wf.first; });

        for_each(policy, words.begin(), words.end(),
                 [this, document_id](const std::string &item)
                 { word_to_document_freqs_[item].erase(document_id); });
    }

    documents_.erase(document_id);
    index2id_.erase(document_id);
    id_to_wordfreqs_.erase(document_id);
}

///
/// private
///

template <typename ExecutionPolicy>
SearchServer::QueryPar SearchServer::ParseQueryPar(ExecutionPolicy policy, const std::string &text) const
{
    QueryPar query;
    for (const std::string &word : SplitIntoWords(text))
    {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop)
        {
            if (query_word.is_minus)
            {
                query.minus_words.push_back(query_word.data);
            }
            else
            {
                query.plus_words.push_back(query_word.data);
            }
        }
    }

    // std::sort(policy,query.minus_words.begin(), query.minus_words.end());
    // auto new_end_plus = std::unique(policy, query.minus_words.begin(), query.minus_words.end());
    // query.minus_words.erase(new_end_plus, query.minus_words.end());

    // std::sort(policy, query.plus_words.begin(), query.plus_words.end());
    // new_end_plus = std::unique(policy, query.plus_words.begin(), query.plus_words.end());
    // query.plus_words.erase(new_end_plus, query.plus_words.end());

    return query;
}

template <typename KeyMapper>
std::vector<Document> SearchServer::FindAllDocuments(const Query &query, KeyMapper keymapper) const
{
    std::map<int, double> document_to_relevance;
    for (const std::string &word : query.plus_words)
    {
        if (word_to_document_freqs_.count(word) == 0)
        {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word))
        {
            // int document_id, DocumentStatus status, int rating
            if (keymapper(document_id, documents_.at(document_id).status, documents_.at(document_id).rating))
            {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (const std::string &word : query.minus_words)
    {
        if (word_to_document_freqs_.count(word) == 0)
        {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word))
        {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance)
    {
        matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
    }
    return matched_documents;
}
