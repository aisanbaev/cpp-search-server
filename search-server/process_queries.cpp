#include "process_queries.h"

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server, const std::vector<std::string>& queries) {
    std::vector<std::vector<Document>> result(queries.size());

    std::transform(
        std::execution::par,
        queries.begin(), queries.end(), result.begin(),
        [&search_server](const auto& query) { return search_server.FindTopDocuments(query); }
    );

    return result;
}

std::vector<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string>& queries) {
    std::vector<Document> result{};
    const auto& documents_on_queries = ProcessQueries(search_server, queries);

    std::for_each(
        documents_on_queries.begin(), documents_on_queries.end(),
        [&result](const auto& documents)
        { result.insert(result.end(), documents.begin(), documents.end()); }
    );

    return result;
}
