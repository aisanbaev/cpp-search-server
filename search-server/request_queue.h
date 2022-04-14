#pragma once
#include "search_server.h"

#include <vector>
#include <string>
#include <deque>

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const;

private:
    struct QueryResult {
        std::vector<Document> query_result;
        bool is_empty_result;
    };

    const static int min_in_day_ = 1440;
    const SearchServer& search_server_;

    std::deque<QueryResult> requests_;
    int requests_count_;
    int empty_requests_count_;
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
    const auto& documents = search_server_.FindTopDocuments(raw_query, document_predicate);
    if (documents.empty()) {
        ++requests_count_;
        ++empty_requests_count_;
    } else {
        ++requests_count_;
    }

    if (requests_count_ > min_in_day_) {
        if (requests_[0].is_empty_result) {
            --empty_requests_count_;
        }
        requests_.pop_front();
        requests_.push_back({documents, documents.empty()});
    } else {
        requests_.push_back({documents, documents.empty()});
    }
    return documents;
}
