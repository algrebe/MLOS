//*********************************************************************
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root
// for license information.
//
// @File: Workloads.cpp
//
// Purpose:
//      <description>
//
// Notes:
//      <special-instructions>
//
//*********************************************************************

#include <iostream>
#include "stdafx.h"
#include "SmartCacheImpl.h"
#include "Workloads.h"

inline void GetPut(SmartCacheImpl<int32_t, int32_t>& smartCache, int32_t i)
{
    int32_t* element = smartCache.Get(i);
    if (element == nullptr)
    {
        smartCache.Push(i, i);
    }
}


uint64_t CyclicalWorkload(uint64_t sequenceNumber, SmartCacheImpl<int32_t, int32_t>& smartCache)
{
    for (int32_t i = 1; i < sequenceNumber; i++)
    {
        GetPut(smartCache, i);
    }

    return 1;
}

uint64_t ElevatorWorkload(uint64_t sequenceNumber, SmartCacheImpl<int32_t, int32_t>& smartCache)
{
    int32_t i = 1;
    
    for (; i < sequenceNumber; i++)
    {
        GetPut(smartCache, i);
    }

    for (; i >= 1; i--)
    {
        GetPut(smartCache, i);
    }

    return 1;
}


uint64_t LFUFriendlyWorkload(SmartCacheImpl<int32_t, int32_t>& smartCache, int32_t cacheSize, int freq, int patternRep)
{
    std::cout << "Running workload with size " << cacheSize << std::endl;
    for(int32_t i=0; i < cacheSize; i++) 
    {
        for(int j=0; j < freq; j++) 
        {
            GetPut(smartCache, i);
        }
    }

    int32_t accessedOnce = cacheSize;
    GetPut(smartCache, accessedOnce);

    accessedOnce++;
    for(int32_t i=0; i < patternRep; i++)
    {
        for(int32_t j=1; j < cacheSize; j++)
        {
            GetPut(smartCache, j);
            for(int32_t k=0; k < cacheSize; k++)
            {
                GetPut(smartCache, accessedOnce);
                accessedOnce++;
            }
        }
    }

    return 1;
}
