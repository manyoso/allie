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
// increasing by one cuts number of entries by half
#define MAX_POTENTIALS_COUNT 159

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

class MyHash : public Hash { };
Q_GLOBAL_STATIC(MyHash, HashInstance)
Hash* Hash::globalInstance()
{
    return HashInstance();
}

Hash::Hash()
    : m_cache(nullptr)
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
    if (m_cache)
        m_cache->clear();
    delete m_cache;
    m_cache = nullptr;
}

quint64 largestPowerofTwoLessThan(quint64 n)
{
   quint64 p = quint64(qLn(n) / qLn(2));
   return quint64(pow(2, p));
}

void Hash::reset()
{
    quint64 bytes = Options::globalInstance()->option("Hash").value().toUInt() * quint64(1024) * quint64(1024);
    quint64 maxSize = bytes / sizeof(HashEntry);
    quint64 size = largestPowerofTwoLessThan(maxSize);
    if (!m_cache || quint64(m_cache->totalCost()) != size) {
        delete m_cache;
        m_cache = new QCache<quint64, HashEntry>(int(size));
#if defined(DEBUG_HASH)
        qDebug() << "Hash size is" << size;
#endif
    }

    clear();
}

void Hash::clear()
{
    if (m_cache)
        m_cache->clear();
}

bool Hash::contains(const Node *node) const
{
    if (!m_cache || !m_cache->maxCost())
        return false;

    return m_cache->contains(node->game().hash());
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
    Q_ASSERT(m_cache);
    if (!m_cache)
        return false;

    HashEntry *entry = m_cache->object(node->game().hash());
    if (!entry)
        return false;

    return fillOutNodeFromEntry(node, *entry);
}

void Hash::insert(const Node *node)
{
    if (!m_cache || !m_cache->maxCost())
        return;

    if (node->potentials().count() > MAX_POTENTIALS_COUNT)
        return; // Too many potentials to cache!

    HashEntry *entry = new HashEntry;
    entry->qValue = node->rawQValue();
    Q_ASSERT(!qFuzzyCompare(entry->qValue, -2.0f));

    const QVector<PotentialNode*> po = node->potentials();
    for (int i = 0; i < po.count(); ++i) {
        PotentialNode *potential = po.at(i);
        HashPValue pValue;
        pValue.pValue = potential->pValue();
        Q_ASSERT(!qFuzzyCompare(potential->pValue(), -2.0f));
        pValue.index = moveToNNIndex(potential->move());
        entry->potentialValues[i] = pValueToHash(pValue);
    }

    for (int i = po.count(); i < MAX_POTENTIALS_COUNT; ++i)
        entry->potentialValues[i] = pValueToHash(HashPValue());

    m_cache->insert(node->game().hash(), entry, 1);
}

float Hash::percentFull(int halfMoveNumber) const
{
    if (!m_cache || !m_cache->maxCost())
        return 1.0f;

    Q_UNUSED(halfMoveNumber);
    return quint64(m_cache->count()) / float(m_cache->maxCost());
}

