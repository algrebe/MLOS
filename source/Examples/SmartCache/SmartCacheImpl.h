//*********************************************************************
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root
// for license information.
//
// @File: SmartCacheImpl.h
//
// Purpose:
//      <description>
//
// Notes:
//      <special-instructions>
//
//*********************************************************************

#pragma once

template<typename TKey, typename TValue>
struct Item_{
    int priority;
    TKey key;
    TValue val;
    
    Item_(int p, TKey k, TValue v){
        priority = p;
        key = k;
        val = v;
    }
    
};

template<typename TKey, typename TValue>
class SmartCacheImpl
{

    typedef Item_<TKey, TValue> Item;
    
private:
    int m_cacheSize;

    std::list<TValue> m_elementSequence;

    std::vector<Item *> m_heap;

    std::unordered_map<TKey, Item *> m_lookupTable;

    // Mlos Tunable Component Config.
    //
    Mlos::Core::ComponentConfig<SmartCache::SmartCacheConfig>& m_config;

public:
    SmartCacheImpl(Mlos::Core::ComponentConfig<SmartCache::SmartCacheConfig>& config);
    ~SmartCacheImpl();

    bool Contains(TKey key);
    TValue* Get(TKey key);
    TValue* Get_LFU(TKey key);
    TValue* Get_LRUMRU(TKey key);
    void Push(TKey key, const TValue value);

    void Reconfigure();
};

template<typename TKey, typename TValue>
inline SmartCacheImpl<TKey, TValue>::SmartCacheImpl(Mlos::Core::ComponentConfig<SmartCache::SmartCacheConfig>& config)
  : m_config(config)
{
    m_cacheSize = 0;

    // Apply initial configuration.
    //
    Reconfigure();
}

template<class K, class V>
SmartCacheImpl<K, V>::~SmartCacheImpl()
{
}

template<typename TKey, typename TValue>
inline bool SmartCacheImpl<TKey, TValue>::Contains(TKey key)
{
    bool isInCache = m_lookupTable.find(key) != m_lookupTable.end();

    SmartCache::CacheRequestEventMessage msg;
    msg.ConfigId = m_config.ConfigId;
    msg.Key = key;
    msg.IsInCache = isInCache;

    m_config.SendTelemetryMessage(msg);

    return isInCache;
}

template<typename TKey, typename TValue>
inline TValue* SmartCacheImpl<TKey, TValue>::Get_LRUMRU(TKey key)
{
    auto lookupItr = m_lookupTable.find(key);

    // Move the element to the beginning of the queue.
    //
    m_elementSequence.emplace_front(*lookupItr->second);
    m_elementSequence.erase(lookupItr->second);

    // As we moved the element, we need to update the element ref.
    //
    lookupItr->second = m_elementSequence.begin();

    return &m_elementSequence.front();
}

template<typename TKey, typename TValue>
inline TValue* SmartCacheImpl<TKey, TValue>::Get(TKey key)
{
    if (!Contains(key))
    {
        return nullptr;
    }

    auto policy = m_config.EvictionPolicy;

    if (policy == SmartCache::CacheEvictionPolicy::CacheEvictionPolicy.LeastFrequentlyUsed)
    {
        return Get_LFU(key);
    }
    else if (policy == SmartCache::CacheEvictionPolicy::CacheEvictionPolicy.LeastRecentlyUsed || 
                policy == SmartCache::CacheEvictionPolicy::CacheEvictionPolicy.MostRecentlyUsed)
    {
        return Get_LRUMRU(key);
    }
    else
    {
        throw std::exception();
    }
}

template<typename TKey, typename TValue>
inline TValue* SmartCacheImpl<TKey, TValue>::Get_LFU(TKey key)
{
    auto lookupItr = m_lookupTable.find(key);
    
    // Every time we get an element, 
    // the priority bumped up +1.
    //
    Item * item = lookupItr->second;
    item->priority += 1;
    std::make_heap(m_heap.begin(), m_heap.end(), [](Item * lhs, Item * rhs){
        return lhs->priority > rhs->priority;
    });
    return & item->val;
}

template<typename TKey, typename TValue>
inline void SmartCacheImpl<TKey, TValue>::Push(TKey key, const TValue value)
{
    // Find the element ref in the lookup table.
    //
    auto lookupItr = m_lookupTable.find(key);

    if (lookupItr == m_lookupTable.end())
    {
        if (m_lookupTable.size() == m_cacheSize)
        {
            std::pop_heap(m_heap.begin(), m_heap.end());
            
            Item * old = m_heap.back();
            
            m_heap.pop_back();
            
            // std::cout << "[EVICT] (" << old->key << ", " << old->val << ")" << std::endl;
            
            m_lookupTable.erase(old->key);
            
            delete old;
            
        }

        Item * item = new Item(1, key, value);
        
        m_lookupTable.insert({key, item});
        
        m_heap.push_back(item);
        
        std::make_heap(m_heap.begin(), m_heap.end(), [](Item * lhs, Item * rhs){
            return lhs->priority > rhs->priority;
        });

    }
    else
    {
        // Not do anything because user should have modify the value by using Get().
    }

    return;
}

template<typename TKey, typename TValue>
inline void SmartCacheImpl<TKey, TValue>::Reconfigure()
{
    // Update the cache size from the latest configuration available in shared memory.
    //
    m_cacheSize = m_config.CacheSize;

    // Clear the cache.
    //
    m_elementSequence.clear();
    m_heap.clear();
    m_lookupTable.clear();

    // Adjust the number of buckets reserved for the cache (relative to the
    // max_load_factor) to match the new size.
    //
    m_lookupTable.reserve(m_cacheSize);
}
