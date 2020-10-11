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
class SmartCacheImpl
{
    struct Item{
        int priority;
        TKey key;
        TValue val;
        
        Item(int p, TKey k, TValue v){
            priority = p;
            key = k;
            val = v;
        }
    };
    
private:
    int m_cacheSize;

    std::list<Item *> m_elementSequence;

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
inline TValue* SmartCacheImpl<TKey, TValue>::Get(TKey key)
{
    if (!Contains(key))
    {
        return nullptr;
    }

    auto policy = m_config.EvictionPolicy;

    if (policy == SmartCache::CacheEvictionPolicy::LeastFrequentlyUsed)
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
    else if (policy == SmartCache::CacheEvictionPolicy::LeastRecentlyUsed || 
                policy == SmartCache::CacheEvictionPolicy::MostRecentlyUsed)
    {
        auto lookupItr = m_lookupTable.find(key);

        // Move the element to the beginning of the queue.
        //
        m_elementSequence.emplace_front(lookupItr->second);
        m_elementSequence.pop_back();
        return &(m_elementSequence.front()->val);
    }
    else
    {
        throw std::exception();
    }
}

template<typename TKey, typename TValue>
inline void SmartCacheImpl<TKey, TValue>::Push(TKey key, const TValue value)
{

    auto policy = m_config.EvictionPolicy;

    if (policy == SmartCache::CacheEvictionPolicy::LeastFrequentlyUsed)
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
    else if (policy == SmartCache::CacheEvictionPolicy::LeastRecentlyUsed || 
                policy == SmartCache::CacheEvictionPolicy::MostRecentlyUsed)
    {
        // Find the element ref in the lookup table.
    //
        auto lookupItr = m_lookupTable.find(key);

        if (lookupItr == m_lookupTable.end())
        {
            if (m_elementSequence.size() == m_cacheSize)
            {
                // We reached the maximum cache size, evict the element based on the current policy.
                //
                if (m_config.EvictionPolicy == SmartCache::CacheEvictionPolicy::LeastRecentlyUsed)
                {
                    auto evictedLookupItr = m_elementSequence.back();
                    auto oldkey = evictedLookupItr->key;
                    m_elementSequence.pop_back();
                    m_lookupTable.erase(oldkey);
                }
                else if (m_config.EvictionPolicy == SmartCache::CacheEvictionPolicy::MostRecentlyUsed)
                {
                    auto evictedLookupItr = m_elementSequence.front();
                    auto oldkey = evictedLookupItr->key;
                    m_elementSequence.pop_front();
                    m_lookupTable.erase(oldkey);
                }
                else
                {
                    // Unknown policy.
                    //
                    throw std::exception();
                }
            }
            
            Item * item = new Item(1, key, value);
            m_elementSequence.emplace_front(item);
            // auto elementItr = m_elementSequence.begin();

            m_lookupTable.emplace(key, item);
        }
        else
        {
            // Don't do anything. 
            // Push the same value won't have any effect.
            // This is for the sake of performance. 
        }

        return;
    }
    else
    {
        throw std::exception();
    }    
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
