/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    icacheallocator.h
/// @brief   Custom STL allocator using lock-free memory pool
/// @details Provides a custom allocator for STL containers that uses
///          iFreeList for lock-free node caching, reducing heap allocations
///          and improving performance for container operations.
///
/// @par Performance Features:
/// - Lock-free memory pooling via iFreeList
/// - Reduced heap allocations for container nodes
/// - Automatic cleanup on destruction
/// - Thread-safe allocator operations
///
/// @par Usage:
/// std::list<T, iCacheAllocator<T>> myList;                    // Default 256
/// std::list<T, iCacheAllocator<T, 512>> myBigList;            // Custom 512
/// std::unordered_map<K, V, std::hash<K>, std::equal_to<K>, 
///                    iCacheAllocator<std::pair<const K, V>>> myMap;
///
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef ICACHEALLOCATOR_H
#define ICACHEALLOCATOR_H

#include <cstddef>
#include <memory>
#include <core/utils/ifreelist.h>
#include <core/utils/isharedptr.h>

namespace iShell {

/// @brief Custom STL allocator that uses lock-free memory pool for node caching
/// @tparam T The type of elements to allocate
/// @tparam MAXSIZE The maximum number of elements in the pool (default: 128)
///
/// @details This allocator wraps iFreeList to provide a standard-conforming
///          STL allocator interface. It maintains a shared memory pool that
///          is reference-counted, allowing multiple allocator instances to
///          share the same pool (required for STL container operations).
///
/// @par Memory Management:
/// - Single elements are cached in iFreeList (lock-free pool)
/// - Arrays (n > 1) fall back to system allocator
/// - Pool is shared via iSharedPtr for proper lifetime management
/// - Automatic cleanup when last allocator instance is destroyed
///
/// @par Thread Safety:
/// - allocate() and deallocate() are thread-safe via iFreeList
/// - Pool sharing is thread-safe via iSharedPtr
template<typename T, int MAXSIZE = 128>
class iCacheAllocator
{
private:
    /// Cleanup callback for iFreeList destruction
    /// Frees all cached memory blocks when pool is destroyed
    static void cleanup_pool(iFreeList<T*>* pool) {
        if (!pool) return;
        
        T* ptr;
        while ((ptr = pool->pop(IX_NULLPTR)) != IX_NULLPTR) {
            ::operator delete(ptr);
        }
    }
    
public:
    // Standard allocator type definitions
    typedef T value_type;
    typedef T* pointer;
    typedef const T* const_pointer;
    typedef T& reference;
    typedef const T& const_reference;
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;

    /// @brief Rebind allocator to different type (required by STL)
    template<typename U>
    struct rebind {
        typedef iCacheAllocator<U, MAXSIZE> other;
    };

    /// @brief Default constructor: creates new pool with shared_ptr
    /// @note Pool size is determined by template parameter MAXSIZE
    iCacheAllocator()
        : m_pool(new iFreeList<T*>(MAXSIZE, cleanup_pool)) {}

    /// @brief Copy constructor: shares the same pool via shared_ptr
    /// @param other The allocator to copy from
    iCacheAllocator(const iCacheAllocator& other)
        : m_pool(other.m_pool) {}

    /// @brief Rebind constructor: creates new pool for different type
    /// @tparam U The source allocator's element type
    /// @param other The allocator to rebind from (ignored, creates new pool)
    template<typename U>
    iCacheAllocator(const iCacheAllocator<U, MAXSIZE>& other)
        : m_pool(new iFreeList<T*>(MAXSIZE, cleanup_pool)) {
        IX_UNUSED(other);
    }

    /// @brief Allocate memory for n elements
    /// @param n Number of elements to allocate
    /// @return Pointer to allocated memory
    /// @note For n=1, tries to reuse from pool; for n>1, uses system allocator
    T* allocate(std::size_t n, const void* = IX_NULLPTR) {
        do {
            if (n > 1) break;

            // Try to pop from freelist (lock-free operation)
            T* ptr = m_pool->pop(IX_NULLPTR);
            if (ptr) return ptr;
        } while (false);

        // Fallback to system allocation for arrays or when pool is empty
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }
    
    void deallocate(pointer p, size_type n) {
        do {
            if (n > 1) break;
            if (m_pool->push(p)) return;
        } while (false);
        ::operator delete(p);
    }

    void construct(pointer p, const T& val) { new((void*)p) T(val); }
    
    // Support for C++11/Modern libc++ strict const-correctness
    template<typename U>
    void construct(U* p, const U& val) { new((void*)p) U(val); }

    void destroy(pointer p) { p->~T(); }

    template<typename U>
    void destroy(U* p) { p->~U(); }
    
    pointer address(reference x) const { return &x; }
    const_pointer address(const_reference x) const { return &x; }
    size_type max_size() const throw() { return size_t(-1) / sizeof(T); }

    /// @brief Same type allocators are equal if they share the same pool
    bool operator==(const iCacheAllocator& other) const {
        return m_pool == other.m_pool;  // Compare shared_ptr equality
    }

    /// @brief Same type allocators are not equal if they have different pools
    bool operator!=(const iCacheAllocator& other) const {
        return m_pool != other.m_pool;
    }

    /// @brief Different type/size allocators are never equal
    template<typename U, int S>
    bool operator==(const iCacheAllocator<U, S>& other) const { 
        IX_UNUSED(other);
        return false; 
    }
    
    /// @brief Different type/size allocators are always not equal
    template<typename U, int S>
    bool operator!=(const iCacheAllocator<U, S>& other) const { 
        IX_UNUSED(other);
        return true; 
    }

private:
    /// Shared pointer to the lock-free memory pool
    /// Multiple allocator instances can share the same pool
    iSharedPtr< iFreeList<T*> > m_pool;
};

} // namespace iShell

#endif // ICACHEALLOCATOR_H
