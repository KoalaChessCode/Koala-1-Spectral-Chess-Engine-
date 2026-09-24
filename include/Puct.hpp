#pragma once
#include "Constants.hpp"
#include "memory.hpp"
#include "GATv2.hpp"

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
    GATv2Criticism* criticism;

    Tree(
    Arena& arena,
    std::size_t maxNodes
)
    :
    nodes(arena.allocate<Node<T>>(maxNodes)),
    childBegin(arena.allocate<std::uint32_t>(maxNodes)),
    childCount(arena.allocate<std::uint16_t>(maxNodes)),
    parentIndex(arena.allocate<std::uint32_t>(maxNodes)),
    size(0),
    capacity(maxNodes),
    moves(arena.allocate<Move>(maxNodes)),
    criticism(arena.allocate<GATv2Criticism>(maxNodes))
{
}


    Node<T>* At(std::size_t index)
    {
        return nodes + index;
    }


    Node<T>* Parent(std::size_t index)
    {
        return nodes + parentIndex[index];
    }


    Node<T>* Children(std::size_t index)
    {
        return nodes + childBegin[index];
    }


    Node<T>* Child(
        std::size_t parent,
        std::size_t child
    )
    {
        return nodes
             + childBegin[parent]
             + child;
    }
};

struct TreeStateWrapper
{
    Arena memory;
    Tree<Board> tree;

    TreeStateWrapper(std::size_t treeSize)
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


