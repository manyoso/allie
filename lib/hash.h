/*
  This file is part of Allie Chess.
  Copyright (C) 2018, 2019 Adam Treat

  Allie Chess is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Allie Chess is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Allie Chess.  If not, see <http://www.gnu.org/licenses/>.

  Additional permission under GNU GPL version 3 section 7
*/

#ifndef HASH_H
#define HASH_H

#include <QtGlobal>
#include <QtMath>
#include <QHash>

#include "node.h"
#include "options.h"

//#define DEBUG_HASH

template <class T>
extern quint64 fixedHash(const T &object);

template <class T>
class FixedSizeHash {
public:
    FixedSizeHash();
    ~FixedSizeHash();

    void reset(int positions);
    bool contains(quint64 hash) const;
    T *object(quint64 hash, bool relink);
    T *newObject(quint64 hash);
    void unlink(quint64 hash);
    void pin(quint64 hash);
    void unpin(quint64 hash);
    void clearPinned();
    float percentFull(int halfMoveNumber) const;
    int size() const { return m_size; }
    int used() const { return m_used; }
    int pinned() const { return m_numberPinned; }

private:
    struct ObjectInfo {
        inline ObjectInfo() :
            previous(nullptr),
            next(nullptr)
        {}
        ObjectInfo *previous;
        ObjectInfo *next;
        T object;
    };

    void clear();
    ObjectInfo* unlinkFromUsed();
    ObjectInfo* unlinkFromUnused();
    void linkToUsed(ObjectInfo &);
    void relinkToUsed(ObjectInfo &);
    void relinkToUnused(ObjectInfo &);
    void relinkToPinned(ObjectInfo &);
    void unlinkFromPinned(ObjectInfo&);
    bool isPinned(ObjectInfo&) const;
    ObjectInfo *m_first;
    ObjectInfo *m_last;
    ObjectInfo *m_unused;
    ObjectInfo *m_pinned;
    QHash<quint64, ObjectInfo*> m_hash;
    int m_size;
    int m_used;
    int m_numberPinned;
};

class Hash {
public:
    static Hash *globalInstance();

    void reset();
    float percentFull(int halfMoveNumber) const;
    int size() const;
    int used() const;
    int pinned() const;

    bool containsNode(quint64 hash) const;
    Node *node(quint64 hash, bool relink = false);
    Node *newNode(quint64 hash);
    void unlinkNode(quint64 hash);
    void pinNode(quint64 hash);
    void unpinNode(quint64 hash);
    void clearPinnedNodes();

    bool containsNodePosition(quint64 hash) const;
    Node::Position *nodePosition(quint64 hash, bool relink = false);
    Node::Position *newNodePosition(quint64 hash);
    void unlinkNodePosition(quint64 hash);

private:
    friend class MyHash;
    FixedSizeHash<Node> m_nodeHash;
    FixedSizeHash<Node::Position> m_positionHash;
};

template <class T>
inline FixedSizeHash<T>::FixedSizeHash()
    : m_first(nullptr),
    m_last(nullptr),
    m_unused(nullptr),
    m_pinned(nullptr),
    m_size(0),
    m_used(0),
    m_numberPinned(0)
{
}

template <class T>
inline FixedSizeHash<T>::~FixedSizeHash()
{
    clear();
}

template <class T>
inline void FixedSizeHash<T>::reset(int positions)
{
    clear();
    if (!positions)
        return;

    m_size = positions;
    for (int i = 0; i < m_size; ++i) {
        ObjectInfo *info = new ObjectInfo;
        if (m_unused) {
            info->next = m_unused;
            m_unused->previous = info;
        }
        m_unused = info;
        m_unused->previous = nullptr;
    }

    m_hash.reserve(m_size);
    Q_ASSERT(!m_first);
    Q_ASSERT(!m_last);
    Q_ASSERT(!m_pinned);
    Q_ASSERT(m_unused);

#if defined(DEBUG_HASH)
        quint64 bytes = positions * sizeof(ObjectInfo);
        qDebug() << "Hash size is" << bytes << "holding" << m_size << "max nodes";
#endif
}

template <class T>
inline void FixedSizeHash<T>::clear()
{
    int numberOfDeleted = 0;
    while (m_first) {
        ObjectInfo *delink = m_first;
        m_first = m_first->next;
        delete delink;
        ++numberOfDeleted;
    }

    while (m_unused) {
        ObjectInfo *delink = m_unused;
        m_unused = m_unused->next;
        delete delink;
        ++numberOfDeleted;
    }

    while (m_pinned) {
        ObjectInfo *delink = m_pinned;
        m_pinned = m_pinned->next;
        delete delink;
        ++numberOfDeleted;
    }

    Q_ASSERT(numberOfDeleted == m_size);
    m_first = nullptr;
    m_last = nullptr;
    m_unused = nullptr;
    m_pinned = nullptr;
    m_hash.clear();
    m_size = 0;
    m_used = 0;
    m_numberPinned = 0;
}

template <class T>
inline typename FixedSizeHash<T>::ObjectInfo* FixedSizeHash<T>::unlinkFromUsed()
{
    if (!m_last)
        return nullptr;

    Q_ASSERT(m_last);
    ObjectInfo &info = *m_last;
    Q_ASSERT(!info.next);
    Q_ASSERT(!isPinned(info));

    // Remove from actual hash
    if (!info.object.deinitialize())
        return nullptr;

    Q_ASSERT(m_hash.contains(fixedHash(info.object)));
    m_hash.remove(fixedHash(info.object));

    // Update first and last
    if (m_first == &info) {
        m_first = nullptr;
    } else {
        // Make previous the last
        Q_ASSERT(info.previous);
        m_last = info.previous;
        Q_ASSERT(!isPinned(*m_last));
        m_last->next = nullptr;
    }

    // Make naked
    info.previous = nullptr;
    info.next = nullptr;

    return &info;
}

template <class T>
inline typename FixedSizeHash<T>::ObjectInfo* FixedSizeHash<T>::unlinkFromUnused()
{
    if (!m_unused)
        return nullptr;

    ObjectInfo &info = *m_unused;
    Q_ASSERT(!info.previous);

    // Update unused
    m_unused = info.next;
    if (m_unused)
        m_unused->previous = nullptr;

    // Make naked
    info.previous = nullptr;
    info.next = nullptr;

    return &info;
}

template <class T>
inline void FixedSizeHash<T>::linkToUsed(ObjectInfo &info)
{
    // Update first
    Q_ASSERT(!info.previous);
    info.next = m_first;
    if (m_first) {
        Q_ASSERT(!m_first->previous);
        m_first->previous = &info;
    }
    m_first = &info;

    // Update last
    if (!m_last) {
        m_last = m_first;
        Q_ASSERT(!isPinned(*m_last));
    }
}

template <class T>
inline void FixedSizeHash<T>::relinkToUsed(ObjectInfo &info)
{
    // If info is already first, then nothing to be done
    if (m_first == &info)
        return;

    // Link together info's previous and next
    if (info.previous)
        info.previous->next = info.next;
    if (info.next)
        info.next->previous = info.previous;

    // Update last
    if (m_last == &info) {
        m_last = info.previous;
        Q_ASSERT(!isPinned(*m_last));
    }

    // Update first
    info.previous = nullptr;
    info.next = m_first;
    Q_ASSERT(m_first);
    m_first->previous = &info;
    m_first = &info;
}

template <class T>
inline void FixedSizeHash<T>::relinkToUnused(ObjectInfo &info)
{
    // Remove from actual hash
    bool success = info.object.deinitialize();
    Q_ASSERT(success);
    Q_ASSERT(m_hash.contains(fixedHash(info.object)));
    m_hash.remove(fixedHash(info.object));

    // Possibly update first and last
    if (m_first == &info) {
        m_first = info.next;
        Q_ASSERT(!isPinned(*m_first));
    }

    if (m_last == &info) {
        m_last = info.previous;
        Q_ASSERT(!isPinned(*m_last));
    }

    // Link together info's previous and next
    if (info.previous)
        info.previous->next = info.next;
    if (info.next)
        info.next->previous = info.previous;

    // Update unused
    info.previous = nullptr;
    info.next = m_unused;
    if (m_unused) {
        Q_ASSERT(!m_unused->previous);
        m_unused->previous = &info;
    }
    m_unused = &info;
}

template <class T>
inline void FixedSizeHash<T>::relinkToPinned(ObjectInfo &info)
{
    // Possibly update first and last
    if (m_first == &info) {
        m_first = info.next;
        Q_ASSERT(!isPinned(*m_first));
    }

    if (m_last == &info) {
        m_last = info.previous;
        Q_ASSERT(!isPinned(*m_last));
    }

    // Link together info's previous and next
    if (info.previous)
        info.previous->next = info.next;
    if (info.next)
        info.next->previous = info.previous;

    // Update pinned
    info.previous = nullptr;
    info.next = m_pinned;
    if (m_pinned) {
        Q_ASSERT(!m_pinned->previous);
        m_pinned->previous = &info;
    }
    m_pinned = &info;
}

template <class T>
inline void FixedSizeHash<T>::unlinkFromPinned(ObjectInfo &info)
{
    if (m_pinned == &info) {
        Q_ASSERT(m_pinned);
        ObjectInfo &info = *m_pinned;
        Q_ASSERT(!info.previous);

        // Update pinned
        m_pinned = info.next;
        if (m_pinned)
            m_pinned->previous = nullptr;
    }

    // Link together info's previous and next
    if (info.previous)
        info.previous->next = info.next;
    if (info.next)
        info.next->previous = info.previous;

    // Make naked
    info.previous = nullptr;
    info.next = nullptr;
}

template <class T>
bool FixedSizeHash<T>::isPinned(ObjectInfo &info) const
{
    ObjectInfo *pinned = m_pinned;
    while (pinned) {
        if (pinned == &info)
            return true;
        pinned = pinned->next;
    }
    return false;
}

template <class T>
inline bool FixedSizeHash<T>::contains(quint64 hash) const
{
    Q_ASSERT(m_size);
    return m_hash.contains(hash);
}

template <class T>
inline T *FixedSizeHash<T>::object(quint64 hash, bool update)
{
    Q_ASSERT(m_size);
    Q_ASSERT(m_hash.contains(hash));

    ObjectInfo *info = m_hash.value(hash);
    if (update && !isPinned(*info))
        relinkToUsed(*info);
    return &(info->object);
}

template <class T>
inline T *FixedSizeHash<T>::newObject(quint64 hash)
{
    Q_ASSERT(m_size);
    Q_ASSERT(!m_hash.contains(hash));

    ObjectInfo *info = nullptr;
    if (m_unused) {
        ++m_used;
        info = unlinkFromUnused();
    } else {
        info = unlinkFromUsed();
    }

    if (!info)
        return nullptr;

    Q_ASSERT(!isPinned(*info));
    m_hash.insert(hash, info);
    linkToUsed(*info);
    return &(info->object);
}

template <class T>
inline void FixedSizeHash<T>::unlink(quint64 hash)
{
    Q_ASSERT(m_size);
    Q_ASSERT(m_hash.contains(hash));

    ObjectInfo *info = m_hash.value(hash);
    relinkToUnused(*info);
    --m_used;
}

template <class T>
inline void FixedSizeHash<T>::pin(quint64 hash)
{
    Q_ASSERT(m_size);
    Q_ASSERT(m_hash.contains(hash));
    Q_ASSERT(m_numberPinned < m_size);

    ObjectInfo *info = m_hash.value(hash);
    relinkToPinned(*info);
    ++m_numberPinned;
}

template <class T>
inline void FixedSizeHash<T>::unpin(quint64 hash)
{
    Q_ASSERT(m_size);
    Q_ASSERT(m_hash.contains(hash));

    ObjectInfo *info = m_hash.value(hash);
    unlinkFromPinned(*info);
    linkToUsed(*info);
    --m_numberPinned;
}

template <class T>
inline void FixedSizeHash<T>::clearPinned()
{
    Q_ASSERT(m_size);
    while (m_pinned) {
        ObjectInfo *delink = m_pinned;
        m_pinned = m_pinned->next;
        unpin(fixedHash(delink->object));
    }
    m_numberPinned = 0;
}

template <class T>
inline float FixedSizeHash<T>::percentFull(int halfMoveNumber) const
{
    Q_ASSERT(m_size);
    Q_UNUSED(halfMoveNumber);
    return quint64(m_used) / float(m_size);
}

inline void Hash::reset()
{
    int positions = Options::globalInstance()->option("Hash").value().toInt();
    m_nodeHash.reset(positions);
    m_positionHash.reset(positions);
}

inline float Hash::percentFull(int halfMoveNumber) const
{
    return m_nodeHash.percentFull(halfMoveNumber);
}

inline int Hash::size() const
{
    Q_ASSERT(m_positionHash.size() == m_nodeHash.size());
    return m_nodeHash.size();
}

inline int Hash::used() const
{
    Q_ASSERT(m_positionHash.used() <= m_nodeHash.size());
    return m_nodeHash.used();
}

inline int Hash::pinned() const
{
    return m_nodeHash.pinned();
}

inline bool Hash::containsNode(quint64 hash) const
{
    return m_nodeHash.contains(hash);
}

inline Node *Hash::node(quint64 hash, bool relink)
{
    return m_nodeHash.object(hash, relink);
}

inline Node *Hash::newNode(quint64 hash)
{
    return m_nodeHash.newObject(hash);
}

inline void Hash::unlinkNode(quint64 hash)
{
    m_nodeHash.unlink(hash);
}

inline void Hash::pinNode(quint64 hash)
{
    m_nodeHash.pin(hash);
}

inline void Hash::unpinNode(quint64 hash)
{
    m_nodeHash.unpin(hash);
}

inline void Hash::clearPinnedNodes()
{
    m_nodeHash.clearPinned();
}

inline bool Hash::containsNodePosition(quint64 hash) const
{
    return m_positionHash.contains(hash);
}

inline Node::Position *Hash::nodePosition(quint64 hash, bool relink)
{
    return m_positionHash.object(hash, relink);
}

inline Node::Position *Hash::newNodePosition(quint64 hash)
{
    return m_positionHash.newObject(hash);
}

inline void Hash::unlinkNodePosition(quint64 hash)
{
    m_positionHash.unlink(hash);
}

#endif // HASH_H
