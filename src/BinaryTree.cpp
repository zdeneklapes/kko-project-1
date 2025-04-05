#include "../include/BinaryTree.h"
#include <iostream>

// ----- Constructor / Destructor -----

BinaryTree::BinaryTree()
        : root(nullptr) {
}

BinaryTree::~BinaryTree() {
    // Recursively delete all nodes to avoid leaks.
    destroy_subtree(root);
}

// Recursively delete all nodes from the subtree rooted at 'current'.
void BinaryTree::destroy_subtree(Node *current) {
    if (!current) {
        return;
    }
    destroy_subtree(current->left);
    destroy_subtree(current->right);
    delete current; // Freed from the heap
}

//----------------------------
// Insert
//----------------------------
void BinaryTree::insert(const std::string &txt, std::uint64_t off) {
    // Insert recursively to keep logic straightforward.
    root = insert_recursive(root, nullptr, txt, off);
}

// The recursive insert function: it returns the new root of this subtree.
Node *BinaryTree::insert_recursive(Node *current, Node *parent,
                                   const std::string &txt, std::uint64_t off) {
    if (!current) {
        // Reached a null link, create a new node here.
        Node *node = new Node(txt, off, nullptr, nullptr, parent);
        return node;
    }

    // Compare the new text with the current node's text (lexicographical).
    if (txt < current->substring) {
        current->left = insert_recursive(current->left, current, txt, off);
    } else {
        // If txt >= current->text, we go right (ties go right for simplicity).
        current->right = insert_recursive(current->right, current, txt, off);
    }
    return current;
}

//------------------------------
// Remove
//------------------------------
void BinaryTree::remove(Node *node) {
    if (!node) {
        return;
    }

    // If node has no left child, we can transplant with node->right.
    if (!node->left) {
        transplant(node, node->right);
    } else if (!node->right) {
        // If node has no right child, we can transplant with node->left.
        transplant(node, node->left);
    } else {
        // Node has two children; find its successor (minimum in right subtree).
        Node *succ = min_node(node->right);
        if (succ->parent != node) {
            // Move successor's right child up
            transplant(succ, succ->right);
            succ->right = node->right;
            succ->right->parent = succ;
        }
        transplant(node, succ);
        succ->left = node->left;
        succ->left->parent = succ;
    }
    delete node; // Freed from the heap
}

void BinaryTree::remove_by_text(const std::string &txt) {
    // We can reuse the simpler approach: find the node, then remove it.
    Node *target = find(txt);
    if (target) {
        remove(target);
    }
}

// This helper re-links the parent's child pointer from old_node to new_node.
void BinaryTree::transplant(Node *old_node, Node *new_node) {
    if (!old_node->parent) {
        // old_node was the root
        root = new_node;
    } else if (old_node == old_node->parent->left) {
        // old_node was the left child
        old_node->parent->left = new_node;
    } else {
        // old_node was the right child
        old_node->parent->right = new_node;
    }
    if (new_node) {
        // new_node is not null
        new_node->parent = old_node->parent;
    }
}

Node *BinaryTree::min_node(Node *current) const {
    // Walk left until left is null.
    while (current && current->left) {
        current = current->left;
    }
    return current;
}

// ----- Traversal -----

void BinaryTree::print_in_order() const {
    print_in_order_recursive(root);
    std::cout << std::endl;
}

void BinaryTree::print_in_order_recursive(Node *current) const {
    if (!current) {
        return;
    }
    print_in_order_recursive(current->left);
    // Print the node's text and offset
    std::cout << "(" << current->substring << ", off=" << current->offset << ") ";
    print_in_order_recursive(current->right);
}

//-------------------------
// Find
//-------------------------

Node *BinaryTree::find(const std::string &sub) const {
    return find_recursive(root, sub);
}

// Standard BST find, matching exactly the 'substring'.
Node *BinaryTree::find_recursive(Node *current, const std::string &sub) const {
    if (!current) {
        return nullptr;
    }
    if (sub == current->substring) {
        return current;
    }
    if (sub < current->substring) {
        return find_recursive(current->left, sub);
    } else {
        return find_recursive(current->right, sub);
    }
}

// Does a DFS to find the node with the best prefix match vs. 'input_sub'.
// 'best_off' and 'best_len' track the best match so far.
void BinaryTree::find_best_prefix_dfs(Node *current,
                                      const std::string &input_sub,
                                      std::size_t threshold,
                                      std::uint64_t &best_off,
                                      std::size_t &best_len) const {
    if (!current) {
        return;
    }

    // Check how many characters match between current->substring and input_sub.
    std::size_t match_count = 0;
    std::size_t max_check = std::min(current->substring.size(), input_sub.size());
    while (match_count < max_check &&
           current->substring[match_count] == input_sub[match_count]) {
        match_count++;
    }

    if (match_count > best_len && match_count > threshold) {
        best_len = match_count;
        best_off = current->offset;
    }

    // Continue DFS left and right.
    find_best_prefix_dfs(current->left, input_sub, threshold, best_off, best_len);
    find_best_prefix_dfs(current->right, input_sub, threshold, best_off, best_len);
}

std::pair<std::uint64_t, std::size_t>
BinaryTree::find_best_prefix_match(const std::string &input_sub,
                                   std::size_t threshold) const {
    std::uint64_t best_off = 0;
    std::size_t best_len = 0;

    // We do a naive full DFS to find the best prefix match in the entire tree.
    find_best_prefix_dfs(root, input_sub, threshold, best_off, best_len);

    // If best_len <= threshold, typically we do not treat it as a match.
    // Return {0,0} in that case.  Otherwise, we return {best_off, best_len}.
    if (best_len <= threshold) {
        return {0, 0};
    } else {
        return {best_off, best_len};
    }
}
