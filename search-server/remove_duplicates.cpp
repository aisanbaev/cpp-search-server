#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
    std::map<std::set<std::string>, int> words_to_document;
    std::set<std::string> words;
    std::vector<int> duplicate_ids;

    for (const int document_id : search_server) {
        for (const auto& [word, freqs] : search_server.GetWordFrequencies(document_id)) {
            words.insert(static_cast<std::string>(word));
        }

        if (words_to_document.count(words)) {
            duplicate_ids.push_back(document_id);
        } else {
            words_to_document[words] = document_id;
        }

        words.clear();
    }

    for (int id : duplicate_ids) {
        std::cout << "Found duplicate document id " << id << std::endl;
        search_server.RemoveDocument(id);
    }
}
