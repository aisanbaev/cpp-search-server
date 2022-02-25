#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
    bool is_duplicate;
    if (search_server.end() - search_server.begin() < 2)
        return;
    for (auto it2 = search_server.begin() + 1; it2 < search_server.end(); ++it2) {
        for (auto it1 = search_server.begin(); it1 < it2; ++it1) {
            if (search_server.GetWordFrequencies(*it1).size() != search_server.GetWordFrequencies(*it2).size()) {
                continue;
            }
            is_duplicate = true;
            for (const auto& [word, freqs] : search_server.GetWordFrequencies(*it2)) {
                if (!search_server.GetWordFrequencies(*it1).count(word)) {
                    is_duplicate = false;
                }
            }
            if (is_duplicate) {
                //std::cout << "document id - " << *it1 << std::endl;
                std::cout << "Found duplicate document id " << *it2 << std::endl;
                search_server.RemoveDocument(*it2);
                --it2;
            }
        }
    }
}
