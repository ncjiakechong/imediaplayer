/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    icache.h
/// @brief   implements a generic cache with a maximum cost limit,
///          using a least-recently-used (LRU) eviction policy
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef ICACHE_H
#define ICACHE_H

#include <list>
#include <unordered_map>
#include <core/global/imacro.h>

namespace iShell {

template <class Key, class T, class Hash = std::hash<Key>>
class iCache
{
    struct Node {
        inline Node() : keyPtr(0) {}
        inline Node(T *data, int cost)
            : keyPtr(0), t(data), c(cost), p(0), n(0) {}
        const Key *keyPtr; T *t; int c; Node *p,*n;
    };
    Node *f, *l;

    typedef std::unordered_map<Key, Node, Hash> HashType;
    HashType hash;
    int mx, total;

    inline void unlink(Node &n) {
        if (n.p) n.p->n = n.n;
        if (n.n) n.n->p = n.p;
        if (l == &n) l = n.p;
        if (f == &n) f = n.n;
        total -= n.c;
        T *obj = n.t;

        typename HashType::iterator it = hash.find(*n.keyPtr);
        hash.erase(it);
        delete obj;
    }
    inline T *relink(const Key &key) {
        typename HashType::iterator i = hash.find(key);
        if (typename HashType::const_iterator(i) == hash.cend())
            return 0;

        Node &n = *i;
        if (f != &n) {
            if (n.p) n.p->n = n.n;
            if (n.n) n.n->p = n.p;
            if (l == &n) l = n.p;
            n.p = 0;
            n.n = f;
            f->p = &n;
            f = &n;
        }
        return n.t;
    }

    IX_DISABLE_COPY(iCache)

public:
    inline explicit iCache(int maxCost = 100);
    inline ~iCache() { clear(); }

    inline int maxCost() const { return mx; }
    void setMaxCost(int m);
    inline int totalCost() const { return total; }

    inline int size() const { return hash.size(); }
    inline int count() const { return hash.size(); }
    inline bool isEmpty() const { return hash.isEmpty(); }
    inline std::list<Key> keys() const { return hash.keys(); }

    void clear();

    bool insert(const Key &key, T *object, int cost = 1);
    T *object(const Key &key) const;
    inline bool contains(const Key &key) const { return hash.contains(key); }
    T *operator[](const Key &key) const;

    bool remove(const Key &key);
    T *take(const Key &key);

private:
    void trim(int m);
};

template <class Key, class T, class Hash>
inline iCache<Key, T,Hash>::iCache(int amaxCost)
    : f(0), l(0), mx(amaxCost), total(0) {}

template <class Key, class T, class Hash>
inline void iCache<Key,T,Hash>::clear()
{ while (f) { delete f->t; f = f->n; }
 hash.clear(); l = 0; total = 0; }

template <class Key, class T, class Hash>
inline void iCache<Key,T,Hash>::setMaxCost(int m)
{ mx = m; trim(mx); }

template <class Key, class T, class Hash>
inline T *iCache<Key,T,Hash>::object(const Key &key) const
{ return const_cast<iCache<Key,T>*>(this)->relink(key); }

template <class Key, class T, class Hash>
inline T *iCache<Key,T,Hash>::operator[](const Key &key) const
{ return object(key); }

template <class Key, class T, class Hash>
inline bool iCache<Key,T,Hash>::remove(const Key &key)
{
    typename HashType::iterator i = hash.find(key);
    if (typename HashType::const_iterator(i) == hash.cend()) {
        return false;
    } else {
        unlink(i->second);
        return true;
    }
}

template <class Key, class T, class Hash>
inline T *iCache<Key,T,Hash>::take(const Key &key)
{
    typename HashType::iterator i = hash.find(key);
    if (i == hash.end())
        return 0;

    Node& n = i->second;
    T *t = n.t;
    n.t = 0;
    unlink(n);
    return t;
}

template <class Key, class T, class Hash>
bool iCache<Key,T,Hash>::insert(const Key &akey, T *aobject, int acost)
{
    remove(akey);
    if (acost > mx) {
        delete aobject;
        return false;
    }
    trim(mx - acost);
    Node sn(aobject, acost);
    auto ret = hash.insert(std::make_pair(akey, sn));
    total += acost;
    Node *n = &ret.first->second;
    n->keyPtr = &ret.first->first;
    if (f) f->p = n;
    n->n = f;
    f = n;
    if (!l) l = f;
    return true;
}

template <class Key, class T, class Hash>
void iCache<Key,T,Hash>::trim(int m)
{
    Node *n = l;
    while (n && total > m) {
        Node *u = n;
        n = n->p;
        unlink(*u);
    }
}

} // namespace iShell

#endif // ICACHE_H
