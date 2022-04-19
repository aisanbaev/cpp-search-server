#include "search_server.h"

SearchServer::SearchServer(std::string_view stop_words_text)
    : SearchServer(SplitIntoWordsView(stop_words_text)) {
}

SearchServer::SearchServer(const std::string& stop_words_text)
    : SearchServer(std::string_view(stop_words_text)) {
}

void SearchServer::AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw std::invalid_argument("Invalid document_id"s);
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});

    const auto words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();

    for (const std::string_view word : words) {
        word_to_document_freqs_[std::string(word)][document_id] += inv_word_count;
        auto it = word_to_document_freqs_.find(std::string(word));
        document_to_word_freqs_[document_id][std::string_view(it->first)] += inv_word_count;
    }

//*** fill string_view container without find ***//

//    for (const std::string_view word : words) {
//        const auto [it, is_true] = word_to_document_freqs_.emplace(std::string(word), std::map<int, double>{});
//        it->second[document_id] += inv_word_count;
//        document_to_word_freqs_[document_id][std::string_view(it->first)] += inv_word_count;
//    }

    document_ids_.insert(document_id);
}

void SearchServer::RemoveDocument(int document_id) {
    if (documents_.count(document_id) == 0) {
        return;
    }
    for (const auto [word, _] : document_to_word_freqs_[document_id]) {
        word_to_document_freqs_[std::string(word)].erase(document_id);
    }

    document_to_word_freqs_.erase(document_id);
    document_ids_.erase(document_id);
    documents_.erase(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
    });
}
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

std::set<int>::const_iterator SearchServer::begin() const {
    return document_ids_.begin();
}

std::set<int>::const_iterator SearchServer::end() const {
    return document_ids_.end();
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static const std::map<std::string_view, double> empty_map{};

    if (document_to_word_freqs_.count(document_id) == 0) {
        return empty_map;
    }

    return document_to_word_freqs_.at(document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::string_view raw_query, int document_id) const {
    return MatchDocument(std::execution::seq, raw_query, document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::sequenced_policy, const std::string_view raw_query, int document_id) const {
    if (!documents_.count(document_id)) {
        throw std::out_of_range("Invalid ID"s);
    }

    const auto query = ParseQuery(raw_query);
    const auto status = documents_.at(document_id).status;

    for (const std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(std::string(word)).count(document_id)) {
            return {std::vector<std::string_view>{}, status};
        }
    }

    std::vector<std::string_view> matched_words;
    for (const std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(std::string(word)).count(document_id)) {
            matched_words.push_back(word);
        }
    }

    return {matched_words, status};
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::parallel_policy, std::string_view raw_query, int document_id) const {
    if (!documents_.count(document_id)) {
        throw std::out_of_range("Invalid ID"s);
    }

    const auto query = ParseQuery(raw_query, false);
    const auto status = documents_.at(document_id).status;
    const auto check_word_contain = [&] (const std::string_view word) {
        return word_to_document_freqs_.at(std::string(word)).count(document_id);
    };

    if (std::any_of(query.minus_words.begin(), query.minus_words.end(), check_word_contain)) {
        return {std::vector<std::string_view>{}, status};
    }

    std::vector<std::string_view> matched_words;
    std::copy_if(
        std::execution::par,
        query.plus_words.begin(), query.plus_words.end(), back_inserter(matched_words),
        check_word_contain
    );

    sort(std::execution::par, matched_words.begin(), matched_words.end());
    auto i = unique(matched_words.begin(), matched_words.end());
    matched_words.erase(i, matched_words.end());

    return {matched_words, status};
}

bool SearchServer::IsStopWord(std::string_view word) const {
    return stop_words_.count(word);
}

bool SearchServer::IsValidWord(std::string_view word) {
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop (std::string_view text) const {
    std::vector<std::string_view> words;
    for (std::string_view word : SplitIntoWordsView(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Word is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = 0;
    rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
    if (text.empty()) {
        throw std::invalid_argument("Query word is empty"s);
    }
    std::string_view word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw std::invalid_argument("The request contains invalid symbols");
    }
    return {word, is_minus, IsStopWord(word)};
}

SearchServer::Query SearchServer::ParseQuery(std::string_view text, bool do_sort) const {
    Query result;
    for (std::string_view word : SplitIntoWordsView(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            } else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }

    if (do_sort) {
        sort(result.plus_words.begin(), result.plus_words.end());
        auto i = unique(result.plus_words.begin(), result.plus_words.end());
        result.plus_words.erase(i, result.plus_words.end());
    }

    return result;
}

// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(std::string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(std::string(word)).size());
}
