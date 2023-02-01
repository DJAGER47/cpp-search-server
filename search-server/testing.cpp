#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <iostream>
#include <numeric>
#include <regex>

using namespace std;

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

// -------- ������ ��������� ������ ��������� ������� ----------

// ���� ���������, ��� ��������� ������� ��������� ����-����� ��� ����������
// ���������� ��������� ����-����. ����-����� ����������� �� ������ ����������.
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

// ���������� ����������. ����������� �������� ������ ���������� �� ����������
// �������, ������� �������� ����� �� ���������.
void TestAddDocFindDoc()
{
    {
        SearchServer server;
        ASSERT_EQUAL(server.GetDocumentCount(), 0);
        server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, {8, -3});
        ASSERT_EQUAL(server.GetDocumentCount(), 1);
    }

    {
        SearchServer server;
        ASSERT_EQUAL(server.FindTopDocuments("����� ��� � ������ �������").size(), 0u);
        server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, {8, -3});
        const auto found = server.FindTopDocuments("����� ��� � ������ �������");
        ASSERT_EQUAL(found.size(), 1u);
        ASSERT_EQUAL(found.at(0).id, 0);
    }
}

// ��������� �����-����. ���������, ���������� �����-����� ���������� �������,
// �� ������ ���������� � ���������� ������.
void TestMinusWords()
{
    {
        SearchServer server;
        server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, {8, -3});
        server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::ACTUAL, {7, 2, 7});

        const auto found_docs1 = server.FindTopDocuments("���"s);
        ASSERT_EQUAL(found_docs1.size(), 2u);
        ASSERT_EQUAL(found_docs1.at(0).id, 1);
        ASSERT_EQUAL(found_docs1.at(1).id, 0);

        const auto found_docs2 = server.FindTopDocuments("��� -��������"s);
        ASSERT_EQUAL(found_docs2.size(), 1u);
        ASSERT_EQUAL(found_docs2.at(0).id, 0);

        const auto found_docs3 = server.FindTopDocuments("-��������"s);
        ASSERT_EQUAL(found_docs3.size(), 0u);
    }
}

// ������� ����������. ��� �������� ��������� �� ���������� ������� ������ ����
// ���������� ��� ����� �� ���������� �������, �������������� � ���������. ����
// ���� ������������ ���� �� �� ������ �����-�����, ������ ������������ ������
// ������ ����.
void TestMatch()
{
    SearchServer search_server;
    search_server.SetStopWords("� � ��"s);
    search_server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::BANNED, {7, 2, 7});
    search_server.AddDocument(2, "��������� �� ������������� �����"s, DocumentStatus::IRRELEVANT, {5, -12, 2, 1});
    search_server.AddDocument(3, "��������� ������� �������"s, DocumentStatus::REMOVED, {9});

    const auto [matched_words1, status1] = search_server.MatchDocument("�������� ��������� ���"s, 0);
    const auto [matched_words2, status2] = search_server.MatchDocument("�������� ��������� ���"s, 1);
    const auto [matched_words3, status3] = search_server.MatchDocument("�������� ��������� ���"s, 2);
    const auto [matched_words4, status4] = search_server.MatchDocument("�������� ��������� ���"s, 3);

    ASSERT_EQUAL(matched_words1.size(), 1u);
    ASSERT_EQUAL(matched_words1.at(0), "���"s);
    ASSERT(status1 == DocumentStatus::ACTUAL);

    ASSERT_EQUAL(matched_words2.size(), 2u);
    ASSERT_EQUAL(matched_words2.at(0), "���"s);
    ASSERT_EQUAL(matched_words2.at(1), "��������"s);
    ASSERT(status2 == DocumentStatus::BANNED);

    ASSERT_EQUAL(matched_words3.size(), 1u);
    ASSERT_EQUAL(matched_words3.at(0), "���������"s);
    ASSERT(status3 == DocumentStatus::IRRELEVANT);

    ASSERT_EQUAL(matched_words4.size(), 1u);
    ASSERT_EQUAL(matched_words4.at(0), "���������"s);
    ASSERT(status4 == DocumentStatus::REMOVED);
}

// ���������� ��������� ���������� �� �������������. ������������ ��� ������
// ���������� ���������� ������ ���� ������������� � ������� ��������
// �������������.
void TestSortByRelevance()
{
    SearchServer search_server;
    search_server.SetStopWords("� � ��"s);
    search_server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(3, "��������� ������� �������"s, DocumentStatus::BANNED, {9});

    const auto found_docs1 = search_server.FindTopDocuments("�������� ��������� ���"s, DocumentStatus::ACTUAL);
    ASSERT_EQUAL(found_docs1.size(), 2u);
    ASSERT(found_docs1.at(0).relevance > found_docs1.at(1).relevance);
}

// ���������� �������� ����������. ������� ������������ ��������� ����� ��������
// ��������������� ������ ���������.
void TestRaiting()
{
    {
        SearchServer server;
        vector<int> _vec = {8, -3};
        server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, _vec);

        const auto found_docs1 = server.FindTopDocuments("���"s);
        int rating = static_cast<int>(accumulate(_vec.begin(), _vec.end(), 0) / (_vec.size() ? _vec.size() : 1));
        ASSERT_EQUAL(found_docs1.at(0).rating, rating);
    }
    {
        SearchServer server;
        vector<int> _vec = {};
        server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, _vec);

        const auto found_docs1 = server.FindTopDocuments("���"s);
        int rating = static_cast<int>(accumulate(_vec.begin(), _vec.end(), 0) / (_vec.size() ? _vec.size() : 1));
        ASSERT_EQUAL(found_docs1.at(0).rating, rating);
    }
    {
        SearchServer server;
        vector<int> _vec = {-9};
        server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, _vec);

        const auto found_docs1 = server.FindTopDocuments("���"s);
        int rating = static_cast<int>(accumulate(_vec.begin(), _vec.end(), 0) / (_vec.size() ? _vec.size() : 1));
        ASSERT_EQUAL(found_docs1.at(0).rating, rating);
    }
}

// ���������� ����������� ������ � �������������� ���������, �����������
// �������������.
void TestPredicate()
{

    SearchServer search_server;
    search_server.SetStopWords("� � ��"s);
    search_server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "��������� �� ������������� �����"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "��������� ������� �������"s, DocumentStatus::BANNED, {9});

    const auto found_docs1 = search_server.FindTopDocuments("�������� ��������� ��� "s,
                                                            [](int document_id, DocumentStatus status, int rating)
                                                            { return document_id % 2 == 0; });
    ASSERT_EQUAL(found_docs1[0].id, 0);
    ASSERT_EQUAL(found_docs1[1].id, 2);

    ASSERT_EQUAL(found_docs1[0].rating, 2);
    ASSERT_EQUAL(found_docs1[1].rating, -1);
}

// ����� ����������, ������� �������� ������.
void TestStatus()
{
    SearchServer search_server;
    search_server.SetStopWords("� � ��"s);
    search_server.AddDocument(0, "����� ��� � ������ �������"s, DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "�������� ��� �������� �����"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "��������� �� ������������� �����"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "��������� ������� �������"s, DocumentStatus::BANNED, {9});

    const auto found_docs1 = search_server.FindTopDocuments("�������� ��������� ���"s, DocumentStatus::ACTUAL);
    ASSERT_EQUAL(found_docs1.size(), 3u);

    const auto found_docs2 = search_server.FindTopDocuments("�������� ��������� ���"s, DocumentStatus::BANNED);
    ASSERT_EQUAL(found_docs2.size(), 1u);

    const auto found_docs3 = search_server.FindTopDocuments("�������� ��������� ���"s, DocumentStatus::IRRELEVANT);
    ASSERT_EQUAL(found_docs3.size(), 0u);

    const auto found_docs4 = search_server.FindTopDocuments("�������� ��������� ���"s, DocumentStatus::REMOVED);
    ASSERT_EQUAL(found_docs4.size(), 0u);
}

// ���������� ���������� ������������� ��������� ����������.
void TestCalcRelevance()
{
    SearchServer search_server;
    search_server.AddDocument(0, "����� ��� ������ �������"s, DocumentStatus::ACTUAL, {});
    search_server.AddDocument(1, "�������� ���"s, DocumentStatus::ACTUAL, {});
    search_server.AddDocument(2, "��������� �� ������������� �����"s, DocumentStatus::ACTUAL, {1});

    // IDF ����� "���". ��� ����������� � ���� ���������� �� ���: 0 � 1. 3 / 2 = 1,5.
    // �������?�� �������� � ����?���� �������� 0,4055.

    // TF ����� "���" � ��������� 0. ����� ���� � ���� ��������� ������,
    // �� ��� ��� � ������ ����. 1 / 4 = 0,25.

    // TF ����� "���" � ��������� 1. ����� ���� � ���� ��������� ���,
    // �� ��� ��� � ������ ����. 1 / 2 = 0,5.

    const auto found_docs1 = search_server.FindTopDocuments("���"s, DocumentStatus::ACTUAL);
    ASSERT_EQUAL(found_docs1.size(), 2u);

    // 0) IDF * TF = 0,4055 * 0,5  = 0,20275
    double relevance1 = log(3.0 / 2.0) * (1.0 / 2.0);
    ASSERT(abs(found_docs1[0].relevance - relevance1) < 10e-6);

    // 1) IDF * TF = 0,4055 * 0,25 = 0,101375
    double relevance2 = log(3.0 / 2.0) * (1.0 / 4.0);
    ASSERT(abs(found_docs1[1].relevance - relevance2) < 10e-6);
}

// ������� TestSearchServer �������� ������ ����� ��� ������� ������
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

// --------- ��������� ��������� ������ ��������� ������� -----------

// int main()
// {
//     TestSearchServer();
//     // ���� �� ������ ��� ������, ������ ��� ����� ������ �������
//     cout << "Search server testing finished"s << endl;
// }