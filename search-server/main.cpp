#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
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

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:

    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate predicate) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, predicate);
        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                    return lhs.rating > rhs.rating;
                } else {
                    return lhs.relevance > rhs.relevance;
                }
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) { return document_status == status; });
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return {matched_words, documents_.at(document_id).status};
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

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

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {
            text,
            is_minus,
            IsStopWord(text)
        };
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query, DocumentPredicate predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }
        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
            });
        }
        return matched_documents;
    }
};

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template<typename TestFunc>
void RunTestImpl(TestFunc test_func, const string& func) {
    test_func();
    cerr << func << " OK"s << endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)

// Тест проверяет, что добавленный документ находится по поисковому запросу, если в нем содержатся слова из документа
void TestAddDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("dog at home"s).empty());
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL(server.FindTopDocuments("cat"s).size(), 1u);
    }
}

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

// Тест проверяет, что документы, содержащие минус-слова из поискового запроса, не включаются в результаты поиска
void TestExcludeDocumentsWithMinusWordInQuery() {
    const int doc_id = 42;
    const int doc_id2 = 50;
    const string content = "fluffy cat in the city"s;
    const string content2 = "fluffy cat with a collar"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("fluffy cat"s);
        ASSERT_EQUAL(found_docs.size(), 2u); // слова запроса найдены в двух документах
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("fluffy cat -collar"s);
        ASSERT_EQUAL(found_docs.size(), 1u); // слова запроса найдены только в одном документе
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, 42); // номер документа, в котором не содержится минус-слово
    }
}

// Тест проверяет соответствие документов поисковому запросу
// Должны быть возвращены все слова из поискового запроса, присутствующие в документе
void TestMatchDocument() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto [words, status] = server.MatchDocument("cat in the village"s, doc_id);
        ASSERT(words.size() == 3u); // в документе найдены три слова из поискового запроса
        ASSERT_EQUAL(words[0], "cat"s);
        ASSERT_EQUAL(words[1], "in"s);
        ASSERT_EQUAL(words[2], "the"s);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto [words, status] = server.MatchDocument("cat in the -city"s, doc_id);
        ASSERT(words.empty()); // слов не найдено, т.к. в запросе есть минус-слово, которое содержится в документе
    }
}

// Тест проверяет правильность сортировки документов по релевантности
// Возвращаемые при поиске документов результаты должны быть отсортированы в порядке убывания релевантности
void TestSortByRelevance() {
    const int doc_id0 = 0;
	const int doc_id1 = 1;
	const int doc_id2 = 2;
	const string content0 = "cat in"s;
	const string content1 = "fluffy cat"s;
	const string content2 = "fluffy cat with a beautiful collar"s;
	const vector<int> ratings = {1, 2, 3};

    SearchServer server;
    server.AddDocument(doc_id0, content0, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings);
    const auto found_docs = server.FindTopDocuments("fluffy cat and collar"s);

    ASSERT_EQUAL((found_docs[0]).id, doc_id2);
    ASSERT_EQUAL((found_docs[1]).id, doc_id1);
    ASSERT_EQUAL((found_docs[2]).id, doc_id0);
    ASSERT((found_docs[0]).relevance > (found_docs[1]).relevance);
    ASSERT((found_docs[1]).relevance > (found_docs[2]).relevance);
}

//Тест для проверки вычисления рейтинга документа. Рейтинг добавленного документа равен среднему арифметическому оценок документа
void TestCalculateRatingDocument() {
    const int doc_id0 = 0;
	const int doc_id1 = 1;
	const int doc_id2 = 2;
	const string content0 = "fluffy cat with a beautiful collar"s;
	const string content1 = "fluffy cat"s;
	const string content2 = "cat in"s;

    SearchServer server;
    server.AddDocument(doc_id0, content0, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, {13, 14, 15 ,16, 17});
    server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, {-5, 0, 5});

    const auto found_docs = server.FindTopDocuments("fluffy cat and collar"s);
    ASSERT((found_docs[0]).rating == 2);
    ASSERT((found_docs[1]).rating == 15);
    ASSERT((found_docs[2]).rating == 0);
}

// Тест для проверки фильтрации результатов поиска с использованием предиката, задаваемого пользователем
void TestFilterSearchResultWithCustomPredicate() {
    const int doc_id0 = 0;
	const int doc_id1 = 1;
	const int doc_id2 = 2;
	const int doc_id3 = 3;
	const string content0 = "cat in"s;
	const string content1 = "fluffy cat"s;
	const string content2 = "fluffy cat in a beautiful collar"s;
	const string content3 = "cat without collar"s;

	SearchServer server;
    server.AddDocument(doc_id0, content0, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, {13, 14, 15 ,16, 17});
    server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, {1, 2, 5});
    server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, {-5, -3, 3});
    {
        const auto found_docs = server.FindTopDocuments("cat"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
        ASSERT(found_docs.size() == 2u); // найдены два документа с четными номерами doc_id
        ASSERT_EQUAL(found_docs[1].id, doc_id2);
    }
    {
        const auto found_docs = server.FindTopDocuments("cat"s, [](int document_id, DocumentStatus status, int rating) { return rating < 0; });
        ASSERT(found_docs.size() == 1u); // найден один документ с отрицательным рейтингом
        ASSERT_EQUAL(found_docs[0].id, doc_id3);
    }

}

// Тест для проверки поиска документов, имеющих заданный статус
void TestSearchDocumentsWithCurrentStatus() {
    const int doc_id0 = 0;
	const int doc_id1 = 1;
	const int doc_id2 = 2;
	const int doc_id3 = 3;
	const string content0 = "cat in"s;
	const string content1 = "fluffy cat"s;
	const string content2 = "fluffy cat in a beautiful collar"s;
	const string content3 = "dog without collar"s;
	const vector<int> ratings = {1, 2, 3};

    SearchServer server;
    server.AddDocument(doc_id0, content0, DocumentStatus::REMOVED, ratings);
    server.AddDocument(doc_id1, content1, DocumentStatus::BANNED, ratings);
    server.AddDocument(doc_id2, content2, DocumentStatus::BANNED, ratings);
    server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings);
	ASSERT(server.FindTopDocuments("fluffy cat and collar"s, DocumentStatus::REMOVED).size() == 1);
	ASSERT(server.FindTopDocuments("fluffy cat and collar"s, DocumentStatus::BANNED).size() == 2);
}

// Тест проверяет корректность вычисления релевантности документа
void TestCalculateRelevanceDocument() {
    const int doc_id0 = 0;
	const int doc_id1 = 1;
	const int doc_id2 = 2;
	const string content0 = "dog at home"s;
	const string content1 = "fluffy cat"s;
	const string content2 = "fluffy cat and collar"s;
	const vector<int> ratings = {1, 2, 3};

    SearchServer server;
    server.AddDocument(doc_id0, content0, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings);
    const auto found_docs = server.FindTopDocuments("brown cat with collar"s);
    const double calculate_relevance_doc_id2 = log(1.5)*0.25 + log(3)*0.25; // рассчитываем релевантность вручную

    ASSERT_EQUAL((found_docs[0]).id, doc_id2);
    ASSERT(abs((found_docs[0]).relevance - calculate_relevance_doc_id2) < 1e-6); // сравниваем вычисленную и расчетную релевантности
}

void TestSearchServer() {
    RUN_TEST(TestAddDocumentContent);
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludeDocumentsWithMinusWordInQuery);
    RUN_TEST(TestMatchDocument);
    RUN_TEST(TestSortByRelevance);
    RUN_TEST(TestCalculateRatingDocument);
    RUN_TEST(TestFilterSearchResultWithCustomPredicate);
    RUN_TEST(TestSearchDocumentsWithCurrentStatus);
    RUN_TEST(TestCalculateRelevanceDocument);
}

int main() {
    TestSearchServer();
    cout << "Search server testing finished"s << endl;

    SearchServer search_server;

    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});

    cout << "ACTUAL by default:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
        PrintDocument(document);
    }

    cout << "BANNED:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED)) {
        PrintDocument(document);
    }

    cout << "Even ids:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }

    return 0;
}
