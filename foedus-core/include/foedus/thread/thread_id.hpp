/*
 * Copyright (c) 2014, Hewlett-Packard Development Company, LP.
 * The license and distribution terms for this file are placed in LICENSE.txt.
 */
#ifndef FOEDUS_THREAD_THREAD_ID_HPP_
#define FOEDUS_THREAD_THREAD_ID_HPP_
#include <foedus/cxx11.hpp>
#include <stdint.h>
/**
 * @file foedus/thread/thread_id.hpp
 * @brief Typedefs of ID types used in thread package.
 * @ingroup THREAD
 */
namespace foedus {
namespace thread {
/**
 * @typedef ThreadGroupId
 * @brief Typedef for an ID of ThreadGroup (NUMA node).
 * @ingroup THREAD
 * @details
 * Currently, we assume there are at most 256 NUMA nodes.
 */
typedef uint8_t ThreadGroupId;

#ifndef DISABLE_CXX11_IN_PUBLIC_HEADERS
CXX11_STATIC_ASSERT (sizeof(ThreadGroupId) == 1, "Max NUMA node count must be 1 byte");
#endif  // DISABLE_CXX11_IN_PUBLIC_HEADERS

/**
 * @var kMaxThreadGroupId
 * @brief Maximum possible value of ThreadGroupId.
 * @ingroup THREAD
 */
const ThreadGroupId kMaxThreadGroupId = 0xFF;

/**
 * @typedef ThreadLocalOrdinal
 * @brief Typedef for a \b local ID of Thread (core), which is \b NOT unique across NUMA nodes.
 * @ingroup THREAD
 * @details
 * Currently, we assume there are at most 256 cores per NUMA node.
 */
typedef uint8_t ThreadLocalOrdinal;

#ifndef DISABLE_CXX11_IN_PUBLIC_HEADERS
CXX11_STATIC_ASSERT (sizeof(ThreadLocalOrdinal) == 1, "Max core-per-node must be 1 byte");
#endif  // DISABLE_CXX11_IN_PUBLIC_HEADERS

/**
 * @var kMaxThreadLocalOrdinal
 * @brief Maximum possible value of ThreadLocalOrdinal.
 * @ingroup THREAD
 */
const ThreadLocalOrdinal kMaxThreadLocalOrdinal = 0xFF;

/**
 * @typedef ThreadId
 * @brief Typedef for a \b global ID of Thread (core), which is unique across NUMA nodes.
 * @ingroup THREAD
 * @details
 * This is a composite of ThreadGroupId (high 1 byte) and ThreadLocalOrdinal (low 1 byte).
 * For example, if there are 2 NUMA nodes and 8 cores each:
 * thread-0=node-0 core-0, thread-1=node-0 core-1, ..., thread-256=node-1 core-0,...
 */
typedef uint16_t ThreadId;

#ifndef DISABLE_CXX11_IN_PUBLIC_HEADERS
CXX11_STATIC_ASSERT (sizeof(ThreadId) == 2, "Max thread count must be 2 bytes");
#endif  // DISABLE_CXX11_IN_PUBLIC_HEADERS

/**
 * @var kMaxThreadId
 * @brief Maximum possible value of ThreadId.
 * @ingroup THREAD
 */
const ThreadId kMaxThreadId = 0xFFFF;

/**
 * Returns a globally unique ID of Thread (core) for the given node and ordinal in the node.
 * @ingroup THREAD
 */
inline ThreadId compose_thread_id(ThreadGroupId node, ThreadLocalOrdinal local_core) {
    return (node << 8) | local_core;
}

/**
 * Extracts NUMA node ID from the given globally unique ID of Thread (core).
 * @ingroup THREAD
 */
inline ThreadGroupId decompose_numa_node(ThreadId global_id) {
    return (global_id >> 8) & 0xFF;
}

/**
 * Extracts local ordinal from the given globally unique ID of Thread (core).
 * @ingroup THREAD
 */
inline ThreadLocalOrdinal decompose_numa_local_ordinal(ThreadId global_id) {
    return global_id & 0xFF;
}

/**
 * @brief Used as a general timeout parameter (in microseconds) for synchronous methods.
 * @details
 * If the method had to wait for this length, it gives up and returns a failure.
 * Negative value means forever. 0 means no wait, in other words it's \e conditional (we
 * execute the function on the condition of immediate availability).
 */
typedef int64_t TimeoutMicrosec;

}  // namespace thread
}  // namespace foedus
#endif  // FOEDUS_THREAD_THREAD_ID_HPP_
