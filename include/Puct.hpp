#pragma once
#include "Constants.hpp"
#include "memory.hpp"
#include "Delta.hpp"


template<typename T>
struct Node
{
    T position{};
    FftDelta<4> fftDelta{};
};

template<typename T>
struct Tree
{
    Node<T>* nodes;

    std::uint32_t* childBegin;

    std::uint16_t* childCount;

    std::uint32_t* parentIndex;

    Move* moves;

    std::size_t size;

    std::size_t capacity;


    Tree(
        Arena& arena,
        std::size_t maxNodes
    )
        :
        nodes(
            arena.allocate<Node<T>>(maxNodes)
        ),

        childBegin(
            arena.allocate<std::uint32_t>(
                maxNodes
            )
        ),

        childCount(
            arena.allocate<std::uint16_t>(
                maxNodes
            )
        ),

        parentIndex(
            arena.allocate<std::uint32_t>(
                maxNodes
            )
        ),

        moves(
            arena.allocate<Move>(
                maxNodes
            )
        ),

        size(1),

        capacity(maxNodes)
    {
        for (std::size_t i = 0;
             i < maxNodes;
             ++i)
        {
            std::construct_at(
                &nodes[i]
            );

            childBegin[i] = 0;
            childCount[i] = 0;
            parentIndex[i] = 0;
        }

        parentIndex[0] = 0;
        childBegin[0] = 0;
        childCount[0] = 0;

        nodes[0].fftDelta = {};
    }


    Node<T>& At(
        std::size_t index
    )
    {
        return nodes[index];
    }


    const Node<T>& At(
        std::size_t index
    ) const
    {
        return nodes[index];
    }


    Node<T>& Parent(
        std::size_t index
    )
    {
        return nodes[
            parentIndex[index]
        ];
    }


    Node<T>& Child(
        std::size_t parent,
        std::size_t child
    )
    {
        return nodes[
            childBegin[parent] + child
        ];
    }


    std::uint32_t ChildBegin(
        std::size_t parent
    ) const
    {
        return childBegin[parent];
    }


    std::uint16_t ChildCount(
        std::size_t parent
    ) const
    {
        return childCount[parent];
    }


    bool IsLeaf(
        std::size_t index
    ) const
    {
        return childCount[index] == 0;
    }


    Node<T>& CreateChild(
        std::uint32_t parentIdx
    )
    {
        if (size >= capacity)
            throw std::out_of_range(
                "Tree full"
            );

        const std::uint32_t newChildIdx =
            static_cast<std::uint32_t>(
                size++
            );

        if (childCount[parentIdx] == 0)
            childBegin[parentIdx] =
                newChildIdx;

        childCount[parentIdx]++;

        parentIndex[newChildIdx] =
            parentIdx;

        return nodes[newChildIdx];
    }


    std::uint32_t CreateChildren(
        std::uint32_t parentIdx,
        std::uint16_t count
    )
    {
        if (size + count > capacity)
            return UINT32_MAX;

        const std::uint32_t first =
            static_cast<std::uint32_t>(
                size
            );

        childBegin[parentIdx] =
            first;

        childCount[parentIdx] =
            count;

        for (std::uint16_t i = 0;
             i < count;
             ++i)
        {
            parentIndex[size] =
                parentIdx;

            ++size;
        }

        return first;
    }
};


//800000<---for 256MB
//2000000<--for 2GB
struct TreeStateWrapper
{
    Arena memory;
    Tree<Board> tree;

    TreeStateWrapper(std::size_t treeSize = 800000)
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


