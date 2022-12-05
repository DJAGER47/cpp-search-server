#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <map>
#include <cmath>
#include <numeric>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result = 0;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
    int rating;
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }
    
    void AddDocument(int document_id, const string& document, const vector<int>& rating) {
        auto words = SplitIntoWordsNoStop(document);
        double TF = 1.0 / words.size();
        for (const auto& word : words){
            word_to_document_freqs_[word][document_id] += TF;
        }
        document_count_++;
        document_ratings_[document_id] = ComputeAverageRating(rating);
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        const Query query_words = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query_words);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 return lhs.relevance > rhs.relevance;
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

private:
    
    struct Query {
        set<string> plus;
        set<string> minus;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, int> document_ratings_;
    size_t document_count_ = 0;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    Query ParseQuery(const string& text) const {
        Query query_words;
        for (const string& word : SplitIntoWordsNoStop(text)) {
            if (word[0] == '-'){
                query_words.minus.insert(word.substr(1));
            }else{
                query_words.plus.insert(word);
            }
        }
        return query_words;
    }

    static double calculateIDF(size_t document_count, size_t word_freqs){
      return log(static_cast<double>(document_count) / word_freqs);
    }

    vector<Document> FindAllDocuments(const Query& query_words) const {
        vector<Document> matched_documents;
        map<int, double> document_to_relevance;

        for (const auto& word : query_words.plus){
            if (word_to_document_freqs_.count(word) != 0){
                double IDF = calculateIDF(document_count_, word_to_document_freqs_.at(word).size());
                for (const auto& [id,TF] : word_to_document_freqs_.at(word)){
                    document_to_relevance[id] += TF * IDF;
                }
            }
        }
        for (const auto& word : query_words.minus){
            if (word_to_document_freqs_.count(word) != 0){
                for (const auto& [id,TF] : word_to_document_freqs_.at(word)){
                    document_to_relevance.erase(id);
                }
            }
        }

        for (const auto [id, relevance] : document_to_relevance){
            matched_documents.push_back({id, relevance, document_ratings_.at(id)});
        }
        
        return matched_documents;
    }

    int ComputeAverageRating(const vector<int>& ratings) const {
        int size = ratings.size();
        return  (accumulate(ratings.begin(), ratings.end(), 0) / size);
    }
};

SearchServer CreateSearchServer() {
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());
    const int document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
      auto doc = ReadLine();
      size_t count;
      int number;
      vector<int> numbers;
      cin >> count;
      for (auto i = 0; i < count; ++i){
          cin >> number;
          numbers.push_back(number);
      }
      search_server.AddDocument(document_id, ReadLine(), numbers);
    }
    return search_server;
}

int main() {
    const SearchServer search_server = CreateSearchServer();
    const string query = ReadLine();
    for (const auto& [document_id, relevance, ratings] : search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "s << document_id << ", "
             << "relevance = "s << relevance << ", "
             << "rating = "s <<  ratings << "}"s << endl;
    }
}