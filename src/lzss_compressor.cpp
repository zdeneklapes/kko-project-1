#include "lzss_compressor.h"
#include "Program.h"
#include <cstring>  // for memset if needed

LZSSCompressor::LZSSCompressor()
        : m_window(WINDOW_SIZE, 0),
          m_hashHead(HASH_SIZE, -1),
          m_hashNext(WINDOW_SIZE, -1),
          m_curPos(0) {
    // Nothing special here, vectors are already allocated with default values.
}

// TODO: Continue here
void LZSSCompressor::compress_static(const Program &program) {
    // We'll read bytes one by one from `in`.
    // For each new byte, we put it in the ring buffer, then try to find the best match.
    std::istream &in = program.m_file->m_in;
    std::ostream &out = program.m_file->m_out;

    while (true) {
        // Read the next byte from input
        uint8_t c;
        if (!in.read(reinterpret_cast<char *>(&c), 1)) {
            break; // EOF or read failure
        }

        // Place it in the ring buffer
        m_window[m_curPos % WINDOW_SIZE] = c;

        // Insert the new position into the hash.
        if (m_curPos + 2 < INT32_MAX) {
            // Make sure we have at least 3 bytes to form a hash if you want a 3-byte hash
            if (m_curPos > 2) {
                // We can safely insert the new position minus 2 so that
                // the substring (pos, pos+1, pos+2) is valid
                insertIntoHash(m_curPos - 2);
            }
        }

        // Now attempt to find a match starting at (m_curPos - 2) or at least near this pos.
        // Usually you'd do the match at `m_curPos - (LOOKAHEAD_SIZE-1)`, but let's simplify.
        int searchPos = m_curPos - (LOOKAHEAD_SIZE - 1);
        if (searchPos < 0) {
            searchPos = 0;
        }

        // Attempt match for the *current* position (but we only have 1 new byte so far).
        // Real LZSS: you'd maintain a lookahead buffer, fill it, then do the search.
        // For demonstration, let's just create a simplistic approach:
        int bestOffset = 0;
        int bestLength = findMatch(m_curPos, bestOffset);

        // Decide if we emit a literal or a match token
        if (bestLength >= THRESHOLD) {
            // We found a match. Create a match token.
            Token t;
            t.isLiteral = false;
            t.offset = bestOffset;
            t.length = bestLength;
            // Output the token
            outputToken(out, t);

            // Advance current position by matchLength - 1, because we already consumed matchLength bytes
            // with that match. The loop's `++m_curPos` will increment one more, effectively
            // moving us forward matchLength bytes in total.
            m_curPos += (bestLength - 1);

            // We should also read ahead in the input the remainder of those match bytes,
            // because we matched them from the dictionary (they are presumably repeated).
            // But in a real LZSS, you'd do a more sophisticated approach to reading the lookahead buffer.
            // Omitted here for brevity.
        } else {
            // No match or match is too short. Emit a literal token.
            Token t;
            t.isLiteral = true;
            t.literal = c;
            outputToken(out, t);
        }

        // Move forward in the input
        m_curPos++;
    }

    // Flush or finalize the output stream if needed.
    out.flush();
}

void LZSSCompressor::insertIntoHash(int pos) {
    // We want to compute the hash of (pos, pos+1, pos+2).
    // Because pos may wrap in the ring buffer, we do mod WINDOW_SIZE.
    // Also ensure pos+2 < m_curPos if we've actually read enough bytes.

    // Edge check: only insert if the data is valid
    if (pos < 0) return;
    if (pos + 2 >= m_curPos) return; // We haven't read that far yet

    uint8_t c1 = m_window[(pos) % WINDOW_SIZE];
    uint8_t c2 = m_window[(pos + 1) % WINDOW_SIZE];
    uint8_t c3 = m_window[(pos + 2) % WINDOW_SIZE];
    unsigned int h = hash3(c1, c2, c3);

    // Insert at the head of the chain for hash h
    int ringIndex = pos % WINDOW_SIZE;
    m_hashNext[ringIndex] = m_hashHead[h];
    m_hashHead[h] = ringIndex;
}

int LZSSCompressor::findMatch(int pos, int &bestOffset) {
    // We'll compute the hash of the 3-byte substring at pos. Then we look up candidates in m_hashHead[h].
    // Then compare forward to find the longest match.
    // Return length of best match, set bestOffset to distance from pos.

    if (pos < 2) {
        bestOffset = 0;
        return 0; // Not enough data to form a 3-byte hash
    }

    uint8_t c1 = m_window[(pos) % WINDOW_SIZE];
    uint8_t c2 = m_window[(pos + 1) % WINDOW_SIZE];
    uint8_t c3 = m_window[(pos + 2) % WINDOW_SIZE];
    unsigned int h = hash3(c1, c2, c3);

    int bestLength = 0;
    bestOffset = 0;

    // Traverse the chain of ring indices for this hash value
    int chainPos = m_hashHead[h];
    while (chainPos != -1) {
        // The actual absolute position of chainPos in the input is
        //   absPos = (m_curPos / WINDOW_SIZE)*WINDOW_SIZE + chainPos  (conceptually).
        // But for matching, we only need to check relative distances in the ring buffer.

        // Check if chainPos is within the valid window range from 'pos'.
        // In LZSS, distance = pos - chainPos (mod window size).
        int distance = (pos - chainPos + WINDOW_SIZE) % WINDOW_SIZE;
        if (distance == 0) {
            // We skip if distance == 0 because it means the same location.
            chainPos = m_hashNext[chainPos];
            continue;
        }
        // If distance >= WINDOW_SIZE, it's out of our sliding window.
        if (distance >= WINDOW_SIZE) {
            chainPos = m_hashNext[chainPos];
            continue;
        }

        // Compare forward to see how many bytes match
        int matchLen = 0;
        while (matchLen < LOOKAHEAD_SIZE) {
            uint8_t w1 = m_window[(pos + matchLen) % WINDOW_SIZE];
            uint8_t w2 = m_window[(chainPos + matchLen) % WINDOW_SIZE];
            if (w1 != w2) break;
            matchLen++;
        }

        if (matchLen > bestLength) {
            bestLength = matchLen;
            bestOffset = distance;
            // If we achieve the max lookahead, no need to check further
            if (bestLength == LOOKAHEAD_SIZE) break;
        }

        chainPos = m_hashNext[chainPos];
    }

    return bestLength;
}

void LZSSCompressor::outputToken(std::ostream &out, const Token &token) {
    // For demonstration, we’ll just write a simple header byte followed by data:
    //
    //   [isLiteral? 1 or 0] + [literal] OR [offset, length]
    //
    // This is not a standard LZSS format—it’s just to illustrate.

    if (token.isLiteral) {
        // Write a marker byte 0x00, then the literal
        uint8_t marker = 0x00;
        out.write(reinterpret_cast<char *>(&marker), 1);
        out.write(reinterpret_cast<const char *>(&token.literal), 1);
    } else {
        // Write a marker byte 0x01, then offset (2 bytes), then length (1 byte)
        uint8_t marker = 0x01;
        out.write(reinterpret_cast<char *>(&marker), 1);
        out.write(reinterpret_cast<const char *>(&token.offset), 2);
        out.write(reinterpret_cast<const char *>(&token.length), 1);
    }
}
