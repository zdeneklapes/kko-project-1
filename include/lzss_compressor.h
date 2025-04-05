#pragma once

#include <vector>
#include <cstdint>
#include <iostream>
#include <algorithm>
#include "Program.h"

// Example constants:
static const int WINDOW_SIZE      = 4096; // LZSS typical
static const int LOOKAHEAD_SIZE   = 18;   // LZSS typical
static const int THRESHOLD        = 2;    // Only encode matches >= this length
static const int HASH_SIZE        = 8192; // Adjust as needed

// For storing tokens (literal or match).
// In a real LZSS, you'd do bit-packing, but let's keep it simple here.
struct Token {
    bool isLiteral;
    uint8_t literal;
    uint16_t offset; // distance backwards
    uint8_t length;  // match length
};

class LZSSCompressor {
public:
    LZSSCompressor();
    ~LZSSCompressor() = default;

    void compress_static(const Program &program);

private:
    // The sliding window (ring buffer).
    // We’ll store data in `m_window[pos % WINDOW_SIZE]`.
    std::vector<uint8_t> m_window;

    // Hash structures:
    //   hashHead[h] holds the head index (in the ring buffer) for the chain of positions that share hash h.
    //   hashNext[i] holds the "next" index for ring-buffer index i.
    std::vector<int> m_hashHead;
    std::vector<int> m_hashNext;

    // Current input position (how many total bytes read so far).
    // We'll store data into m_window at (m_curPos % WINDOW_SIZE).
    int m_curPos;

    // Compute a simple hash of 3 bytes at (pos, pos+1, pos+2).
    // For real code, you might want a faster rolling hash.
    inline unsigned int hash3(uint8_t c1, uint8_t c2, uint8_t c3) {
        unsigned int h = c1;
        h = ((h << 4) + c2) % HASH_SIZE;
        h = ((h << 4) + c3) % HASH_SIZE;
        return h;
    }

    // Insert the current position into the hash table.
    void insertIntoHash(int pos);

    // Find the best match in the sliding window for the substring starting at `pos`.
    // Returns the length of the match and sets `bestOffset`.
    int findMatch(int pos, int &bestOffset);

    // Emit a token (literal or match) to `out`.
    // For demonstration, we just write raw data. Production code would do bit/byte packing.
    void outputToken(std::ostream &out, const Token &token);
};



