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

#ifndef TREEITERATOR_H
#define TREEITERATOR_H

#include "node.h"
#include "treeutils.h"

template <Traversal>
class TreeIterator {
public:
    bool operator!=(const TreeIterator& other) const;
    Node *operator*();
    void operator++();

private:
    friend class Node;
    TreeIterator();
    TreeIterator(Node *data);
    Node *previousNode;
    Node *currentNode;
};

template <Traversal t>
inline TreeIterator<t>::TreeIterator()
{
    previousNode = nullptr;
    currentNode = nullptr;
}

template <Traversal t>
inline TreeIterator<t>::TreeIterator(Node *data)
{
    previousNode = nullptr;
    currentNode = data;
}

template <Traversal t>
inline bool TreeIterator<t>::operator!=(const TreeIterator& other) const
{
    return currentNode != other.currentNode;
}

template <Traversal t>
inline Node *TreeIterator<t>::operator*()
{
    return currentNode;
}

template<>
inline void TreeIterator<PreOrder>::operator++()
{
    if (!currentNode)
        return;

    if (Node *next = currentNode->leftChild()) {
        previousNode = currentNode;
        currentNode = next;
        return;
    }

    if (Node *next = currentNode->nextSibling()) {
        previousNode = currentNode;
        currentNode = next;
        return;
    }

    Node *next = currentNode->nextAncestorSibling();
    previousNode = currentNode;
    currentNode = next;
}

template <>
inline TreeIterator<PostOrder>::TreeIterator(Node *data)
{
    previousNode = nullptr;
    currentNode = data->leftMostChild();
}

template<>
inline void TreeIterator<PostOrder>::operator++()
{
    if (!currentNode)
        return;

    if (!previousNode->isChildOf(currentNode)) {
        if (Node *next = currentNode->leftMostChild()) {
            previousNode = currentNode;
            currentNode = next;
            return;
        }
    }

    if (Node *nextSibling = currentNode->nextSibling()) {
        if (Node *next = nextSibling->leftMostChild()) {
            previousNode = currentNode;
            currentNode = next;
            return;
        }
        previousNode = currentNode;
        currentNode = nextSibling;
        return;
    }

    previousNode = currentNode;
    currentNode = currentNode->parent();
}

#endif // TreeIterator_H
