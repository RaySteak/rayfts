#pragma once
#include <unordered_map>
#include <mutex>

template <typename K, typename T>
class lock_writable_unordered_map
{
private:
    std::mutex map_mutex;
    std::unordered_map<K, T> m;

public:
    typename std::unordered_map<K, T>::const_iterator find(K key);
    void insert(std::pair<K, T> elem);
    void erase(K key);
    void clear();
    typename std::unordered_map<K, T>::iterator begin();
    typename std::unordered_map<K, T>::iterator end();
    size_t size();
};

#include "lock_writable_unordered_map.tpp"