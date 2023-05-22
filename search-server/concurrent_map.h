#pragma once


#include <map>
#include <mutex>
#include <type_traits>
#include <vector>

template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

    struct Access {        
        std::lock_guard<std::mutex> guard;   
        Value& ref_to_value;        
    };

    explicit ConcurrentMap(size_t bucket_count);

    Access operator[](const Key& key);

    std::map<Key, Value> BuildOrdinaryMap();
    void erase(const Key& key);
    
private:
    size_t bucket_count_;
    std::vector<std::map<Key, Value>> maps_;
    std::vector<std::mutex> map_mutexes_;
};

template<typename Key, typename Value>
ConcurrentMap<Key, Value>::ConcurrentMap(size_t bucket_count):
    bucket_count_{bucket_count},
    maps_{bucket_count},
    map_mutexes_{bucket_count}
{    
}

template<typename Key, typename Value>
typename ConcurrentMap<Key, Value>::Access ConcurrentMap<Key, Value>::operator[](
        const Key& key)
{
    const uint64_t bucket_index{static_cast<uint64_t>(key) % bucket_count_};  
    
    return Access{std::lock_guard<std::mutex>{map_mutexes_[bucket_index]}, 
                 maps_[bucket_index][key]};
}

template<typename Key, typename Value>
std::map<Key, Value> ConcurrentMap<Key, Value>::BuildOrdinaryMap()
{
    std::map<Key, Value> out;
    
    for(size_t i{0}; i < bucket_count_; ++i)
    {
        std::lock_guard<std::mutex> guard{map_mutexes_[i]};
        const auto& item{maps_[i]};
        out.insert(item.begin(), item.end());
    }
    
    return out;
}

template<typename Key, typename Value>
void ConcurrentMap<Key, Value>::erase(const Key& key)
{
    const uint64_t bucket_index{static_cast<uint64_t>(key) % bucket_count_};  
    
    {
        std::lock_guard<std::mutex>{map_mutexes_[bucket_index]};
        maps_[bucket_index].erase(key);
    }    
}
