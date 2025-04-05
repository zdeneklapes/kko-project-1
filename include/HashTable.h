#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <vector>
#include <map>
#include <set>
#include <cstddef>
#include <cstdint>

// Simple struct for holding a match result (offset + length)
struct lz_match {
    std::size_t offset = 0;
    std::size_t length = 0;
};

// A minimal, hash-based structure using std::map to store substring positions.
// We store an integer "hash" -> a set of positions that share that hash.
class HashTable
{
public:
    // Constructor: we receive a reference to the sliding window, its size,
    // and how many bytes we consider for hashing (max_coded).
    HashTable(std::vector<uint8_t> &window,
              std::size_t search_size,
              std::size_t max_coded);

    // Insert the substring at 'pos' (of length max_coded+1) into our table.
    void add_string(std::size_t pos);

    // Remove that same substring from our table.
    void remove_string(std::size_t pos);

    // Find the best match for 'lookahead' in the sliding window.
    // Returns (offset=0, length=0) if none is found.
    lz_match find_match(const std::vector<uint8_t> &lookahead) const;

    // If we change a single character at 'pos', we remove all relevant substrings,
    // perform the replacement, and re-insert them.
    void replace_char(std::size_t pos, uint8_t new_char);

private:
    // Compute a simple shift-and-XOR hash for (max_coded+1) bytes starting at pos.
    std::size_t compute_hash(std::size_t pos) const;

    // Wrap index for circular referencing in the sliding window.
    inline std::size_t wrap_index(std::size_t i) const { return i % m_window_size; }

private:
    std::vector<uint8_t> &m_window;  // The sliding window
    std::size_t m_window_size;       // e.g. 4096
    std::size_t m_max_coded;         // We hash (max_coded + 1) bytes

    // The map: hash_value -> set of positions that share that hash
    std::map<std::size_t, std::set<std::size_t>> m_hash_map;
};

#endif // HASH_TABLE_H
