//*********************************************************************
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See License.txt in the project root
// for license information.
//
// @File: Workloads.h
//
// Purpose:
//      <description>
//
// Notes:
//      <special-instructions>
//
//*********************************************************************

#pragma once

uint64_t CyclicalWorkload(uint64_t sequenceNumber, SmartCacheImpl<int32_t, int32_t>& smartCache);
uint64_t LFUFriendlyWorkload(SmartCacheImpl<int32_t, int32_t>& smartCache, int32_t cacheSize, int freq, int patternRep);
uint64_t ElevatorWorkload(uint64_t sequenceNumber, SmartCacheImpl<int32_t, int32_t>& smartCache);
