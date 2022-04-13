#include "request_queue.h"

#include <vector>
#include <string>

RequestQueue::RequestQueue(const SearchServer& search_server)
    : search_server_(search_server)
    , requests_count_(0)
    , empty_requests_count_(0)
{
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status) {
    return AddFindRequest(raw_query, [status](int document_id, DocumentStatus doc_status, int rating) {
        return doc_status == status;
    });
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query) {
    return AddFindRequest(raw_query, DocumentStatus::ACTUAL);
}

int RequestQueue::GetNoResultRequests() const {
    return empty_requests_count_;
}