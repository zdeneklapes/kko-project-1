#ifndef BINARY_TREE_H
#define BINARY_TREE_H

#include <cstdint>
#include <cstring>


#include <string>
#include <cstdint>

// A basic node class for a binary search tree.
// 'text' can hold the substring or key, 'offset' can be used for position info.
class Node {
public:
    // Constructor that sets up the node with given text key and offset.
    // left, right, and parent default to nullptr if not provided.
    Node(const std::string &sub, std::uint64_t off,
         Node *l = nullptr, Node *r = nullptr, Node *p = nullptr)
            : substring(sub), offset(off), left(l), right(r), parent(p) {}

    // A trivial destructor is fine here; tree cleanup is usually handled by the tree class.
    ~Node() = default;

    // The 'text' could represent the substring key for the node.
    std::string substring;
    // The 'offset' might be used in an LZSS context or other position-based logic.
    std::uint64_t offset;

    // Pointers to left and right children, and to this node's parent.
    Node *left;
    Node *right;
    Node *parent;
};

class BinaryTree {
public:
    // Constructor/destructor.
    BinaryTree();

    ~BinaryTree();

    // Insert a new node with the given text/offset into the BST.
    void insert(const std::string &txt, std::uint64_t off);

    // Find a node by its text (exact match).
    // Returns the Node* if found, or nullptr if not found.
    Node *find(const std::string &txt) const;

    // Delete a specific node from the tree.
    void remove(Node *node);

    // Optional: A helper to delete the node matching 'txt' if it exists.
    void remove_by_text(const std::string &txt);

    // Optional: A simple traversal (in-order) to demonstrate the tree contents.
    void print_in_order() const;

    std::pair<std::uint64_t, std::size_t>
    find_best_prefix_match(const std::string &input_sub, std::size_t threshold) const;


private:
    // Pointer to the root of the tree.
    Node *root;

    // Internal helper functions.
    Node *insert_recursive(Node *current, Node *parent,
                           const std::string &txt, std::uint64_t off);

    Node *find_recursive(Node *current, const std::string &txt) const;

    Node *min_node(Node *current) const;   // find the minimum node in a subtree
    Node *remove_recursive(Node *current, const std::string &txt);

    void transplant(Node *old_node, Node *new_node);

    void destroy_subtree(Node *current);

    void print_in_order_recursive(Node *current) const;

    // The naive "best prefix" search – we do a full DFS on the entire tree.
    void find_best_prefix_dfs(Node *current,
                              const std::string &input_sub,
                              std::size_t threshold,
                              std::uint64_t &best_off,
                              std::size_t &best_len) const;

};


#endif // BINARY_TREE_H
