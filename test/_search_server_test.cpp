#include <iostream>
#include <random>
#include <string>
#include <vector>
#include <cmath>
#include "..\search-server\src\search_server.h"

#include "..\search-server\src\process_queries.h"
#include "..\search-server\src\logduration.h"
#include "..\search-server\src\paginator.h"
#include "..\search-server\src\remove_duplicates.h"
#include "..\search-server\src\request_queue.h"
#include "test_runner.h"

using namespace std;

SearchServer GetSearchServer()
{
    SearchServer server(""s);
    server.AddDocument(0, "dog in the cat cat happy"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(10, "cat and cat and happy cat"s, DocumentStatus::ACTUAL, {5});
    server.AddDocument(24, "dog the city dog is full happy"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(13, "cat and cat and cat cat"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(43, "cat in cat and happy cat"s, DocumentStatus::ACTUAL, {1});
    return server;
}

SearchServer GetSearchServerDifferentDocsStatus()
{
    SearchServer server(""s);
    server.AddDocument(4, "dog in the cat cat happy"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(3, "cat and cat and happy cat"s, DocumentStatus::IRRELEVANT, {5});
    server.AddDocument(2, "dog the city dog is full happy"s, DocumentStatus::BANNED, {1});
    server.AddDocument(1, "cat and cat and cat cat"s, DocumentStatus::ACTUAL, {1});
    server.AddDocument(0, "cat in cat and happy cat"s, DocumentStatus::REMOVED, {1});
    return server;
}

#define FIND_DOC_WITH_STATUS(server, doc_status)                                   \
    server.FindTopDocuments("cat dog"s, [](int document_id, DocumentStatus status, \
                                           int rating) { return status == doc_status; })

void TestStopWordStringConstructor()
{
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

        const auto found_docs = server.FindTopDocuments("in"s);

        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, doc_id);
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

        ASSERT(server.FindTopDocuments("in"s).empty());
    }
}

void TestStopWordVectorConstructor()
{
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

    {
        const vector<string> stop_words_vector = {""s, ""s};
        SearchServer server(stop_words_vector);

        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

        const auto found_docs = server.FindTopDocuments("in"s);

        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, doc_id);
    }

    {
        const vector<string> stop_words_vector = {"in"s, "a"s, "the"s, ""s};
        SearchServer server(stop_words_vector);

        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

        ASSERT(server.FindTopDocuments("in"s).empty());
    }
}

void TestStopWordSetConstructor()
{
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

    {
        const set<string> stop_words_set = {""s, ""s};
        SearchServer server(stop_words_set);

        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

        const auto found_docs = server.FindTopDocuments("in"s);

        ASSERT_EQUAL(found_docs.size(), 1u);
        ASSERT_EQUAL(found_docs[0].id, doc_id);
    }

    {
        const set<string> stop_words_set = {"in"s, "the"s, ""s};
        SearchServer server(stop_words_set);

        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

        ASSERT(server.FindTopDocuments("in"s).empty());
    }
}

void TestStringConstructorWithSpecialCharacters()
{
    string exString{};

    try
    {
        SearchServer server("in the\x13 a"s);
    }
    catch (const exception &e)
    {
        ASSERT_EQUAL(typeid(e).name(), "class std::invalid_argument"s);
        exString = e.what();
    }

    ASSERT(!exString.empty());
}

void TestVectorConstructorWithSpecialCharacters()
{
    string exString{};
    const vector<string> stop_words_vector = {
        ""s,
        "in"s,
        "the\x12"s,
    };

    try
    {
        SearchServer server(stop_words_vector);
    }
    catch (const exception &e)
    {
        ASSERT_EQUAL(typeid(e).name(), "class std::invalid_argument"s);
        exString = e.what();
    }

    ASSERT(!exString.empty());
}

void TestSetConstructorWithSpecialCharacters()
{
    string exString{};
    const set<string> stop_words_set = {
        ""s,
        "in"s,
        "the\x13"s,
    };

    try
    {
        SearchServer server(stop_words_set);
    }
    catch (const exception &e)
    {
        ASSERT_EQUAL(typeid(e).name(), "class std::invalid_argument"s);
        exString = e.what();
    }

    ASSERT(!exString.empty());
}

void TestAddDocWithNegativeID()
{
    string exString{};
    SearchServer server("in the a"s);

    try
    {
        server.AddDocument(-1, "cat in the city"s, DocumentStatus::ACTUAL, {1});
    }
    catch (const exception &e)
    {
        ASSERT_EQUAL(typeid(e).name(), "class std::invalid_argument"s);
        exString = e.what();
    }

    ASSERT(!exString.empty());
}

void TestAddDocWithAddedID()
{
    string exString{};
    SearchServer server("in the a"s);

    try
    {
        server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, {1});
        server.AddDocument(1, "NY city"s, DocumentStatus::ACTUAL, {1});
    }
    catch (const exception &e)
    {
        ASSERT_EQUAL(typeid(e).name(), "class std::invalid_argument"s);
        exString = e.what();
    }

    ASSERT(!exString.empty());
}

void TestAddDocWithSpecialCharacters()
{
    string exString{};
    SearchServer server("in the a"s);

    try
    {
        server.AddDocument(1, "cat i\0n the city"s, DocumentStatus::ACTUAL, {1});
    }
    catch (const exception &e)
    {
        ASSERT_EQUAL(typeid(e).name(), "class std::invalid_argument"s);
        exString = e.what();
    }

    ASSERT(!exString.empty());
}

void TestSearchQueryWithSpecialCharacters()
{
    string exString{};
    SearchServer server("in the a"s);
    server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, {1});
    try
    {
        server.FindTopDocuments("ca\x10t"s);
    }
    catch (const exception &e)
    {
        ASSERT_EQUAL(typeid(e).name(), "class std::invalid_argument"s);
        exString = e.what();
    }
    ASSERT(!exString.empty());
}

void TestSearchQueryWithDoubleMinus()
{
    string exString{};
    SearchServer server("in the a"s);
    server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, {1});
    try
    {
        server.FindTopDocuments("cat --city"s);
    }
    catch (const exception &e)
    {
        ASSERT_EQUAL(typeid(e).name(), "class std::invalid_argument"s);
        exString = e.what();
    }
    ASSERT(!exString.empty());
}

void TestSearchQueryWithEmptyMinusWord()
{
    string exString{};
    SearchServer server("in the a"s);
    server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, {1});
    try
    {
        server.FindTopDocuments("cat -"s);
    }
    catch (const exception &e)
    {
        ASSERT_EQUAL(typeid(e).name(), "class std::invalid_argument"s);
        exString = e.what();
    }
    ASSERT(!exString.empty());
}

void TestExcludeDocumentsWithMinusWords()
{
    SearchServer server("in"s);
    server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, {1});
    ASSERT(server.FindTopDocuments("cat -city"s).empty());
}

void TestMatchDocumentNormalQuery()
{
    SearchServer server(""s);
    const vector<string_view> match{"cat"sv, "happy"sv};

    server.AddDocument(1, "cat in the city. cat is full and happy"s, DocumentStatus::ACTUAL,
                       {1});
    const auto [matched_words, _] = server.MatchDocument("happy cat"s, 1);

    ASSERT_EQUAL(matched_words, match);
}

void TestMatchDocumentQueryWithMinusWords()
{
    SearchServer server(""s);
    server.AddDocument(1, "cat in the city. cat is full and happy"s, DocumentStatus::ACTUAL,
                       {1});

    const auto [matched_words, _] = server.MatchDocument("-happy cat"s, 1);

    ASSERT(matched_words.empty());
}

void TestMatchDocumentQueryWithSpecialCharacters()
{
    string exString{};

    SearchServer server("in the a"s);
    server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, {1});

    try
    {
        server.MatchDocument("ca\x10t"s, 1);
    }
    catch (const exception &e)
    {
        ASSERT_EQUAL(typeid(e).name(), "class std::invalid_argument"s);
        exString = e.what();
    }

    ASSERT(!exString.empty());
}

void TestMatchDocumentQueryWithDoubleMinus()
{
    string exString{};

    SearchServer server("in the a"s);
    server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, {1});

    try
    {
        server.MatchDocument("cat --dog"s, 1);
    }
    catch (const exception &e)
    {
        ASSERT_EQUAL(typeid(e).name(), "class std::invalid_argument"s);
        exString = e.what();
    }

    ASSERT(!exString.empty());
}

void TestMatchDocumentQueryWithEmptyMinusWord()
{
    string exString{};

    SearchServer server("in the a"s);
    server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, {1});

    try
    {
        server.MatchDocument(" - cat"s, 1);
    }
    catch (const exception &e)
    {
        ASSERT_EQUAL(typeid(e).name(), "class std::invalid_argument"s);
        exString = e.what();
    }

    ASSERT(!exString.empty());
}

void TestSortFoundDocumentsToRelevance()
{
    SearchServer server = GetSearchServer();
    const auto found_docs = server.FindTopDocuments("cat"s);

    ASSERT_EQUAL(found_docs.size(), 4u);
    ASSERT_EQUAL(found_docs[0].id, 13);
    ASSERT_EQUAL(found_docs[1].id, 10);
    ASSERT_EQUAL(found_docs[2].id, 43);
    ASSERT_EQUAL(found_docs[3].id, 0);
}

void TestFoundDocumentsPlusRating()
{
    SearchServer server(""s);
    server.AddDocument(1, "cat in the city. cat is full and happy"s, DocumentStatus::ACTUAL,
                       {numeric_limits<int>::max() - 50, 20, 20, 10});

    const auto found_docs = server.FindTopDocuments("cat"s);

    ASSERT_EQUAL(found_docs.size(), 1u);
    ASSERT_EQUAL(found_docs[0].rating, numeric_limits<int>::max() / 4);
}

void TestFoundDocumentsMinusRating()
{
    SearchServer server(""s);
    server.AddDocument(1, "cat in the city. cat is full and happy"s, DocumentStatus::ACTUAL,
                       {numeric_limits<int>::min() + 5, -2, -3});

    const auto found_docs = server.FindTopDocuments("cat"s);

    ASSERT_EQUAL(found_docs.size(), 1u);
    ASSERT_EQUAL(found_docs[0].rating, numeric_limits<int>::min() / 3);
}

void TestUserFilterFoundDocuments()
{
    SearchServer server = GetSearchServer();

    auto IsEvenDocId = [](int document_id, DocumentStatus status, int rating)
    {
        return document_id % 2 == 0;
    };

    const auto &found_docs = server.FindTopDocuments("cat"s, IsEvenDocId);

    ASSERT_EQUAL(found_docs.size(), 2u);
    ASSERT_EQUAL(found_docs[0].id, 10);
    ASSERT_EQUAL(found_docs[1].id, 0);
}

void TestActualStatusFilterFoundDocuments()
{
    SearchServer server = GetSearchServerDifferentDocsStatus();
    {
        const auto &found_docs = server.FindTopDocuments("cat dog"s);

        ASSERT_EQUAL(found_docs.size(), 2u);
        ASSERT_EQUAL(found_docs[0].id, 4);
        ASSERT_EQUAL(found_docs[1].id, 1);
    }
    {
        const auto &found_docs = FIND_DOC_WITH_STATUS(server, DocumentStatus::ACTUAL);

        ASSERT_EQUAL(found_docs.size(), 2u);
        ASSERT_EQUAL(found_docs[0].id, 4);
        ASSERT_EQUAL(found_docs[1].id, 1);
    }
}

void TestIrrelevantStatusFilterFoundDocuments()
{
    SearchServer server = GetSearchServerDifferentDocsStatus();

    const auto &found_docs = FIND_DOC_WITH_STATUS(server, DocumentStatus::IRRELEVANT);

    ASSERT_EQUAL(found_docs.size(), 1u);
    ASSERT_EQUAL(found_docs[0].id, 3);
}

void TestBannedStatusFilterFoundDocuments()
{
    SearchServer server = GetSearchServerDifferentDocsStatus();

    const auto &found_docs = FIND_DOC_WITH_STATUS(server, DocumentStatus::BANNED);

    ASSERT_EQUAL(found_docs.size(), 1u);
    ASSERT_EQUAL(found_docs[0].id, 2);
}

void TestRemovedStatusFilterFoundDocuments()
{
    SearchServer server = GetSearchServerDifferentDocsStatus();

    const auto &found_docs = FIND_DOC_WITH_STATUS(server, DocumentStatus::REMOVED);

    ASSERT_EQUAL(found_docs.size(), 1u);
    ASSERT_EQUAL(found_docs[0].id, 0);
}

void TestRelevanceCalc(int index, double etalon)
{
    const double epsilon = 1e-6;

    SearchServer server = GetSearchServer();
    const auto found_docs = server.FindTopDocuments("cat"s);

    ASSERT(std::abs(found_docs[static_cast<size_t>(index)].relevance - etalon) < epsilon);
}

void TestRelevance()
{
    const double idf_cat = log(5 / 4.0);
    const double relevance_0 = idf_cat * (4 / 6.0);
    const double relevance_1 = idf_cat * (3 / 6.0);
    const double relevance_2 = idf_cat * (3 / 6.0);
    const double relevance_3 = idf_cat * (2 / 6.0);

    TestRelevanceCalc(0, relevance_0);
    TestRelevanceCalc(1, relevance_1);
    TestRelevanceCalc(2, relevance_2);
    TestRelevanceCalc(3, relevance_3);
}

void TestPaginator()
{
    SearchServer server = GetSearchServer();
    const auto search_results = server.FindTopDocuments("dog cat"s);
    {
        const auto pages = Paginate(search_results, static_cast<size_t>(2));
        ASSERT_EQUAL(pages.size(), 3u);
    }

    {
        const auto pages = Paginate(search_results, static_cast<size_t>(3));
        ASSERT_EQUAL(pages.size(), 2u);
    }

    {
        const auto pages = Paginate(search_results, static_cast<size_t>(5));
        ASSERT_EQUAL(pages.size(), 1u);
    }
}

void TestRequestQueue()
{
    SearchServer server("and on at"s);
    RequestQueue request_queue(server);

    server.AddDocument(1, "fluffy cat fluffy tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(2, "fluffy dog and fancy collar"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(3, "big cat fancy collar "s, DocumentStatus::ACTUAL, {1, 2, 8});
    server.AddDocument(4, "big dog starling Eugine"s, DocumentStatus::ACTUAL, {1, 3, 2});
    server.AddDocument(5, "big dog starling Vasya"s, DocumentStatus::ACTUAL, {1, 1, 1});

    constexpr int null_requests = 1439;
    for (int i = 0; i < null_requests; ++i)
    {
        request_queue.AddFindRequest("empty request"s);
    }
    // still 1439 empty requests
    request_queue.AddFindRequest("fluffy dog"s);

    // new day, first query was deleted, 1438 empty requests
    request_queue.AddFindRequest("big collar"s);

    // first query was deleted, 1437 empty requests
    request_queue.AddFindRequest("starling"s);

    ASSERT_EQUAL(request_queue.GetNoResultRequests(), 1437);
}

void TestRemoveDuplicates()
{
    SearchServer server("and with"s);

    server.AddDocument(1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2});

    // дубликат документа 2, будет удалён
    server.AddDocument(3, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2});

    // отличие только в стоп-словах, считаем дубликатом
    server.AddDocument(4, "funny pet and curly hair"s, DocumentStatus::ACTUAL, {1, 2});

    // множество слов такое же, считаем дубликатом документа 1
    server.AddDocument(5, "funny funny pet and nasty nasty rat"s, DocumentStatus::ACTUAL,
                       {1, 2});

    // добавились новые слова, дубликатом не является
    server.AddDocument(6, "funny pet and not very nasty rat"s, DocumentStatus::ACTUAL, {1, 2});

    // множество слов такое же, как в id 6, несмотря на другой порядок, считаем дубликатом
    server.AddDocument(7, "very nasty rat and not very funny pet"s, DocumentStatus::ACTUAL,
                       {1, 2});

    // есть не все слова, не является дубликатом
    server.AddDocument(8, "pet with rat and rat and rat"s, DocumentStatus::ACTUAL, {1, 2});

    // слова из разных документов, не является дубликатом
    server.AddDocument(9, "nasty rat with curly hair"s, DocumentStatus::ACTUAL, {1, 2});

    ASSERT_EQUAL(server.GetDocumentCount(), 9);

    streambuf *orig_buf = cout.rdbuf();
    cout.rdbuf(NULL);

    RemoveDuplicates(server);

    cout.rdbuf(orig_buf);

    ASSERT_EQUAL(server.GetDocumentCount(), 5);
}

void TestProcessQueries()
{
    SearchServer search_server("and with"s);

    for (int id = 0; const string &text : {
                         "funny pet and nasty rat"s,
                         "funny pet with curly hair"s,
                         "funny pet and not very nasty rat"s,
                         "pet with rat and rat and rat"s,
                         "nasty rat with curly hair"s,
                     })
    {
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
    }
    const vector<string> queries = {"nasty rat -not"s, "not very funny nasty pet"s,
                                    "curly hair"s};
    std::vector<std::vector<Document>> result = ProcessQueries(search_server, queries);

    ASSERT_EQUAL(result[0].size(), 3u);
    ASSERT_EQUAL(result[1].size(), 5u);
    ASSERT_EQUAL(result[2].size(), 2u);
}

void TestProcessQueriesJoined()
{
    SearchServer search_server("and with"s);

    for (int id = 0; const string &text : {
                         "funny pet and nasty rat"s,
                         "funny pet with curly hair"s,
                         "funny pet and not very nasty rat"s,
                         "pet with rat and rat and rat"s,
                         "nasty rat with curly hair"s,
                     })
    {
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
    }

    const vector<string> queries = {"nasty rat -not"s, "not very funny nasty pet"s,
                                    "curly hair"s};

    std::vector<Document> result = ProcessQueriesJoined(search_server, queries);

    ASSERT_EQUAL(result.size(), 10u);
    ASSERT_EQUAL(result.front().id, 1);
    // result.pop_front();
    result.erase(result.begin(), result.begin() + 1);
    ASSERT_EQUAL(result.front().id, 5);
    result.erase(result.begin(), result.begin() + 1);
    ASSERT_EQUAL(result.front().id, 4);
    result.erase(result.begin(), result.begin() + 1);
    ASSERT_EQUAL(result.front().id, 3);
    result.erase(result.begin(), result.begin() + 1);
    ASSERT_EQUAL(result.front().id, 1);
    result.erase(result.begin(), result.begin() + 1);
    ASSERT_EQUAL(result.front().id, 2);
    result.erase(result.begin(), result.begin() + 1);
    ASSERT_EQUAL(result.front().id, 5);
    result.erase(result.begin(), result.begin() + 1);
    ASSERT_EQUAL(result.front().id, 4);
    result.erase(result.begin(), result.begin() + 1);
    ASSERT_EQUAL(result.front().id, 2);
    result.erase(result.begin(), result.begin() + 1);
    ASSERT_EQUAL(result.front().id, 5);
    result.erase(result.begin(), result.begin() + 1);
}

void TestAll1()
{
    TestRunner tr;

    RUN_TEST(tr, TestStopWordStringConstructor);
    RUN_TEST(tr, TestStopWordVectorConstructor);
    RUN_TEST(tr, TestStopWordSetConstructor);
    RUN_TEST(tr, TestStringConstructorWithSpecialCharacters);
    RUN_TEST(tr, TestVectorConstructorWithSpecialCharacters);
    RUN_TEST(tr, TestSetConstructorWithSpecialCharacters);

    RUN_TEST(tr, TestAddDocWithNegativeID);
    RUN_TEST(tr, TestAddDocWithAddedID);
    RUN_TEST(tr, TestAddDocWithSpecialCharacters);

    RUN_TEST(tr, TestSearchQueryWithSpecialCharacters);
    RUN_TEST(tr, TestSearchQueryWithDoubleMinus);
    RUN_TEST(tr, TestSearchQueryWithEmptyMinusWord);

    RUN_TEST(tr, TestExcludeDocumentsWithMinusWords);
    RUN_TEST(tr, TestMatchDocumentNormalQuery);
    RUN_TEST(tr, TestMatchDocumentQueryWithMinusWords);
    RUN_TEST(tr, TestMatchDocumentQueryWithSpecialCharacters);
    RUN_TEST(tr, TestMatchDocumentQueryWithDoubleMinus);
    RUN_TEST(tr, TestMatchDocumentQueryWithEmptyMinusWord);

    RUN_TEST(tr, TestSortFoundDocumentsToRelevance);
    RUN_TEST(tr, TestFoundDocumentsPlusRating);
    RUN_TEST(tr, TestFoundDocumentsMinusRating);
    RUN_TEST(tr, TestUserFilterFoundDocuments);

    RUN_TEST(tr, TestActualStatusFilterFoundDocuments);
    RUN_TEST(tr, TestIrrelevantStatusFilterFoundDocuments);
    RUN_TEST(tr, TestBannedStatusFilterFoundDocuments);
    RUN_TEST(tr, TestRemovedStatusFilterFoundDocuments);

    RUN_TEST(tr, TestRelevance);

    RUN_TEST(tr, TestPaginator);

    RUN_TEST(tr, TestRequestQueue);

    RUN_TEST(tr, TestRemoveDuplicates);

    RUN_TEST(tr, TestProcessQueries);
    RUN_TEST(tr, TestProcessQueriesJoined);
}

////////////////////////////////////////////////////////////////////////////

string GenerateWord(mt19937 &generator, int max_length)
{
    const int length = uniform_int_distribution(1, max_length)(generator);
    string word;
    word.reserve(length);
    for (int i = 0; i < length; ++i)
    {
        word.push_back(uniform_int_distribution<>('a', 'z')(generator));
    }
    return word;
}

vector<string> GenerateDictionary(mt19937 &generator,
                                  int word_count, int max_length)
{
    vector<string> words;
    words.reserve(word_count);
    for (int i = 0; i < word_count; ++i)
    {
        words.push_back(GenerateWord(generator, max_length));
    }
    sort(words.begin(), words.end());
    words.erase(unique(words.begin(), words.end()), words.end());
    return words;
}

string GenerateQuery(mt19937 &generator,
                     const vector<string> &dictionary,
                     int word_count,
                     double minus_prob = 0)
{
    string query;
    for (int i = 0; i < word_count; ++i)
    {
        if (!query.empty())
        {
            query.push_back(' ');
        }
        if (uniform_real_distribution<>(0, 1)(generator) < minus_prob)
        {
            query.push_back('-');
        }
        query +=
            dictionary[uniform_int_distribution<int>(0, dictionary.size() - 1)(generator)];
    }
    return query;
}

vector<string> GenerateQueries(mt19937 &generator,
                               const vector<string> &dictionary,
                               int query_count,
                               int max_word_count)
{
    vector<string> queries;
    queries.reserve(query_count);
    for (int i = 0; i < query_count; ++i)
    {
        queries.push_back(GenerateQuery(generator, dictionary, max_word_count));
    }
    return queries;
}

template <typename QueriesProcessor>
void Test(string_view mark,
          QueriesProcessor processor,
          const SearchServer &search_server,
          const vector<string> &queries)
{
    LOG_DURATION_STREAM(mark, cout);
    const auto documents = processor(search_server, queries);
    cout << "documents size: "s << documents.size() << endl;
}

#define TEST(processor) Test(#processor, processor, search_server, queries)

template <typename ExecutionPolicy>
void TestRemoveDocument(string_view mark,
                        SearchServer search_server,
                        ExecutionPolicy &&policy)
{
    LOG_DURATION_STREAM(mark, cout);
    const int document_count = search_server.GetDocumentCount();
    for (int id = 0; id < document_count; ++id)
    {
        search_server.RemoveDocument(policy, id);
    }
    cout << "SearchServer DocumentCount: "s << search_server.GetDocumentCount() << endl;
}

#define TEST_REMOVE_DOCUMENT(policy) \
    TestRemoveDocument(#policy, search_server, execution::policy)

template <typename ExecutionPolicy>
void TestMatchDocument(string_view mark,
                       SearchServer search_server,
                       const string &query,
                       ExecutionPolicy &&policy)
{
    LOG_DURATION_STREAM(mark, cout);
    const int document_count = search_server.GetDocumentCount();
    int word_count = 0;
    for (int id = 0; id < document_count; ++id)
    {
        const auto [words, status] = search_server.MatchDocument(policy, query, id);
        word_count += words.size();
    }
    cout << "word count: "s << word_count << endl;
}

#define TEST_MATCH_DOCUMENT(policy) \
    TestMatchDocument(#policy, search_server, query, execution::policy)

// template <typename ExecutionPolicy>
// void TestFindTopDocuments(string_view mark,
//                           const SearchServer& search_server,
//                           const vector<string>& queries,
//                           ExecutionPolicy&& policy) {
//     LOG_DURATION_STREAM(mark, cout);
//     double total_relevance = 0;
//     for (const string_view query : queries) {
//         for (const auto &document : search_server.FindTopDocuments(policy, query)) {
//             total_relevance += document.relevance;
//         }
//     }
//     cout << "total relevance: "s << total_relevance << endl;
// }
//
// #def ine TEST_FIND_DOC(policy)                                                                 \
//    TestFindTopDocuments(#policy, search_server, queries, execution::policy)

void TestAll2()
{

    mt19937 generator;
    {
        cout << "\tTESTING PROCESS QUERIES"s << endl;
        const auto dictionary = GenerateDictionary(generator, 10000, 25);
        const auto documents = GenerateQueries(generator, dictionary, 100'000, 10);

        SearchServer search_server(dictionary[0]);
        for (size_t i = 0; i < documents.size(); ++i)
        {
            search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
        }

        const auto queries = GenerateQueries(generator, dictionary, 10'000, 7);
        TEST(ProcessQueries);
    }

    cout << endl;

    {
        cout << "\tTESTING REMOVE DOCUMENT"s << endl;
        const auto dictionary = GenerateDictionary(generator, 10000, 25);
        const auto documents = GenerateQueries(generator, dictionary, 10'000, 100);

        SearchServer search_server(dictionary[0]);
        for (size_t i = 0; i < documents.size(); ++i)
        {
            search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
        }

        TEST_REMOVE_DOCUMENT(seq);
        TEST_REMOVE_DOCUMENT(par);
    }

    cout << endl;

    {
        cout << "\tTESTING MATCH DOCUMENT"s << endl;
        const auto dictionary = GenerateDictionary(generator, 1000, 10);
        const auto documents = GenerateQueries(generator, dictionary, 10'000, 70);

        const string query = GenerateQuery(generator, dictionary, 500, 0.1);

        SearchServer search_server(dictionary[0]);
        for (size_t i = 0; i < documents.size(); ++i)
        {
            search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
        }
        TEST_MATCH_DOCUMENT(seq);
        TEST_MATCH_DOCUMENT(par);
    }

    cout << endl;

    //{
    //    cout << "\tTESTING FIND TOP DOCUMENTS"s << endl;
    //    const auto dictionary = GenerateDictionary(generator, 1000, 10);
    //    const auto documents = GenerateQueries(generator, dictionary, 10'000, 70);

    //    SearchServer search_server(dictionary[0]);
    //    for (size_t i = 0; i < documents.size(); ++i) {
    //        search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
    //    }

    //    const auto queries = GenerateQueries(generator, dictionary, 100, 70);

    //    TEST_FIND_DOC(seq);
    //    TEST_FIND_DOC(par);
    //}

    cout << endl;
}

////////////////////////////////////////////////////////////////////////////

int main()
{
    try
    {
        TestAll1();
        TestAll2();
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}
