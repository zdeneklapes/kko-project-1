//
// Created by Zdeněk Lapeš on 26/03/2025.
//

// TODO
// - Fix fread and usage of read_char and get_char
// - Try compress some raw file

//------------------------------------------------------------------------------
// Includes
//------------------------------------------------------------------------------
#include "include/argparse/argparse.hpp"
#include <bitset>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <tuple>
#include <vector>

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------
static const std::size_t FLAG_SIZE_BITS = 1; // LZSS typical

static const std::size_t WINDOW_SIZE_BITS = 13; // LZSS typical
static const std::size_t WINDOW_SIZE = (1 << WINDOW_SIZE_BITS) - 1;

static const std::size_t LOOKAHEAD_SIZE_BITS = 5;
static const std::size_t LOOKAHEAD_SIZE = (1 << LOOKAHEAD_SIZE_BITS);

static const std::size_t MIN_MATCH_LENGTH = 3;
static const std::size_t LITERAL_SIZE = MIN_MATCH_LENGTH - 1;
static const std::size_t LITERAL_SIZE_BITS = LITERAL_SIZE * 8;
static const std::size_t HASH_TABLE_SIZE = 8192; // Adjust as needed

//------------------------------------------------------------------------------
// Macros
//------------------------------------------------------------------------------
#define CYCLIC_INCREMENT(head, size) ((head) = ((head) + 1) % (size))
#define CYCLIC_DECREMENT(head, size) ((head) = ((head) - 1) % (size))
#define DEBUG (1)
#define DEBUG_LITE (DEBUG)
#define DEBUG_PRINT_LITE(fmt, ...)                                                                                     \
    do {                                                                                                               \
        if (DEBUG_LITE)                                                                                                \
            fprintf(stderr, fmt, __VA_ARGS__);                                                                         \
    } while (0)
#define DEBUG_PRINT(fmt, ...)                                                                                          \
    do {                                                                                                               \
        if (DEBUG)                                                                                                     \
            fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, __LINE__, __func__, __VA_ARGS__);                            \
    } while (0)

//------------------------------------------------------------------------------
// Structs
//------------------------------------------------------------------------------

/**
 * Simple structure to store match information.
 */
struct lz_match {
    bool is_compressed = false;
    std::size_t offset = 0;
    std::size_t length = 0;
};

std::size_t wrap_index(std::size_t i, std::size_t buffer_size);

/**
 * Buffer
 */
struct Buffer {
    char *window = nullptr;
    char *lookahead = nullptr;
    std::size_t window_head = 0;    // Points always on the next position to write character to
    std::size_t lookahead_head = 0; // Points always on the currently read character

    Buffer() {
        window = new char[WINDOW_SIZE];
        lookahead = new char[LOOKAHEAD_SIZE];
        memset(window, 0, WINDOW_SIZE);
        memset(lookahead, 0, LOOKAHEAD_SIZE);
    }

    ~Buffer() {
        delete[] window;
        delete[] lookahead;
    }

    char *get_lookahead_current() { return &this->lookahead[wrap_index(this->lookahead_head + 1, LOOKAHEAD_SIZE)]; };

    void print_window() {
        DEBUG_PRINT_LITE("Window head: %zu\n", this->window_head);
        for (std::size_t i = 0; i < WINDOW_SIZE; ++i) {
            DEBUG_PRINT_LITE("%c", this->window[i]);
        }
        DEBUG_PRINT_LITE("%c", '\n');
    }

    void print_lookahead() {
        DEBUG_PRINT_LITE("Lookahead head: %zu\n", this->lookahead_head);
        for (std::size_t i = 0; i < LOOKAHEAD_SIZE; ++i) {
            DEBUG_PRINT_LITE("%c", this->lookahead[i]);
        }
        DEBUG_PRINT_LITE("%c", '\n');
    }
};

// Encoded structure
struct token {
    unsigned int flag : 1; // 1 bit: 0 indicates compressed token

    // if flag == 1
    unsigned int offset : 13; // 13 bits for the sliding-window offset
    unsigned int length : 5;  // 5 bits for the match length

    // if flag == 0
    unsigned int literal : 16; // 16 bits to pack 2 literal characters (8 bits each)
};
typedef struct token token_t;

//------------------------------------------------------------------------------
// Forward declarations
//------------------------------------------------------------------------------

class Buffer;

class Program;

// class HashTable {
// public:
//     // Constructor and destructor.
//     HashTable();
//
//     ~HashTable();
//
//     // Initializes the hash table using the current state of the sliding window.
//     void init_search_structures(Program &program);
//
//     // Inserts the substring starting at the given window index into the hash table.
//     void add_string(std::size_t character_index_in_sliding_window, Program &program);
//
//     // Removes the substring starting at the given window index from the hash table.
//     void remove_string(std::size_t character_index_in_sliding_window, Program &program);
//
//     // Finds the best match for the current lookahead buffer in the sliding window.
//     lz_match find_match(Program &program);
//
//     // The hash table: maps hash keys to a set of window positions that share that hash.
//     std::map<std::size_t, std::set<std::size_t>> hash_table;
// };

class File;

//------------------------------------------------------------------------------
// Classes
//------------------------------------------------------------------------------

class Program {
  public:
    // Heap-allocated pointer to the ArgumentParser instance.
    argparse::ArgumentParser *args;
    File *files;
    Buffer *buffers;
    //    HashTable *hash_table;

    // Constructor and Destructor.
    Program()
        : args(nullptr), buffers(new Buffer())
    //                hash_table(new HashTable())
    {}

    ~Program() {
        delete args;
        delete buffers;
        delete files; // Close files
                      //        delete hash_table;
    }

    // Parse command-line arguments.
    void parse_arguments(int argc, char **argv) {
        // Allocate the parser on the heap.
        args = new argparse::ArgumentParser("lz_codec");

        // Define mode arguments: compression (-c) and decompression (-d)
        args->add_argument("-c").help("activate compression mode").default_value(false).implicit_value(true);
        args->add_argument("-d").help("activate decompression mode").default_value(false).implicit_value(true);

        // Define additional flags: model (-m) and adaptive scanning (-a)
        args->add_argument("-m")
            .help("activate model for preprocessing input data")
            .default_value(false)
            .implicit_value(true);
        args->add_argument("-a").help("activate adaptive scanning mode").default_value(false).implicit_value(true);

        // Define file arguments: input (-i) and output (-o) file names (both
        // required)
        args->add_argument("-i").help("input file name").required();
        args->add_argument("-o").help("output file name").required();

        // Define the image width argument (-w) for compression mode.
        args->add_argument("-w")
            .help("image width (required for compression; must be >= 1)")
            .scan<'i', int>()
            .default_value(-1);

        // Optional verbose flag
        args->add_argument("-v", "--verbose")
            .help("increase output verbosity")
            .default_value(false)
            .implicit_value(true);

        // Parse the command-line arguments.
        try {
            args->parse_args(argc, argv);
        } catch (const std::runtime_error &err) {
            std::cerr << err.what() << std::endl;
            std::cerr << *args;
            exit(1);
        }
    }

    void print_arguments() {
        DEBUG_PRINT_LITE("Program arguments:%c", '\n');
        DEBUG_PRINT_LITE("-c | compress: %d%c", args->get<bool>("-c"), '\n');
        DEBUG_PRINT_LITE("-d | decompress: %d%c", args->get<bool>("-d"), '\n');
        DEBUG_PRINT_LITE("-m | model: %d%c", args->get<bool>("-m"), '\n');
        DEBUG_PRINT_LITE("-a | adaptive scanning: %d%c", args->get<bool>("-a"), '\n');
        DEBUG_PRINT_LITE("-i | input file: %s%c", args->get<std::string>("-i").c_str(), '\n');
        DEBUG_PRINT_LITE("-o | output file: %s%c", args->get<std::string>("-o").c_str(), '\n');
        DEBUG_PRINT_LITE("-w | width: %d%c", args->get<int>("-w"), '\n');
        DEBUG_PRINT_LITE("-v | verbose: %d%c", args->get<bool>("-v"), '\n');
    }
};

class File {
  public:
    // Retrieve input/output filenames from Program
    //    std::ifstream in;
    Program &program;
    FILE *in;
    std::ofstream out;
    uint8_t current_char = '\0';
    bool EOF_reached = false; // track if we hit EOF
#if DEBUG
    std::size_t current_pos = 0;
#endif
    char *buffer = nullptr;
    std::size_t buffer_size;
    long long unsigned int buffer_head = 0;
    // Returns a pointer to a dynamically allocated char array containing the file's data.
    // The file size is returned via the out parameter 'size'.
    std::tuple<char *, std::size_t> readBinaryFileToCharArray(const std::string &filename) {
        // Open the file in binary mode.
        std::ifstream file(filename, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Unable to open file " + filename);
        }

        // Seek to the end to determine the file size.
        file.seekg(0, std::ios::end);
        std::size_t size = file.tellg();
        file.seekg(0, std::ios::beg);

        // Allocate a buffer to hold the file's contents.
        char *buffer = new char[static_cast<size_t>(size)];
        memset(buffer, 0, size);

        // Read the file data into the buffer.
        if (!file.read(buffer, size)) {
            delete[] buffer;
            throw std::runtime_error("Error reading file " + filename);
        }

        return (std::make_tuple(buffer, size));
    }

    File(std::string in_filepath, std::string out_filepath, Program &program) : program(program) {
        const auto width = program.args->get<int>("-w");

        // Check if file exists
        if (!std::filesystem::exists(in_filepath)) {
            throw std::runtime_error("Input file does not exist");
        }
        //        in = std::ifstream(in_filepath, std::ios::binary);
        out = std::ofstream(out_filepath, std::ios::binary);

        //        if ((in = fopen(in_filepath.c_str(), "rb")) == NULL) {
        //            throw std::runtime_error("Input file does not exist");
        //        }
        //
        //        fread(buffer, sizeof(char), width * width, in);
        //        buffer_length = strlen(buffer); // TODO fix this
        auto [buffer, buffer_size] = readBinaryFileToCharArray(in_filepath);
        this->buffer = buffer;
        this->buffer_size = buffer_size;
    }

    ~File() {
        // Explicitly flush and close the output stream.
        if (out.is_open()) {
            out.flush();
            out.close();
        }
        // Close the input stream.
        //        if (in.is_open()) {
        //            // (ifstream typically does not buffer output, but we still close it)
        //            in.close();
        //        }

        fclose(in);

        delete[] buffer;
    }

    //    bool read_char() {
    // #if DEBUG
    //        current_pos++;
    // #endif
    //        throw std::runtime_error("Not implemented");
    //        //        int c = this->in.get();
    //        //        if (c == EOF) {
    //        //            current_char = static_cast<uint8_t>(c);
    //        //            EOF_reached = true;
    //        //            return false; // no more data
    //        //        }
    //        //        current_char = static_cast<uint8_t>(c);
    //        //        return true;
    //    }

    bool get_char() {
        current_char = buffer[buffer_head];
        const long long unsigned int width = program.args->get<int>("-w");
        buffer_head++;
        if (buffer_head == buffer_size) {
            EOF_reached = true;
            return false;
        }
        return true;
    }

    bool write_char(uint8_t in_byte) {
        // Debug print with a correct format (one %c and a newline)
        //        DEBUG_PRINT_LITE("===Writing byte%c", '\n');

        // Check that the stream is open
        //        assert(this->out.is_open());
        if (!this->out.is_open()) {
            DEBUG_PRINT_LITE("Error writing byte: %c\n", in_byte);
            return false;
        }

        // Write the byte to the file.
        this->out.put(static_cast<char>(in_byte));

        // Optionally flush to ensure the byte is written immediately.
        this->out.flush();

        // Check for errors.
        //        assert(!this->out.fail());
        if (this->out.fail()) {
            DEBUG_PRINT_LITE("Error writing byte: %c\n", in_byte);
            return false;
        }

        return true;
    }
};

class BitsetWriter {
  public:
    BitsetWriter(Program &program) : program(program), bits_filled(0), buffer(0) {}

    // Write the lower 'count' bits from 'bits' (starting from MSB)
    void write_bits(uint32_t bits, uint32_t count) {
        for (int i = count - 1; i >= 0; --i) {
            // Get the i-th bit (from MSB side)
            bool bit = (bits >> i) & 1;
            // Set bit in our bitset buffer at current position
            // We use position 7 - bits_filled so that the first bit goes into the MSB.
            buffer[7 - bits_filled] = bit;
            bits_filled++;
            if (bits_filled == 8) {
                flush_byte();
            }
        }
    }

    // Flush any remaining bits by padding with 0s
    void flush() {
        if (bits_filled > 0) {
            // Pad remaining bits with 0 (they are already 0 in our bitset)
            flush_byte();
        }
    }

  private:
    Program &program;
    int bits_filled;       // number of bits currently in the buffer (0 to 7)
    std::bitset<8> buffer; // our 8-bit buffer

    void flush_byte() {
        // Convert bitset to an unsigned long and cast to uint8_t
        uint8_t byte = static_cast<uint8_t>(buffer.to_ulong());
        program.files->write_char(byte);
        // Reset buffer and counter for the next byte
        bits_filled = 0;
        buffer.reset();
    }
};

//------------------------------------------------------------------------------
// Functions
//------------------------------------------------------------------------------
char update_buffers(Program &program) {
    auto *buffers = program.buffers;
    auto *files = program.files;

    files->get_char();
    const auto new_char = files->current_char;
    //    const auto _eof = EOF;
    //    DEBUG_PRINT_LITE("Is EOF: %c", (int)new_char == _eof);

    const auto lookahead_index_1 = wrap_index(buffers->lookahead_head + 1, LOOKAHEAD_SIZE);
    char output_char = buffers->lookahead[lookahead_index_1];

    // Update windows
    const auto window_index = wrap_index(buffers->window_head, WINDOW_SIZE);
    buffers->window[window_index] = output_char;

    const auto lookahead_index_2 = wrap_index(buffers->lookahead_head + 1, LOOKAHEAD_SIZE);
    if (files->EOF_reached) {
        buffers->lookahead[lookahead_index_2] = '\0';
    } else {
        buffers->lookahead[lookahead_index_2] = new_char;
    }

    // Update heads
    CYCLIC_INCREMENT(buffers->window_head, WINDOW_SIZE);
    CYCLIC_INCREMENT(buffers->lookahead_head, LOOKAHEAD_SIZE);

    return output_char;
}

void process_is_compressed(Program &program, lz_match &match, BitsetWriter &bitset_writer) {
    Buffer *buffers = program.buffers;
    bitset_writer.write_bits(1, FLAG_SIZE_BITS);
    bitset_writer.write_bits(match.offset, WINDOW_SIZE_BITS);
    bitset_writer.write_bits(match.length, LOOKAHEAD_SIZE_BITS);

    // Update buffers
    for (std::size_t i = 0; i < match.length; ++i) {
        //        DEBUG_PRINT_LITE("Updating buffer: %zu%c", i, '\n');
        update_buffers(program);
    }
}

void process_is_literal(Program &program, lz_match &match, BitsetWriter &bitset_writer) {
    Buffer *buffers = program.buffers;

    bitset_writer.write_bits(0, FLAG_SIZE_BITS);
    char char1 = buffers->lookahead[wrap_index(buffers->lookahead_head + 1, LOOKAHEAD_SIZE)];
    char char2 = buffers->lookahead[wrap_index(buffers->lookahead_head + 2, LOOKAHEAD_SIZE)];
    uint16_t literal = (char1 << 8) | char2;
    bitset_writer.write_bits(literal, LITERAL_SIZE_BITS);

    // Update buffers
    for (std::size_t i = 0; (i < LITERAL_SIZE) && strcmp(buffers->get_lookahead_current(), "\0") != 0; ++i) {
        //        DEBUG_PRINT_LITE("Updating buffer: %zu%c", i, '\n');
        update_buffers(program);
    }
}

//// Constructor: (if you need to initialize members, do it here)
// HashTable::HashTable() {
//     // You may leave this empty if nothing special is needed.
// }
//
//// Destructor
// HashTable::~HashTable() {
//     // Clean-up if required.
// }
//
// void HashTable::init_search_structures(Program &program) {
//     // Assume that Program has a pointer 'buffers' of type Buffer*
//     // and that the sliding window (buffers->window) is pre-filled with a known character,
//     // for example, a space ' '.
//     auto *buffers = program.buffers;
//     hash_table.clear();
//     std::size_t hash_key = compute_hash(0, buffers->window, WINDOW_SIZE);
//     // Insert the substring starting at position 0.
//     hash_table[hash_key].insert(0);
// }
//
// void HashTable::add_string(std::size_t character_index_in_sliding_window, Program &program) {
//     auto *buffers = program.buffers;
//     std::size_t hash_key = compute_hash(character_index_in_sliding_window, buffers->window, WINDOW_SIZE);
//     hash_table[hash_key].insert(character_index_in_sliding_window);
// }
//
// void HashTable::remove_string(std::size_t character_index_in_sliding_window, Program &program) {
//     auto *buffers = program.buffers;
//     std::size_t hash_key = compute_hash(character_index_in_sliding_window, buffers->window, WINDOW_SIZE);
//     auto it = hash_table.find(hash_key);
//     if (it != hash_table.end()) {
//         it->second.erase(character_index_in_sliding_window);
//         if (it->second.empty()) {
//             hash_table.erase(it);
//         }
//     }
// }
//
// lz_match HashTable::find_match(Program &program) {
//     auto *buffers = program.buffers;
//     lz_match best{NO, 0, 0};
//
//     // Compute the hash of the lookahead buffer (starting at index 0)
//     std::size_t hash_key = compute_hash(0, buffers->lookahead, LOOKAHEAD_SIZE);
//     auto mapIter = hash_table.find(hash_key);
//     if (mapIter == hash_table.end()) {
//         best.length = 1; // per original behavior when no match exists
//         return best;
//     }
//
//     // Iterate over candidate positions in the sliding window.
//     const auto &positions = mapIter->second;
//     for (auto pos: positions) {
//         std::size_t match_len = 0;
//         while (match_len < LOOKAHEAD_SIZE) {
//             std::size_t idx = wrap_index(pos + match_len, WINDOW_SIZE);
//             if (buffers->window[idx] != buffers->lookahead[match_len]) {
//                 break;
//             }
//             ++match_len;
//         }
//         if (match_len > best.length) {
//             best.length = match_len;
//             best.offset = pos;
//             if (match_len == LOOKAHEAD_SIZE) {
//                 break; // perfect match found
//             }
//         }
//     }
//
//     return best;
// }

lz_match brute_force_search(Program &program) {
    //    Buffer *buffers = program.buffers;
    char *window = program.buffers->window;
    char *lookahead = program.buffers->lookahead;
    std::size_t window_head = program.buffers->window_head;
    std::size_t lookahead_head = program.buffers->lookahead_head;
    lz_match match = {false, 0, 0};

    const auto length_of_window = strlen(window);
    const auto length_of_lookahead = strlen(lookahead);

    // Iterate over the entire sliding window to find the longest match
    for (std::size_t window_index_1 = 0; window_index_1 < WINDOW_SIZE && window_index_1 < length_of_window;
         window_index_1++) {
        std::size_t current_length = 0;
        // Continue comparing until the end of the lookahead or a mismatch
        auto is_possible_to_find_better_match = [&](std::size_t current_length_lambda) {
            const auto lookahead_char_1 = &lookahead[wrap_index(lookahead_head + current_length + 1, LOOKAHEAD_SIZE)];
            const auto result = (current_length_lambda < LOOKAHEAD_SIZE) && (strcmp(lookahead_char_1, "\0") != 0) &&
                                (current_length_lambda < length_of_lookahead);
            return result;
        };
        //        const auto is_possible_to_find_better_match =
        //            (current_length < LOOKAHEAD_SIZE) && (lookahead_char_1 != '\0') && (current_length <
        //            length_of_lookahead);
        while (is_possible_to_find_better_match(current_length)) {
            const auto window_index_2 = wrap_index(window_index_1 + current_length, WINDOW_SIZE);
            const auto window_char_2 = window[window_index_2];
            const auto lookahead_index_2 = wrap_index(lookahead_head + current_length + 1, LOOKAHEAD_SIZE);
            const auto lookahead_char_2 = lookahead[lookahead_index_2];
            const auto is_match = window_char_2 == lookahead_char_2;
            if (!is_match) { // Go to check next character
                break;
            }
            current_length++;
        }
        // Update best match if current match meets the minimum requirement and is longer than previous
        const auto is_better_match = current_length >= MIN_MATCH_LENGTH && current_length > match.length;
        if (is_better_match) {
            match.is_compressed = true;
            match.offset = window_index_1;
            match.length = current_length;
            const auto is_maximum_match_reached = current_length == LOOKAHEAD_SIZE;
            if (is_maximum_match_reached) { // Finish
                break;                      // Maximum possible match reached
            }
        }
    }
    return match;
}

void init_lookahead_buffer(Program &program) {
    DEBUG_PRINT("%c", '\n');
    auto *buffers = program.buffers;
    buffers->lookahead_head = 0;
    bool is_fine_to_continue = true;
    while (is_fine_to_continue) {
        program.files->get_char();
        const auto char_to_add = program.files->current_char;
        const auto index_to_add = wrap_index(buffers->lookahead_head, LOOKAHEAD_SIZE);
        buffers->lookahead[index_to_add] = char_to_add;
        CYCLIC_INCREMENT(buffers->lookahead_head, LOOKAHEAD_SIZE);

        //
        const auto whole_lookahead_buffer_was_loaded = program.buffers->lookahead_head == 0; // We are back at index 0
        is_fine_to_continue = !whole_lookahead_buffer_was_loaded && !program.files->EOF_reached;
    }
    //    DEBUG_PRINT_LITE("===Loaded lookahead buffer%c", '\n');
}

// token_t process_compression_result(Program &program, lz_match &match) {
//     DEBUG_PRINT_LITE("===Processing compression result%c", '\n');
//     auto output = std::string();
//
//     auto *buffers = program.buffers;
//     //    auto *hash_table = program.hash_table;
//     auto *files = program.files;
//     token_t token;
//
//     if (match.is_compressed == YES) {
//         assert(match.length >= MIN_MATCH_LENGTH);
//
//         for (std::size_t i = 0; i < match.length && !files->EOF_reached;
//              ++i) { // NOTE: Here can not be EOF_reached condition
//
//             // TODO: Update heads
//             //  TODO:          update_buffers(program);
//             // TODO: Recalculate hash table
//         }
//     } else {
//         assert(match.length < MIN_MATCH_LENGTH);
//         for (std::size_t i = 0; i < match.length && !files->EOF_reached; ++i) {
//             // Read new bytes and shift window and lookahead
//             files->read_char();
//             //            DEBUG_PRINT_LITE("Reading new byte: %c%c", files->current_char, '\n');
//
//             output += update_buffers(program);
//             // TODO: Recalculate hash table
//         }
//     }
//
//     return output;
// }

bool has_something_to_process(std::size_t i, lz_match &match, const char *current) {
    const auto foo = ((i < match.length) || (i < LITERAL_SIZE)) && (strcmp(current, "\0") != 0);
    return foo;
}

void process_end(Program &program, BitsetWriter &bitset_writer) {
    // NOTE: Also possible that lookahead_head at teh end and we read just a few char before '\0'
    //       but it's OK because we want to read from lookahead_head till '\0' and that's the end
    DEBUG_PRINT_LITE("Process end%c", '\n');

    Buffer *buffers = program.buffers;

    while (strcmp(buffers->get_lookahead_current(), "\0") != 0) {
        lz_match match = brute_force_search(program);
        if (match.is_compressed) {
            process_is_compressed(program, match, bitset_writer);
        } else { // match.is_compressed == NO
            process_is_literal(program, match, bitset_writer);
        }
    }

    // Write EOF token and flush bitset
    bitset_writer.write_bits('\0', 1);
    bitset_writer.flush();
}

//------------------------------------------------------------------------------
// LZSS
//------------------------------------------------------------------------------
// TODO:   static void compress_adaptive(const Program &program);
// TODO:   static decompress(const Program &program);
static void compress_static(Program &program) {
    DEBUG_PRINT("%c", '\n');
    Buffer *buffers = program.buffers;
    BitsetWriter bitset_writer(program);

    init_lookahead_buffer(program);

    int tmp_i = 0;
    while (true) {
        tmp_i++;

        lz_match match = brute_force_search(program);
//        DEBUG_PRINT_LITE("is_compressed: %d | offset: %zu | length: %zu | lookahead: %s | lookahead_head: %zu | window: %s | window_head: %zu%c",
//                         match.is_compressed, match.offset, match.length,
//                         buffers->lookahead
//                         '\n');

        if (match.is_compressed) {
            process_is_compressed(program, match, bitset_writer);
        } else { // match.is_compressed == NO
            process_is_literal(program, match, bitset_writer);
            //            DEBUG_PRINT_LITE("%d\n", buffers->window[buffers->window_head]);
        }

        if (program.files->EOF_reached) {
            DEBUG_PRINT("%s", "EOF: continue to process end\n");
            break;
        }

        //        buffers->print_lookahead();
    }

    // Process end
    process_end(program, bitset_writer);
}

//------------------------------------------------------------------------------
// Function
//------------------------------------------------------------------------------
// Wrap the index so that it “circles around” the buffer.
std::size_t wrap_index(std::size_t i, std::size_t buffer_size) { return i % buffer_size; }

/**
 * Source: https://github.com/MichaelDipperstein/lzss
 */
std::size_t compute_hash(std::size_t pos, char *buffer, std::size_t buffer_size) {
    std::size_t hash_key = 0;
    for (std::size_t i = 0; i <= (WINDOW_SIZE + 1); i++) {
        std::size_t idx = wrap_index(pos + i, buffer_size);
        hash_key = (hash_key << 5) ^ buffer[idx];
        hash_key %= HASH_TABLE_SIZE;
    }
    return hash_key;
}

Program *init_program(int argc, char **argv) {
    // ------------------
    // Initialization
    // ------------------
    auto *program = new Program();
    program->parse_arguments(argc, argv);
    if (DEBUG) {
        program->print_arguments();
    }
    auto input_file = program->args->get<std::string>("-i");
    auto output_file = program->args->get<std::string>("-o");
    auto *file = new File(input_file, output_file, *program);
    program->files = file;
    return program;
}

int main(int argc, char **argv) {
    Program *program;
    try {
        program = init_program(argc, argv);
    } catch (const std::exception &err) {
        std::cerr << err.what() << std::endl;
        return 1;
    }

    // ------------------
    // Run
    // ------------------
    try {
        if (program->args->get<bool>("-c") && !program->args->get<bool>("-a")) {
            compress_static(*program); // TODO
            // TODO: compress_adaptive(*program);
        } else if (program->args->get<bool>("-d")) {
            DEBUG_PRINT_LITE("TODO%c", '\n');
        }
        // TODO: decompress(*program);
    } catch (const std::exception &err) {
        // ------------------
        // Clean up and exit with error
        // ------------------
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        delete program;
        return 1;
    }

    // ------------------
    // Clean up
    // ------------------
    delete program;
    return 0;
}
