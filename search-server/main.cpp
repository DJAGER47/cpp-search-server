#include <iostream>
#include "search_server.h"
#include "request_queue.h"
#include "paginator.h"

using namespace std;

std::ostream &operator<<(std::ostream &out, const Document &doc)
{
    out << "{ "s
        << "document_id = "s << doc.id << ", "s
        << "relevance = "s << doc.relevance << ", "s
        << "rating = "s << doc.rating << " }"s;
    return out;
}

int main()
{
    setlocale(LC_ALL, "Russian");
    SearchServer search_server("and in at"s);
    RequestQueue request_queue(search_server);
    search_server.AddDocument(1, "curly cat curly tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "curly dog and fancy collar"s, DocumentStatus::ACTUAL, {1, 2, 3});
    search_server.AddDocument(3, "big cat fancy collar "s, DocumentStatus::ACTUAL, {1, 2, 8});
    search_server.AddDocument(4, "big dog sparrow Eugene"s, DocumentStatus::ACTUAL, {1, 3, 2});
    search_server.AddDocument(5, "big dog sparrow Vasiliy"s, DocumentStatus::ACTUAL, {1, 1, 1});
    // 1439 �������� � ������� �����������
    for (int i = 0; i < 1439; ++i)
    {
        request_queue.AddFindRequest("empty request"s);
    }
    // ��� ��� 1439 �������� � ������� �����������
    request_queue.AddFindRequest("curly dog"s);
    // ����� �����, ������ ������ ������, 1438 �������� � ������� �����������
    request_queue.AddFindRequest("big collar"s);
    // ������ ������ ������, 1437 �������� � ������� �����������
    request_queue.AddFindRequest("sparrow"s);
    std::cout << "Total empty requests: "s << request_queue.GetNoResultRequests() << endl;
    return 0;
}
