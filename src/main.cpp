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
#include <deque>
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

static const std::size_t OFFSET_SIZE_BITS = 13; // LZSS typical
static const std::size_t LENGTH_SIZE_BITS = 5;

// static const std::size_t WINDOW_SIZE = (1 << WINDOW_SIZE_BITS);
// static const std::size_t LOOKAHEAD_SIZE = (1 << LOOKAHEAD_SIZE_BITS);

static const std::size_t MIN_MATCH_LENGTH = 3;
static const std::size_t LITERAL_SIZE = MIN_MATCH_LENGTH - 1;
static const std::size_t LITERAL_SIZE_BITS = LITERAL_SIZE * 8;
static const std::size_t CHARACTER_SIZE_BITS = 8;
// static const std::size_t HASH_TABLE_SIZE = 8192; // Adjust as needed

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
    bool found = false;
    std::size_t offset = 0;
    std::size_t length = 0;
};

std::size_t wrap_index(std::size_t i, std::size_t buffer_size);

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
    argparse::ArgumentParser *args = nullptr;
    File *files = nullptr;
    Buffer *buffers = nullptr;

    // Constructor and Destructor.
    Program() {}

    ~Program() {
        delete args; // TODO[fixme]: Segmentation fault
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

/**
 * Buffer
 */
struct Buffer {
    std::deque<char> window;
    std::deque<char> lookahead;

    std::size_t max_window_size = 1 << OFFSET_SIZE_BITS;
    std::size_t max_lookahead_size = 1 << LENGTH_SIZE_BITS;

    Buffer() {
        // NOTE: We do nto need to resize buffers
        //        window.resize(max_window_size);
        //        lookahead.resize(max_lookahead_size);
    }

    ~Buffer() {}

    void debug_print_window() {
        if (DEBUG) {
            std::string output(window.begin(), window.end());
            std::cout << "Window (size: " << window.size() << "): " << output << std::endl;
        }
    }

    void debug_print_lookahead() {
        if (DEBUG) {
            std::string output(lookahead.begin(), lookahead.end());
            std::cout << "Lookahead (size: " << lookahead.size() << "): " << output << std::endl;
        }
    }

    lz_match brute_force_search() {
        lz_match best_match = {false, 0, 0};

        /// DEBUG
        //        DEBUG_PRINT_LITE("Brute force search BEFORE%c", '\n');
        //        this->debug_print_window();
        //        this->debug_print_lookahead();

        // Iterate over each possible starting position in the window.
        for (std::size_t i = 0; i < window.size(); i++) {
            std::size_t match_length = 0;
            // Compare the window starting at 'i' with the lookahead buffer.
            while (match_length < lookahead.size() && (i + match_length) < window.size() &&
                   window[i + match_length] == lookahead[match_length]) {
                match_length++;
            }
            // Update best_match if a longer sequence is found.
            if (match_length > best_match.length) {
                best_match.found = true;
                best_match.length = match_length;
                // Offset is defined as the distance from the end of the window.
                best_match.offset = window.size() - i;
            }
        }
        if (best_match.length < MIN_MATCH_LENGTH) {
            best_match.found = false;
        }

        /// DEBUG
        //        DEBUG_PRINT_LITE("Brute force search AFTER%c", '\n');
        //        this->debug_print_window();
        //        this->debug_print_lookahead();

        return best_match;
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
        const auto size_with_null_index = static_cast<size_t>(size) + 1;
        char *buffer = new char[size_with_null_index];
        memset(buffer, 0, size_with_null_index);

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
        out = std::ofstream(out_filepath, std::ios::binary);
        auto [buffer, buffer_size] = readBinaryFileToCharArray(in_filepath);
        this->buffer = buffer;
        this->buffer_size = buffer_size;

        const bool is_empty_file = buffer_size == 0;
        if (is_empty_file) {
            this->EOF_reached = true;
        }

        // Print buffer
        DEBUG_PRINT_LITE("Buffer size: %zu | buffer: ", buffer_size);
        std::cout << std::string(buffer, buffer_size) << std::endl;
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

    char get_char() {
        current_char = buffer[buffer_head];
        const long long unsigned int width = program.args->get<int>("-w");

        buffer_head++;
        if (buffer_head == buffer_size) {
            EOF_reached = true;
            return current_char;
        }

        return current_char;
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

// Helper class to read bits from the input file.
class BitsetReader {
  public:
    BitsetReader(Program &program) : program(program), bits_remaining(0) {}

    // Reads 'count' bits (from MSB side) and returns them as an unsigned integer.
    uint32_t read_bits(uint32_t count) {
        uint32_t result = 0;
        for (uint32_t i = 0; i < count; i++) {
            if (bits_remaining == 0) {
                if (program.files->EOF_reached) {
                    // If no more data is available, break early.
                    break;
                }
                // Read next byte from input (as an unsigned char).
                uint8_t next_byte = static_cast<uint8_t>(program.files->get_char());
                buffer = std::bitset<8>(next_byte);
                bits_remaining = 8;
            }
            // Read the next bit (from MSB side in our current byte)
            bool bit = buffer[bits_remaining - 1];
            bits_remaining--;
            result = (result << 1) | bit;
        }
        return result;
    }

  private:
    Program &program;
    std::bitset<8> buffer;
    int bits_remaining;
};

//------------------------------------------------------------------------------
// Functions
//------------------------------------------------------------------------------
char shift_buffers_and_read_new_char(Program &program) {
    auto *buffers = program.buffers;
    auto *files = program.files;

    /// DEBUG
    //    DEBUG_PRINT_LITE("Updating buffers BEFORE%c", '\n');
    //    buffers->debug_print_window();
    //    buffers->debug_print_lookahead();

    char char_to_add = buffers->lookahead.front();
    buffers->lookahead.pop_front();
    if (buffers->window.size() >= buffers->max_window_size) {
        buffers->window.pop_back();
    }
    buffers->window.push_back(char_to_add);

    if (!files->EOF_reached) {
        buffers->lookahead.push_back(files->get_char());
    }

    /// DEBUG
    //    DEBUG_PRINT_LITE("Updating buffers AFTER%c", '\n');
    //    buffers->debug_print_window();
    //    buffers->debug_print_lookahead();
    //    DEBUG_PRINT_LITE("------------------%c", '\n');
}

void process_is_compressed(Program &program, lz_match &match, BitsetWriter &bitset_writer) {
    DEBUG_PRINT("Start%c", '\n');

    bitset_writer.write_bits(1, FLAG_SIZE_BITS);
    bitset_writer.write_bits(match.offset, OFFSET_SIZE_BITS);
    bitset_writer.write_bits(match.length, LENGTH_SIZE_BITS);

    // Update buffers
    for (std::size_t i = 0; i < match.length; ++i) {
        shift_buffers_and_read_new_char(program);
    }
}

void process_is_literal(Program &program, lz_match &match, BitsetWriter &bitset_writer) {
    Buffer *buffers = program.buffers;

    DEBUG_PRINT("Start%c", '\n');

    /// DEBUG
    //    DEBUG_PRINT_LITE("Updating buffers literal BEFORE%c", '\n');
    //    buffers->debug_print_window();
    //    buffers->debug_print_lookahead();

    bitset_writer.write_bits(0, FLAG_SIZE_BITS);

    // Read
    char char1 = buffers->lookahead.front();
    shift_buffers_and_read_new_char(program);
    bitset_writer.write_bits(char1, CHARACTER_SIZE_BITS);

    if (buffers->lookahead.empty()) {
        return;
    }

    char char2 = buffers->lookahead.front();
    shift_buffers_and_read_new_char(program);
    bitset_writer.write_bits(char2, CHARACTER_SIZE_BITS);

    /// DEBUG
    //    DEBUG_PRINT_LITE("Updating buffers literal AFTER%c", '\n');
    //    buffers->debug_print_window();
    //    buffers->debug_print_lookahead();
}

void init_lookahead_buffer(Program &program) {
    //    DEBUG_PRINT("%c", '\n');
    Buffer *buffers = program.buffers;

    //    DEBUG_PRINT_LITE("Lookahead size: %zu%c", buffers->lookahead.size(), '\n');

    for (std::size_t i = 0; i < buffers->max_lookahead_size && !program.files->EOF_reached; i++) {
        char char_to_add = program.files->get_char();
        //        DEBUG_PRINT_LITE("Adding char to lookahead: %c%c", char_to_add, '\n');
        buffers->lookahead.push_back(char_to_add); // TODO[fixme]: SIGSEGV
    }

    const auto lookahead_string = std::string(buffers->lookahead.begin(), buffers->lookahead.end());
    /// DEBUG
    DEBUG_PRINT_LITE("After initialization%c", '\n');
    buffers->debug_print_window();
    buffers->debug_print_lookahead();
    DEBUG_PRINT_LITE("------------------%c", '\n');
}

// bool has_something_to_process(std::size_t i, lz_match &match, const char *current) {
//     const auto foo = ((i < match.length) || (i < LITERAL_SIZE)) && (strcmp(current, "\0") != 0);
//     return foo;
// }

void process_end(Program &program, BitsetWriter &bitset_writer) {
    // NOTE: Also possible that lookahead_head at teh end and we read just a few char before '\0'
    //       but it's OK because we want to read from lookahead_head till '\0' and that's the end
    Buffer *buffers = program.buffers;

    DEBUG_PRINT("Start%c", '\n');
    buffers->debug_print_window();
    buffers->debug_print_lookahead();
    DEBUG_PRINT_LITE("------------------%c", '\n');

    while (!buffers->lookahead.empty()) {
        lz_match match = buffers->brute_force_search();

        /// DEBUG
        buffers->debug_print_window();
        buffers->debug_print_lookahead();
        DEBUG_PRINT_LITE("is_compressed: %d | offset: %zu | length: %zu\n---------------\n", match.found, match.offset,
                         match.length);

        if (match.found) {
            process_is_compressed(program, match, bitset_writer);
        } else { // match.is_compressed == NO
            process_is_literal(program, match, bitset_writer);
        }
    }

    // Flush bitset (write last byte)
    bitset_writer.flush();
}

//------------------------------------------------------------------------------
// LZSS
//------------------------------------------------------------------------------
// TODO:   static void compress_adaptive(const Program &program);
// TODO:   static decompress(const Program &program);
void compress_static(Program &program) {
    //    DEBUG_PRINT("%c", '\n');
    Buffer *buffers = program.buffers;
    BitsetWriter bitset_writer(program);

    init_lookahead_buffer(program);

    int tmp_i = 0;
    while (!program.buffers->lookahead.empty()) {
        tmp_i++;
        lz_match match = buffers->brute_force_search();

        /// DEBUG
        DEBUG_PRINT_LITE("Before process match%c", '\n');
        buffers->debug_print_window();
        buffers->debug_print_lookahead();
        DEBUG_PRINT_LITE("is_compressed: %d | offset: %zu | length: %zu\n---------------\n", match.found, match.offset,
                         match.length);

        if (match.found) {
            process_is_compressed(program, match, bitset_writer);
        } else {
            process_is_literal(program, match, bitset_writer);
        }

        /// DEBUG
        DEBUG_PRINT_LITE("After process match%c", '\n');
        buffers->debug_print_window();
        buffers->debug_print_lookahead();
        DEBUG_PRINT_LITE("is_compressed: %d | offset: %zu | length: %zu\n---------------\n", match.found, match.offset,
                         match.length);

//        if (program.files->EOF_reached) {
//            DEBUG_PRINT("%s", "EOF: continue to process end\n");
//            break;
//        }
    }

    // Process end
//    process_end(program, bitset_writer);

    bitset_writer.flush();
}

void decompress_compressed(Program &program, BitsetReader &bitset_reader) {
    Buffer *buffers = program.buffers;

    uint32_t offset = bitset_reader.read_bits(OFFSET_SIZE_BITS);
    uint32_t length = bitset_reader.read_bits(LENGTH_SIZE_BITS);

    // For each character in the match, copy from the sliding window.
    for (uint32_t i = 0; i < length; i++) {
        // Compute the starting position:
        // The token's offset is defined relative to the end of the current window.
        // It is assumed that the window contains at least 'offset' characters.
        if (buffers->window.empty() || buffers->window.size() < offset) {
            throw std::runtime_error("Invalid offset during decompression.");
        }
        std::size_t pos = buffers->window.size() - offset;
        // For overlapping copies, recompute the source index on every iteration.
        char c = buffers->window[pos];
        // Write the character to the output file.
        program.files->write_char(static_cast<uint8_t>(c));
        // Append the character to the sliding window.
        buffers->window.push_back(c);
        // If the window exceeds its maximum size, remove the oldest character.
        if (buffers->window.size() > buffers->max_window_size) {
            buffers->window.pop_front();
        }
    }
}

void decompress_literal(Program &program, BitsetReader &bitset_reader) {
    Buffer *buffers = program.buffers;

    uint32_t literal = bitset_reader.read_bits(LITERAL_SIZE_BITS);
    // Extract two characters (8 bits each).
    char char1 = (literal >> 8) & 0xFF;
    char char2 = literal & 0xFF;
    // Write the literal characters.
    program.files->write_char(static_cast<uint8_t>(char1));
    program.files->write_char(static_cast<uint8_t>(char2));
    // Update the sliding window with each literal.
    buffers->window.push_back(char1);
    if (buffers->window.size() > buffers->max_window_size) {
        buffers->window.pop_front();
    }
    buffers->window.push_back(char2);
    if (buffers->window.size() > buffers->max_window_size) {
        buffers->window.pop_front();
    }
}

void decompress(Program &program) {
    BitsetReader bitset_reader(program);
    Buffer *buffers = program.buffers;
    // Clear any existing data in the sliding window.
    buffers->window.clear();

    // Continue until we reach the end of the compressed file.
    // (Note: The compressed file is read via BitsetReader using program.files->get_char())
    while (!program.files->EOF_reached) {
        // Read the flag bit.
        uint32_t flag = bitset_reader.read_bits(FLAG_SIZE_BITS);
        // If we weren't able to read a flag bit, break out.
        if (program.files->EOF_reached) {
            break;
        }
        if (flag == 1) { // Compressed token.
            decompress_compressed(program, bitset_reader);
        } else { // Literal token.
            decompress_literal(program, bitset_reader);
        }
    }
}

//------------------------------------------------------------------------------
// Function
//------------------------------------------------------------------------------
// Wrap the index so that it “circles around” the buffer.
std::size_t wrap_index(std::size_t i, std::size_t buffer_size) { return i % buffer_size; }

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
    auto buffers = new Buffer();
    program->buffers = buffers;
    return program;
}

int main(int argc, char **argv) {
    Program *program = nullptr;

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
            decompress(*program);
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
    delete program->buffers;
    delete program;
    return 0;
}
