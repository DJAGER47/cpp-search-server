#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <iostream>
#include <numeric>

// #include "search_server.h"

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine()
{
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber()
{
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string &text)
{
    vector<string> words;
    string word;
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
    REMOVED,
};

class SearchServer
{
public:
    void SetStopWords(const string &text)
    {
        for (const string &word : SplitIntoWords(text))
        {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string &document, DocumentStatus status,
                     const vector<int> &ratings)
    {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string &word : words)
        {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    }

    vector<Document> FindTopDocuments(const string &raw_query, DocumentStatus status_query) const
    {
        return FindTopDocuments(raw_query, [status_query](int document_id, DocumentStatus status, int rating)
                                { return status == status_query; });
    }

    vector<Document> FindTopDocuments(const string &raw_query) const
    {
        return FindTopDocuments(raw_query, [](int document_id, DocumentStatus status, int rating)
                                { return status == DocumentStatus::ACTUAL; });
    }

    template <typename KeyMapper>
    vector<Document> FindTopDocuments(const string &raw_query, KeyMapper keymapper = NULL) const
    {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, keymapper);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document &lhs, const Document &rhs)
             {
                 if (abs(lhs.relevance - rhs.relevance) < 1e-6)
                 {
                     return lhs.rating > rhs.rating;
                 }
                 else
                 {
                     return lhs.relevance > rhs.relevance;
                 }
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT)
        {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    int GetDocumentCount() const
    {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string &raw_query,
                                                        int document_id) const
    {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string &word : query.plus_words)
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
        for (const string &word : query.minus_words)
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

private:
    struct DocumentData
    {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string &word) const
    {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string &text) const
    {
        vector<string> words;
        for (const string &word : SplitIntoWords(text))
        {
            if (!IsStopWord(word))
            {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int> &ratings)
    {
        if (ratings.empty())
        {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings)
        {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord
    {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const
    {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-')
        {
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
    }

    struct Query
    {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string &text) const
    {
        Query query;
        for (const string &word : SplitIntoWords(text))
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
    double ComputeWordInverseDocumentFreq(const string &word) const
    {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename KeyMapper>
    vector<Document> FindAllDocuments(const Query &query, KeyMapper keymapper) const
    {
        map<int, double> document_to_relevance;
        for (const string &word : query.plus_words)
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

        for (const string &word : query.minus_words)
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

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance)
        {
            matched_documents.push_back(
                {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
};

template <typename T, typename U>
void AssertEqualImpl(const T &t, const U &u, const string &t_str, const string &u_str, const string &file,
                     const string &func, unsigned line, const string &hint)
{
    if (t != u)
    {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty())
        {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string &expr_str, const string &file, const string &func, unsigned line,
                const string &hint)
{
    if (!value)
    {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty())
        {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename T>
void RunTestImpl(const T &func, const string &name)
{
    func();
    cerr << name << " OK" << endl;
}

#define RUN_TEST(func) RunTestImpl(func, #func)

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении
// документов Поддержка стоп-слов. Стоп-слова исключаются из текста документов.
void TestExcludeStopWordsFromAddedDocumentContent()
{
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document &doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Stop words must be excluded from documents"s);
    }
}

// Добавление документов. Добавленный документ должен находиться по поисковому
// запросу, который содержит слова из документа.
void TestAddDocFindDoc()
{
    {
        SearchServer server;
        ASSERT_EQUAL(server.GetDocumentCount(), 0);
        server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
        ASSERT_EQUAL(server.GetDocumentCount(), 1);
    }

    {
        SearchServer server;
        ASSERT_EQUAL(server.FindTopDocuments("белый кот и модный ошейник").size(), 0u);
        server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
        const auto found = server.FindTopDocuments("белый кот и модный ошейник");
        ASSERT_EQUAL(found.size(), 1u);
        ASSERT_EQUAL(found.at(0).id, 0);
    }
}

// Поддержка минус-слов. Документы, содержащие минус-слова поискового запроса,
// не должны включаться в результаты поиска.
void TestMinusWords()
{
    {
        SearchServer server;
        server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
        server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});

        const auto found_docs1 = server.FindTopDocuments("кот"s);
        ASSERT_EQUAL(found_docs1.size(), 2u);
        ASSERT_EQUAL(found_docs1.at(0).id, 1);
        ASSERT_EQUAL(found_docs1.at(1).id, 0);

        const auto found_docs2 = server.FindTopDocuments("кот -пушистый"s);
        ASSERT_EQUAL(found_docs2.size(), 1u);
        ASSERT_EQUAL(found_docs2.at(0).id, 0);

        const auto found_docs3 = server.FindTopDocuments("-пушистый"s);
        ASSERT_EQUAL(found_docs3.size(), 0u);
    }
}

// Матчинг документов. При матчинге документа по поисковому запросу должны быть
// возвращены все слова из поискового запроса, присутствующие в документе. Если
// есть соответствие хотя бы по одному минус-слову, должен возвращаться пустой
// список слов.
void TestMatch()
{
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::BANNED, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::IRRELEVANT, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::REMOVED, {9});

    const auto [matched_words1, status1] = search_server.MatchDocument("пушистый ухоженный кот"s, 0);
    const auto [matched_words2, status2] = search_server.MatchDocument("пушистый ухоженный кот"s, 1);
    const auto [matched_words3, status3] = search_server.MatchDocument("пушистый ухоженный кот"s, 2);
    const auto [matched_words4, status4] = search_server.MatchDocument("пушистый ухоженный кот"s, 3);

    ASSERT_EQUAL(matched_words1.size(), 1u);
    ASSERT_EQUAL(matched_words1.at(0), "кот"s);
    ASSERT(status1 == DocumentStatus::ACTUAL);

    ASSERT_EQUAL(matched_words2.size(), 2u);
    ASSERT_EQUAL(matched_words2.at(0), "кот"s);
    ASSERT_EQUAL(matched_words2.at(1), "пушистый"s);
    ASSERT(status2 == DocumentStatus::BANNED);

    ASSERT_EQUAL(matched_words3.size(), 1u);
    ASSERT_EQUAL(matched_words3.at(0), "ухоженный"s);
    ASSERT(status3 == DocumentStatus::IRRELEVANT);

    ASSERT_EQUAL(matched_words4.size(), 1u);
    ASSERT_EQUAL(matched_words4.at(0), "ухоженный"s);
    ASSERT(status4 == DocumentStatus::REMOVED);
}

// Сортировка найденных документов по релевантности. Возвращаемые при поиске
// документов результаты должны быть отсортированы в порядке убывания
// релевантности.
void TestSortByRelevance()
{
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, {9});

    const auto found_docs1 = search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::ACTUAL);
    ASSERT_EQUAL(found_docs1.size(), 2u);
    ASSERT(found_docs1.at(0).relevance > found_docs1.at(1).relevance);
}

// Вычисление рейтинга документов. Рейтинг добавленного документа равен среднему
// арифметическому оценок документа.
void TestRaiting()
{
    {
        SearchServer server;
        vector<int> _vec = {8, -3};
        server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, _vec);

        const auto found_docs1 = server.FindTopDocuments("кот"s);
        int rating = static_cast<int>(accumulate(_vec.begin(), _vec.end(), 0) / (_vec.size() ? _vec.size() : 1));
        ASSERT_EQUAL(found_docs1.at(0).rating, rating);
    }
    {
        SearchServer server;
        vector<int> _vec = {};
        server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, _vec);

        const auto found_docs1 = server.FindTopDocuments("кот"s);
        int rating = static_cast<int>(accumulate(_vec.begin(), _vec.end(), 0) / (_vec.size() ? _vec.size() : 1));
        ASSERT_EQUAL(found_docs1.at(0).rating, rating);
    }
    {
        SearchServer server;
        vector<int> _vec = {-9};
        server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, _vec);

        const auto found_docs1 = server.FindTopDocuments("кот"s);
        int rating = static_cast<int>(accumulate(_vec.begin(), _vec.end(), 0) / (_vec.size() ? _vec.size() : 1));
        ASSERT_EQUAL(found_docs1.at(0).rating, rating);
    }
}

// Фильтрация результатов поиска с использованием предиката, задаваемого
// пользователем.
void TestPredicate()
{

    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, {9});

    const auto found_docs1 = search_server.FindTopDocuments("пушистый ухоженный кот "s,
                                                            [](int document_id, DocumentStatus status, int rating)
                                                            { return document_id % 2 == 0; });
    ASSERT_EQUAL(found_docs1[0].id, 0);
    ASSERT_EQUAL(found_docs1[1].id, 2);

    ASSERT_EQUAL(found_docs1[0].rating, 2);
    ASSERT_EQUAL(found_docs1[1].rating, -1);
}

// Поиск документов, имеющих заданный статус.
void TestStatus()
{
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, {9});

    const auto found_docs1 = search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::ACTUAL);
    ASSERT_EQUAL(found_docs1.size(), 3u);

    const auto found_docs2 = search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED);
    ASSERT_EQUAL(found_docs2.size(), 1u);

    const auto found_docs3 = search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::IRRELEVANT);
    ASSERT_EQUAL(found_docs3.size(), 0u);

    const auto found_docs4 = search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::REMOVED);
    ASSERT_EQUAL(found_docs4.size(), 0u);
}

// Корректное вычисление релевантности найденных документов.
void TestCalcRelevance()
{
    SearchServer search_server;
    search_server.AddDocument(0, "белый кот модный ошейник"s, DocumentStatus::ACTUAL, {});
    search_server.AddDocument(1, "пушистый кот"s, DocumentStatus::ACTUAL, {});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {1});

    // IDF слова "кот". Оно встречается в двух документах из трёх: 0 и 1. 3 / 2 = 1,5.
    // Примени?те логарифм и полу?чите примерно 0,4055.

    // TF слова "кот" в документе 0. Всего слов в этом документе четыре,
    // из них кот — только одно. 1 / 4 = 0,25.

    // TF слова "кот" в документе 1. Всего слов в этом документе два,
    // из них кот — только одно. 1 / 2 = 0,5.

    const auto found_docs1 = search_server.FindTopDocuments("кот"s, DocumentStatus::ACTUAL);
    ASSERT_EQUAL(found_docs1.size(), 2u);

    // 0) IDF * TF = 0,4055 * 0,5  = 0,20275
    double relevance1 = log(3.0 / 2.0) * (1.0 / 2.0);
    ASSERT(abs(found_docs1[0].relevance - relevance1) < 10e-6);

    // 1) IDF * TF = 0,4055 * 0,25 = 0,101375
    double relevance2 = log(3.0 / 2.0) * (1.0 / 4.0);
    ASSERT(abs(found_docs1[1].relevance - relevance2) < 10e-6);
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer()
{
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddDocFindDoc);
    RUN_TEST(TestMinusWords);
    RUN_TEST(TestMatch);
    RUN_TEST(TestSortByRelevance);
    RUN_TEST(TestRaiting);
    RUN_TEST(TestPredicate);
    RUN_TEST(TestStatus);
    RUN_TEST(TestCalcRelevance);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main()
{
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}