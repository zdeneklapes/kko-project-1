#include "../include/HashTable.h"
#include <algorithm> // for std::min

HashTable::HashTable(std::vector<uint8_t> &window,
                     std::size_t search_size,
                     std::size_t max_coded)
        : m_window(window),
          m_window_size(search_size),
          m_max_coded(max_coded)
{
    // For std::map, we don't do extra reservations. The structure is a balanced tree.
}

std::size_t HashTable::compute_hash(std::size_t pos) const
{
    // We'll hash up to (m_max_coded + 1) bytes in a shift-and-XOR manner.
    std::size_t hval = 0;
    for (std::size_t i = 0; i <= m_max_coded; i++) {
        std::size_t idx = wrap_index(pos + i);
        hval = (hval << 5) ^ m_window[idx];
    }
    return hval;
}

void HashTable::add_string(std::size_t pos)
{
    if (pos >= m_window_size) return;
    std::size_t h = compute_hash(pos);
    // Insert 'pos' into the set for this hash. Automatic in std::set
    m_hash_map[h].insert(pos);
}

void HashTable::remove_string(std::size_t pos)
{
    if (pos >= m_window_size) return;
    std::size_t h = compute_hash(pos);

    auto it = m_hash_map.find(h);
    if (it == m_hash_map.end()) return; // not present

    // Remove 'pos' from the set
    it->second.erase(pos);

    // If that set is now empty, we remove the map entry
    if (it->second.empty()) {
        m_hash_map.erase(it);
    }
}

lz_match HashTable::find_match(const std::vector<uint8_t> &lookahead) const
{
    lz_match best;
    if (lookahead.empty()) return best;

    // We'll hash the first (m_max_coded+1) or fewer bytes from lookahead
    std::size_t needed = std::min(m_max_coded + 1, lookahead.size());
    std::size_t hval = 0;
    for (std::size_t i = 0; i < needed; i++) {
        hval = (hval << 5) ^ lookahead[i];
    }

    // If we don't find that hash, no match
    auto it = m_hash_map.find(hval);
    if (it == m_hash_map.end()) {
        return best;
    }

    // For each candidate position in the set, check how many bytes match
    for (auto pos : it->second) {
        std::size_t match_len = 0;
        while (match_len < lookahead.size()) {
            std::size_t idx = wrap_index(pos + match_len);
            if (m_window[idx] != lookahead[match_len]) break;
            match_len++;
        }
        if (match_len > best.length) {
            best.length = match_len;
            best.offset = pos;

            if (match_len == lookahead.size()) break; // perfect match
        }
    }
    return best;
}

void HashTable::replace_char(std::size_t pos, uint8_t new_char)
{
    // We remove all substrings that might include 'pos'
    // i.e. from [pos - m_max_coded .. pos], wrapping as needed
    std::size_t start = (pos < m_max_coded) ? (m_window_size + pos - m_max_coded)
                                            : (pos - m_max_coded);

    for (std::size_t i = 0; i <= m_max_coded; i++) {
        remove_string((start + i) % m_window_size);
    }

    // Actually replace the character
    m_window[pos] = new_char;

    // Re-insert them
    for (std::size_t i = 0; i <= m_max_coded; i++) {
        add_string((start + i) % m_window_size);
    }
}
