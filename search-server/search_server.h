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
    template <typename StringContainer>
    explicit SearchServer(const StringContainer &stop_words);
    explicit SearchServer(std::string stop_words) : SearchServer(SplitIntoWords(stop_words))
    {
    }
    explicit SearchServer(std::string_view stop_words) : SearchServer(SplitIntoWords(stop_words))
    {
    }

    /// @brief Добавить документ в поисковую базу
    void AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int> &ratings);

    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentStatus status_query) const;

    std::vector<Document> FindTopDocuments(const std::string_view raw_query) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const;

    int GetDocumentCount() const { return static_cast<int>(documents_.size()); }
    auto begin() { return index2id_.begin(); }
    auto end() { return index2id_.end(); }

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::sequenced_policy, const std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::parallel_policy, const std::string_view raw_query, int document_id) const;

    /// @brief Метод получения частот слов по id документа
    /// @param document_id
    /// @return Если документа не существует, возвратите ссылку на пустой map
    const std::map<std::string_view, double> &GetWordFrequencies(int document_id) const;

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

    std::set<std::string, std::less<>> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> id_to_wordfreqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> index2id_;
    std::set<std::string, std::less<>> unique_words_; // Храним слова

    /// @brief Наличие спецсимволов — то есть символов с кодами в диапазоне от 0 до 31 включительно — в тексте документов и поискового запроса.
    static bool IsValidWord(const std::string_view word);

    bool IsStopWord(const std::string_view word) const { return stop_words_.count(word) > 0; }

    std::vector<std::string> SplitIntoWordsNoStop(const std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int> &ratings);

    struct QueryWord
    {
        std::string data;
        bool is_minus;
        bool is_stop;
    };
    QueryWord ParseQueryWord(std::string text) const;

    struct QueryWordView
    {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };
    QueryWordView ParseQueryWord(std::string_view text) const;

    struct Query
    {
        std::set<std::string_view, std::less<>> plus_words;
        std::set<std::string_view, std::less<>> minus_words;
    };

    struct QueryPar
    {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    Query ParseQuery(const std::string_view text) const;
    QueryPar ParseQueryPar(const std::string_view text) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(const std::string_view word) const;

    template <typename KeyMapper>
    std::vector<Document> FindAllDocuments(const Query &query, KeyMapper keymapper) const;
};

///
/// public
///

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer &stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words))
{
    if (!all_of(stop_words_.cbegin(), stop_words_.cend(),
                [](const std::string &word)
                {
                    return IsValidWord(word);
                }))
    {
        throw std::invalid_argument{"Contains invalid characters in stop words"};
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const
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
    const std::map<std::string_view, double> &words_freqs{id_to_wordfreqs_.at(document_id)};

    if (!words_freqs.empty())
    {
        std::vector<std::string_view> words{words_freqs.size()};

        transform(policy,
                  words_freqs.begin(), words_freqs.end(),
                  words.begin(),
                  [](const auto &wf)
                  { return wf.first; });

        for_each(policy, words.begin(), words.end(),
                 [this, document_id](const std::string_view &item)
                 { word_to_document_freqs_[item].erase(document_id); });
    }

    documents_.erase(document_id);
    index2id_.erase(document_id);
    id_to_wordfreqs_.erase(document_id);
}

///
/// private
///

template <typename KeyMapper>
std::vector<Document> SearchServer::FindAllDocuments(const Query &query, KeyMapper keymapper) const
{
    std::map<int, double> document_to_relevance;
    for (const std::string_view &word : query.plus_words)
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

    for (const std::string_view &word : query.minus_words)
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
