
template <typename K, typename T>
typename std::unordered_map<K, T>::const_iterator lock_writable_unordered_map<K, T>::find(K key)
{
    std::lock_guard<std::mutex> lk_grd(map_mutex);
    return m.find(key);
}

template <typename K, typename T>
void lock_writable_unordered_map<K, T>::insert(std::pair<K, T> elem)
{
    std::lock_guard<std::mutex> lk_grd(map_mutex);
    m.insert(elem);
}

template <typename K, typename T>
void lock_writable_unordered_map<K, T>::erase(K key)
{
    std::lock_guard<std::mutex> lk_grd(map_mutex);
    m.erase(key);
}

template <typename K, typename T>
typename std::unordered_map<K, T>::iterator lock_writable_unordered_map<K, T>::begin()
{
    std::lock_guard<std::mutex> lk_grd(map_mutex);
    return m.begin();
}

template <typename K, typename T>
typename std::unordered_map<K, T>::iterator lock_writable_unordered_map<K, T>::end()
{
    std::lock_guard<std::mutex> lk_grd(map_mutex);
    return m.end();
}

template <typename K, typename T>
size_t lock_writable_unordered_map<K, T>::size()
{
    return m.size();
}