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

#include "hash.h"

#include <QtMath>

#include "node.h"
#include "neural/nn_policy.h"
#include "options.h"

//#define DEBUG_HASH
#define MAX_POTENTIALS_COUNT 37 // gives 32768 entries while increasing by one cuts that by half

struct HashPValue {
    float pValue = -2.0f;
    qint32 index = -1;
};

quint64 pValueToHash(const HashPValue &hashPValue)
{
    Q_ASSERT(sizeof(HashPValue) == 8);
    quint32 pValue;
    memcpy(&pValue, &hashPValue.pValue, sizeof(hashPValue.pValue));
    return quint64(pValue) << 32 | quint64(hashPValue.index);
}

HashPValue pValueFromHash(quint64 hash)
{
    HashPValue hashPValue;
    quint32 pValue = quint32((hash >> 32) & 0xFFFFFFFF);
    memcpy(&hashPValue.pValue, &pValue, sizeof(pValue));
    hashPValue.index = quint16(hash & 0xFFFFFFFF);
    return hashPValue;
}

struct HashEntry {
    float qValue = -2.0f;
    quint64 potentialValues[MAX_POTENTIALS_COUNT];
};

struct HashNode {
    quint64 key = 0;
    qint8 generation = -1;
    HashEntry value;
};

struct HashBucket {
    std::atomic<HashNode> first;
    std::atomic<HashNode> second;
};

class MyHash : public Hash { };
Q_GLOBAL_STATIC(MyHash, HashInstance)
Hash* Hash::globalInstance()
{
    return HashInstance();
}

Hash::Hash()
    : m_size(0),
      m_table(nullptr)
{
    Q_ASSERT(sizeof(HashPValue) == 8);

#if !defined(QT_NO_DEBUG)
    HashPValue pValue;
    pValue.pValue = 42.42f;
    pValue.index = 42;

    quint64 hash = pValueToHash(pValue);
    HashPValue newPValue = pValueFromHash(hash);
    bool pValsMatch = qFuzzyCompare(pValue.pValue, newPValue.pValue);
    bool indexesMatch = pValue.index == newPValue.index;
    if (!pValsMatch || !indexesMatch) {
        qDebug() << "pVals:" << pValue.pValue << "," << newPValue.pValue;
        qDebug() << "indexes:" << pValue.index << "," << newPValue.index;
    }
    Q_ASSERT(pValsMatch);
    Q_ASSERT(indexesMatch);
#endif
}

Hash::~Hash()
{
    delete[] m_table;
}

quint64 largestPowerofTwoLessThan(quint64 n)
{
   quint64 p = quint64(qLn(n) / qLn(2));
   return quint64(pow(2, p));
}

void Hash::reset()
{
    quint64 bytes = Options::globalInstance()->option("Hash").value().toUInt() * quint64(1024) * quint64(1024);
    quint64 maxSize = bytes / sizeof(HashBucket);
    quint64 size = largestPowerofTwoLessThan(maxSize);
    if (!m_table || m_size != size) {
        delete[] m_table;
        m_size = size;
        m_table = new HashBucket[m_size];
#if defined(DEBUG_HASH)
        qDebug() << "Hash size is" << m_size;
#endif
    }

    clear();
}

void Hash::clear()
{
    memset(m_table, 0, sizeof(m_table) * m_size);
}

bool Hash::contains(const Node *node) const
{
    if (!m_size)
        return false;

    const quint64 hash = node->game().hash();
    const quint64 index = hash & (m_size - 1);
    const HashBucket *bucket = m_table + index;
    Q_ASSERT(bucket);
    if (!bucket)
        return false;
    if (bucket->first.load().key == hash)
        return true;
    if (bucket->second.load().key == hash)
        return true;
    return false;
}

bool fillOutNodeFromEntry(Node *node, const HashEntry &entry)
{
    Q_ASSERT(!qFuzzyCompare(entry.qValue, -2.0f));
    Q_ASSERT(!node->hasRawQValue());
    node->setRawQValue(entry.qValue);
    Q_ASSERT((node->hasPotentials()) || node->isCheckMate() || node->isStaleMate());
    if (!node->hasPotentials())
        return true;

    QHash<qint32, float> pVals;
    for (int i = 0; i < MAX_POTENTIALS_COUNT; ++i) {
        HashPValue pVal = pValueFromHash(entry.potentialValues[i]);
        if (pVal.index != -1)
            pVals.insert(pVal.index, pVal.pValue);
    }
    Q_ASSERT(!pVals.isEmpty());
    for (PotentialNode *potential : node->potentials()) {
        Q_ASSERT(!potential->hasPValue());
        const float pValue = pVals.value(moveToNNIndex(potential->move()));
        Q_ASSERT(!qFuzzyCompare(pValue, -2.0f));
        potential->setPValue(pValue);
    }

    return true;
}

bool Hash::fillOut(Node *node) const
{
    Q_ASSERT(m_size);
    const quint64 hash = node->game().hash();
    const quint64 index = hash & (m_size - 1);
    const HashBucket *bucket = m_table + index;
    Q_ASSERT(bucket);
    if (!bucket)
        return false;
    {
        HashNode hashNode = bucket->first.load();
        if (hashNode.key == hash)
            return fillOutNodeFromEntry(node, hashNode.value);
    }
    {
        HashNode hashNode = bucket->second.load();
        if (hashNode.key == hash)
            return fillOutNodeFromEntry(node, hashNode.value);
    }
    return false;
}

void Hash::insert(const Node *node)
{
    if (!m_size)
        return;

    const quint64 hash = node->game().hash();
    const quint64 index = hash & (m_size - 1);
    HashBucket *bucket = m_table + index;
    Q_ASSERT(bucket);
    if (!bucket)
        return;

    if (node->potentials().count() > MAX_POTENTIALS_COUNT)
        return; // Too many potentials to cache!

    HashEntry entry;
    entry.qValue = node->rawQValue();
    Q_ASSERT(!qFuzzyCompare(entry.qValue, -2.0f));

    const QVector<PotentialNode*> po = node->potentials();
    for (int i = 0; i < po.count(); ++i) {
        PotentialNode *potential = po.at(i);
        HashPValue pValue;
        pValue.pValue = potential->pValue();
        Q_ASSERT(!qFuzzyCompare(potential->pValue(), -2.0f));
        pValue.index = moveToNNIndex(potential->move());
        entry.potentialValues[i] = pValueToHash(pValue);
    }

    for (int i = po.count(); i < MAX_POTENTIALS_COUNT; ++i)
        entry.potentialValues[i] = pValueToHash(HashPValue());

    HashNode hashNode;
    hashNode.key = hash;
    hashNode.value = entry;
    hashNode.generation = qint8(node->rootNode()->game().halfMoveNumber());

    // See if the first bucket is empty and if so use that
    HashNode first = bucket->first.load();
    if (qFuzzyCompare(first.value.qValue, -2.0f)) {
        bucket->first.store(hashNode);
        return;
    }

    // See if the second bucket is empty and if so use that
    HashNode second = bucket->second.load();
    if (qFuzzyCompare(second.value.qValue, -2.0f)) {
        bucket->second.store(hashNode);
        return;
    }

    // See if the first bucket is older and if so use that
    bool firstIsOlder = first.generation < second.generation;
    if (firstIsOlder) {
        bucket->first.store(hashNode);
        return;
    }

    // Just use the second bucket
    bucket->second.store(hashNode);
}

float Hash::percentFull(int halfMoveNumber) const
{
    if (!m_size)
        return 1.0f;

#if defined(DEBUG_HASH)
    quint64 sample = m_size;
#else
    quint64 sample = 1000;
#endif
    quint64 touchedFirst = 0;
    quint64 touchedSecond = 0;
    QVector<quint64> indexesTouched;
    for (quint64 i = 0; i < sample; ++i) {
        quint64 index = i;
        HashBucket *bucket = m_table + index;
        {
            HashNode hashNode = bucket->first.load();
            if (hashNode.generation == halfMoveNumber) {
                touchedFirst++;
                indexesTouched.append(index);
            }
        }
        {
            HashNode hashNode = bucket->second.load();
            if (hashNode.generation == halfMoveNumber) {
                touchedSecond++;
                indexesTouched.append(index);
            }
        }
    }
    if (!(touchedFirst + touchedSecond))
        return 0.0f;
#if defined(DEBUG_HASH)
    qDebug() << "Full hash buckets first:" << touchedFirst << "second:" << touchedSecond << "of" << m_size * 2 /*<< indexesTouched*/;
#endif
    return (touchedFirst + touchedSecond) / float(sample * 2);
}

