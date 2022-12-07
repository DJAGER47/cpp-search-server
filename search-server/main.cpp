#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <string>
#include <utility>
#include <vector>

const int MAX_RESULT_DOCUMENT_COUNT = 5;

std::string ReadLine()
{
    std::string s;
    std::getline(cin, s);
    return s;
}

int ReadLineWithNumber()
{
    int result = 0;
    std::cin >> result;
    ReadLine();
    return result;
}

std::vector<string> SplitIntoWords(const std::string &text)
{
    std::vector<string> words;
    std::string word;
    for (const char c : text)
    {
        if (c == ' ')
        {
            if (!word.empty())
            {
                words.push_back(word);
                word.clear();
            }
        }
        else
        {
            word += c;
        }
    }
    if (!word.empty())
    {
        words.push_back(word);
    }

    return words;
}

struct Document
{
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus
{
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED
};

class SearchServer
{
private:
    struct Query
    {
        std::set<std::string> plus;
        std::set<std::string> minus;
    };

    struct DocumentData
    {
        int rating;
        DocumentStatus status;
    };

    std::set<std::string> stop_words_;
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> document_data_;
    size_t document_count_ = 0;

public:
    void SetStopWords(const std::string &text)
    {
        for (const std::string &word : SplitIntoWords(text))
        {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const std::string &document, DocumentStatus status, const std::vector<int> &rating)
    {
        auto words = SplitIntoWordsNoStop(document);
        double TF = 1.0 / words.size();
        for (const auto &word : words)
        {
            word_to_document_freqs_[word][document_id] += TF;
        }
        document_count_++;
        document_data_[document_id].rating = ComputeAverageRating(rating);
        document_data_[document_id].status = status;
    }

    std::vector<Document> FindTopDocuments(const std::string &raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const
    {
        const Query query_words = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query_words, status);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document &lhs, const Document &rhs)
             { return lhs.relevance > rhs.relevance; });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT)
        {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

private:
    bool IsStopWord(const std::string &word) const
    {
        return stop_words_.count(word) > 0;
    }

    std::vector<std::string> SplitIntoWordsNoStop(const std::string &text) const
    {
        std::vector<std::string> words;
        for (const std::string &word : SplitIntoWords(text))
        {
            if (!IsStopWord(word))
            {
                words.push_back(word);
            }
        }
        return words;
    }

    Query ParseQuery(const std::string &text) const
    {
        Query query_words;
        for (const std::string &word : SplitIntoWordsNoStop(text))
        {
            if (word[0] == '-')
            {
                query_words.minus.insert(word.substr(1));
            }
            else
            {
                query_words.plus.insert(word);
            }
        }
        return query_words;
    }

    static double calculateIDF(size_t document_count, size_t word_freqs)
    {
        return log(static_cast<double>(document_count) / word_freqs);
    }

    std::vector<Document> FindAllDocuments(const Query &query_words, DocumentStatus status) const
    {
        std::vector<Document> matched_documents;
        std::map<int, double> document_to_relevance;

        for (const auto &word : query_words.plus)
        {
            if (word_to_document_freqs_.count(word) != 0)
            {
                double IDF = calculateIDF(document_count_, word_to_document_freqs_.at(word).size());
                for (const auto &[id, TF] : word_to_document_freqs_.at(word))
                {
                    if (status == document_data_.at(id).status)
                    {
                        document_to_relevance[id] += TF * IDF;
                    }
                }
            }
        }
        for (const auto &word : query_words.minus)
        {
            if (word_to_document_freqs_.count(word) != 0)
            {
                for (const auto &[id, TF] : word_to_document_freqs_.at(word))
                {
                    document_to_relevance.erase(id);
                }
            }
        }

        for (const auto [id, relevance] : document_to_relevance)
        {
            matched_documents.push_back({id, relevance, document_data_.at(id).rating});
        }

        return matched_documents;
    }

    static int ComputeAverageRating(const std::vector<int> &ratings)
    {
        return (accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size()));
    }
};

void PrintDocument(const Document &document)
{
    cout << "{ "
         << "document_id = " << document.id << ", "
         << "relevance = " << document.relevance << ", "
         << "rating = " << document.rating << " }" << endl;
}

int main()
{
    SearchServer search_server;
    search_server.SetStopWords("и в на");
    search_server.AddDocument(0, "белый кот и модный ошейник", DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост", DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза", DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений", DocumentStatus::BANNED, {9});
    cout << "ACTUAL:" << endl;
    for (const Document &document : search_server.FindTopDocuments("пушистый ухоженный кот", DocumentStatus::ACTUAL))
    {
        PrintDocument(document);
    }
    cout << "BANNED:" << endl;
    for (const Document &document : search_server.FindTopDocuments("пушистый ухоженный кот", DocumentStatus::BANNED))
    {
        PrintDocument(document);
    }
    return 0;
}