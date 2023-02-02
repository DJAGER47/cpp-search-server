#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <numeric>
#include <regex>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <optional>

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

/// @brief Разбиваем на слова, игнорируем повторяющиеся пробелы
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
    int id = 0;
    double relevance = 0;
    int rating = 0;

    Document() = default;

    Document(int _id, double _relevance, int _rating)
        : id(_id), relevance(_relevance), rating(_rating){};
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
    explicit SearchServer() = default;

    explicit SearchServer(const string &text)
    {
        if (!IsValidWord(text))
            throw invalid_argument("Stop-words invalid char");

        auto trim_text = regex_replace(text, regex("^ +| +$|( ) +"), "$1");
        for (const string &word : SplitIntoWords(trim_text))
        {
            stop_words_.insert(word);
        }
    }

    template <typename T>
    explicit SearchServer(const T &text)
    {
        for (const string &word : text)
        {
            if (!IsValidWord(word))
                throw invalid_argument("Stop-words invalid char");

            if (word.length() && (stop_words_.count(word) == 0))
                stop_words_.insert(word);
        }
    }

    /// @brief Добавить документ в поисковую базу
    void AddDocument(int document_id, const string &document, DocumentStatus status, const vector<int> &ratings)
    {
        // Попытка добавить документ с отрицательным id. Или уже такой id есть
        if (document_id < 0)
            throw invalid_argument("document_id must be positive");

        if (documents_.count(document_id))
            throw invalid_argument("document_id already exists");

        if (!IsValidWord(document))
            throw invalid_argument("document invalid char");

        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string &word : words)
        {
            if (!IsMinusWordVoid(word))
                throw invalid_argument("document invalid minus word");

            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
        index2id.push_back(document_id);
    }

    vector<Document> FindTopDocuments(const string &raw_query, DocumentStatus status_query) const
    {
        return FindTopDocuments(
            raw_query, [status_query](int document_id, DocumentStatus status, int rating)
            { return status == status_query; });
    }

    vector<Document> FindTopDocuments(const string &raw_query) const
    {
        return FindTopDocuments(
            raw_query, [](int document_id, DocumentStatus status, int rating)
            { return status == DocumentStatus::ACTUAL; });
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string &raw_query, DocumentPredicate document_predicate) const
    {
        if (!IsValidWord(raw_query))
            throw invalid_argument("raw_query invalid char");

        const Query query = ParseQuery(raw_query);
        for (const string &word : query.minus_words)
        {
            if (!Is2MinusWord(word))
                throw invalid_argument("raw_query incorrect query minus words");
        }
        vector<Document> result = FindAllDocuments(query, document_predicate);
        sort(result.begin(), result.end(),
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
        if (result.size() > MAX_RESULT_DOCUMENT_COUNT)
        {
            result.resize(MAX_RESULT_DOCUMENT_COUNT);
        }

        return result;
    }

    int GetDocumentCount() const { return static_cast<int>(documents_.size()); }

    int GetDocumentId(int index) const
    {
        if ((index >= 0) && (index < index2id.size()))
        {
            return index2id.at(index);
        }
        throw out_of_range("index does not exist");
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string &raw_query, int document_id) const
    {
        if (document_id < 0)
            throw invalid_argument("document_id must be positive");

        if (!IsValidWord(raw_query))
            throw invalid_argument("raw_query invalid char");

        const Query query = ParseQuery(raw_query);
        for (const string &word : query.minus_words)
        {
            if (!Is2MinusWord(word))
                throw invalid_argument("raw_query incorrect query minus words");
        }

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
    vector<int> index2id;

    /// @brief Наличие спецсимволов — то есть символов с кодами в диапазоне от 0 до 31 включительно — в тексте документов и поискового запроса.
    static bool IsValidWord(const string &word)
    {
        // A valid word must not contain special characters
        return none_of(word.begin(), word.end(), [](char c)
                       { return c >= '\0' && c < ' '; });
    }

    /// @brief Отсутствие в поисковом запросе текста после символа «минус»,  "кот -"
    static bool IsMinusWordVoid(const string &word)
    {
        if ((word.length() == 1) && (word[0] == '-'))
            return false;
        return true;
    }

    /// @brief Отсутствие в минус словах "--"
    static bool Is2MinusWord(const string &word)
    {
        if ((word.empty()) || (word[0] == '-'))
            return false;
        else
            return true;
    }

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
            matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
};

void PrintDocument(const Document &document)
{
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s << endl;
}
int main()
{
    setlocale(LC_ALL, "Russian");

    SearchServer search_server("и в на"s);
}
