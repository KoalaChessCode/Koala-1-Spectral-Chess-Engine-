#pragma once
#include "Constants.hpp"
#include "memory.hpp"


template<typename T>
struct Node
{
    T position{};
};

template<typename T>
struct Tree
{
    Node<T>* nodes;

    std::uint32_t* childBegin;
    std::uint16_t* childCount;
    std::uint32_t* parentIndex;

    std::size_t size;
    std::size_t capacity;

    Move* moves;

    Tree(Arena& arena, std::size_t maxNodes)
        :
        nodes(arena.allocate<Node<T>>(maxNodes)),
        childBegin(arena.allocate<std::uint32_t>(maxNodes)),
        childCount(arena.allocate<std::uint16_t>(maxNodes)),
        parentIndex(arena.allocate<std::uint32_t>(maxNodes)),
        size(1),
        capacity(maxNodes),
        moves(arena.allocate<Move>(maxNodes))
    {
        // Construct all Node<T>
        for (std::size_t i = 0; i < maxNodes; ++i)
        {
            std::construct_at(&nodes[i]);
        }

        parentIndex[0] = 0;
        childBegin[0] = 0;
        childCount[0] = 0;
    }

    Node<T>& At(std::size_t index)
    {
        return nodes[index];
    }

    Node<T>& Parent(std::size_t index)
    {
        return nodes[parentIndex[index]];
    }

    Node<T>& Child(
        std::size_t parent,
        std::size_t child
    )
    {
        return nodes[childBegin[parent] + child];
    }

    std::uint32_t ChildBegin(std::size_t parent) const
    {
        return childBegin[parent];
    }

    std::uint16_t ChildCount(std::size_t parent) const
    {
        return childCount[parent];
    }

    bool IsLeaf(std::size_t index) const
    {
        return childCount[index] == 0;
    }

    Node<T>& CreateChild(std::uint32_t parentIdx)
    {
        if (size >= capacity)
            throw std::out_of_range("Tree full");

        std::uint32_t newChildIdx =
            static_cast<std::uint32_t>(size++);

        if (childCount[parentIdx] == 0)
            childBegin[parentIdx] = newChildIdx;

        childCount[parentIdx]++;
        parentIndex[newChildIdx] = parentIdx;

        return nodes[newChildIdx];
    }

    std::uint32_t CreateChildren(
        std::uint32_t parentIdx,
        std::uint16_t count
    )
    {
        if (size + count > capacity)
            return UINT32_MAX;

        std::uint32_t first =
            static_cast<std::uint32_t>(size);

        childBegin[parentIdx] = first;
        childCount[parentIdx] = count;

        for (std::uint16_t i = 0; i < count; ++i)
        {
            parentIndex[size] = parentIdx;
            ++size;
        }

        return first;
    }
};

struct TreeStateWrapper
{
    Arena memory;
    Tree<Board> tree;

    TreeStateWrapper(std::size_t treeSize = 1000) //980000 dla 256 MB
    :
    memory(),
    tree(memory, treeSize)
{
}
};

struct PUCT{
    float Score;
    float value;
    int visits;
    float prior;

};


