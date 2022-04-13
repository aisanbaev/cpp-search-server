#pragma once

#include <map>
#include <vector>
#include <mutex>

template <typename Key, typename Value>
class ConcurrentMap {
private:
    using Bucket = std::pair<std::mutex, std::map<Key, Value>>;
    std::vector<Bucket> buckets_;

public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct Access {
        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;

        Access(const Key& key, Bucket& bucket)
            : guard(bucket.first)
            , ref_to_value(bucket.second[key]) {
        }
    };

    explicit ConcurrentMap(size_t bucket_count)
        : buckets_(bucket_count) {
    }

    Access operator[](const Key& key) {
        uint64_t index = key % buckets_.size();
        return {key, buckets_[index]};
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;

        for (auto& bucket : buckets_) {
            std::lock_guard g(bucket.first);
            result.merge(bucket.second);
        }

        return result;
    }
};