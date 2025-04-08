//
// Created by Zdeněk Lapeš on 26/03/2025.
//

// TODO
// - Fix fread and usage of read_char and get_char
// - Try compress some raw file
// TODO:   static void compress_adaptive(const Program &program);
// TODO:   static decompress(const Program &program);

//------------------------------------------------------------------------------
// Includes
//------------------------------------------------------------------------------
#include "include/argparse/argparse.hpp"
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <filesystem> // NEW: include filesystem for file_size()
#include <fstream>
#include <iostream>
#include <tuple>
#include <vector>

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------
static const std::size_t FLAG_SIZE_BITS = 1; // LZSS typical

static const std::size_t OFFSET_SIZE_BITS = 13; // LZSS typical
static const std::size_t LENGTH_SIZE_BITS = 5;
// static const std::size_t LENGTH_SIZE_BITS = 6; // TODO

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

    std::size_t max_window_size = (1 << OFFSET_SIZE_BITS);
    std::size_t max_lookahead_size = (1 << LENGTH_SIZE_BITS);

    Buffer() {
        // NOTE: We do nto need to resize buffers
        //        window.resize(max_window_size);
        //        lookahead.resize(max_lookahead_size);
    }

    ~Buffer() {}

    void debug_print_window() {
        if (DEBUG) {
            std::string output(window.begin(), window.end());
            std::cout << "Window (size: " << window.size() << "):\n" << output << std::endl;
        }
    }

    void debug_print_lookahead() {
        if (DEBUG) {
            std::string output(lookahead.begin(), lookahead.end());
            std::cout << "Lookahead (size: " << lookahead.size() << "):\n" << output << std::endl;
        }
    }

    lz_match brute_force_search() {
        lz_match match = {false, 0, 0};

        /// DEBUG
        //        DEBUG_PRINT_LITE("Brute force search BEFORE%c", '\n');
        //        this->debug_print_window();
        //        this->debug_print_lookahead();

        // Iterate over each possible starting position in the window.
        for (std::size_t i = 0; i < window.size(); i++) {
            std::size_t match_length = 0;
            // Compare the window starting at 'i' with the lookahead buffer.
            // NOTE: here must be -1 in: lookahead.size()-1 (because when decompressing it overflow the 5 bits so max
            // match length is 31 chars nto whole 32 chars)
            while (match_length < lookahead.size() - 1 && (i + match_length) < window.size() &&
                   window[i + match_length] == lookahead[match_length]) {
                match_length++;
            }
            // Update best_match if a longer sequence is found.
            if (match_length > match.length) {
                match.found = true;
                match.length = match_length;
                // Offset is defined as the distance from the end of the window.
                match.offset = window.size() - i;
            }

            // Stop if nothing longer can be found
            if (match.length == max_lookahead_size - 1) {
                break;
            }
        }
        if (match.length < MIN_MATCH_LENGTH) {
            match.found = false;
        }

        /// DEBUG
        //        DEBUG_PRINT_LITE("Brute force search AFTER%c", '\n');
        //        this->debug_print_window();
        //        this->debug_print_lookahead();

        return match;
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

        //        DEBUG_PRINT_LITE("Buffer size: %zu | buffer: %s\n", buffer_size, buffer);

        // Print each byte in its bit representation.
        //        std::cout << "Buffer bytes in bits (buffer size: " << buffer_size << "):" << std::endl;
        //        for (std::size_t i = 0; i < buffer_size; i++) {
        //            // Create a bitset representing the 8 bits of the byte.
        //            std::bitset<8> bits(static_cast<unsigned char>(buffer[i]));
        //            // Print the byte index, its character value (if printable) and the bits.
        //            std::cout << "buffer[" << i << "]: " << bits << std::endl;
        //        }
        //        std::cout << std::string(buffer, buffer_size) << std::endl;
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

struct CompressionHeader {
    unsigned final_padding_bits : 3;
};

class BitsetWriter {
  public:
    BitsetWriter(Program &program) : program(program), bits_filled(0), buffer(0), final_padding_bits(0) {}

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
                flush_byte(false);
            }
        }
    }

    // Flush any remaining bits by padding with 0s.
    void flush() {
        if (bits_filled > 0) {
            // Before flushing, record the final padding bits.
            flush_byte(true);
        }
    }

    // Write the header and all stored flushed bytes to file.
    void write_all_to_file() {
        // Create and populate the header.
        CompressionHeader header;
        header.final_padding_bits = final_padding_bits; // Only 3 bits are used.

        // Write the header as one byte. We pack header in the lower 3 bits.
        // We assume that header occupies the lower 3 bits and the upper bits are 0.
        uint8_t header_byte = static_cast<uint8_t>(header.final_padding_bits);

        std::bitset<8> bits(header_byte);
        std::cout << "Header bits: " << bits << std::endl;

        program.files->write_char(header_byte);

        // Write each flushed byte.
        for (auto byte : flushed_bytes) {
            std::bitset<8> bits(byte);
            std::cout << "Byte bits: " << bits << std::endl;
            program.files->write_char(byte);
        }

        DEBUG_PRINT_LITE("Padding: %d\n", final_padding_bits);
    }

    // Optionally, a getter for the final padding bits.
    int get_final_padding_bits() const { return final_padding_bits; }

  private:
    // Flush the current buffer. If 'is_final' is true, record the padding bits.
    void flush_byte(bool is_final) {
        if (is_final) {
            final_padding_bits = 8 - bits_filled;
        } else {
            final_padding_bits = 0;
        }
        // Print debug information.
        //        std::cout << "Compressed bits (buffer: " << bits_filled << "): " << buffer << " | Real value: " <<
        //        static_cast<uint8_t>(buffer.to_ulong()) << std::endl; std::cout << "Compressed bits: " << buffer <<
        //        std::endl;

        // Convert the bitset to an unsigned long and cast it to uint8_t.
        uint8_t byte = static_cast<uint8_t>(buffer.to_ulong());
        // Store the byte in our vector.
        flushed_bytes.push_back(byte);
        // Reset the buffer and the counter.
        bits_filled = 0;
        buffer.reset();
    }

    Program &program;
    int bits_filled;                    // number of bits currently in the buffer (0 to 7)
    std::bitset<8> buffer;              // our 8-bit buffer
    std::vector<uint8_t> flushed_bytes; // stores all flushed bytes
    int final_padding_bits;             // how many bits were padded in the final byte
};

// Add a getter to expose remaining bits, if desired.
class BitsetReader {
  public:
    BitsetReader(Program &program, CompressionHeader *header) : program(program), bits_remaining(0), header(header) {}

    // Reads 'count' bits (from MSB side) and returns them as an unsigned integer.
    uint32_t read_bits(uint32_t count) {
        //        File *file = program.files;
        //        std::cout << "buffer[" << file->buffer_head << "]: " <<
        //        std::bitset<8>(file->buffer[file->buffer_head]) << std::endl;
        uint32_t result = 0;
        for (uint32_t i = 0; i < count; i++) {
            if (bits_remaining == 0) {
                // Instead of checking EOF_reached directly, check if the file position
                // is still within the file size.
                if (program.files->buffer_head >= program.files->buffer_size) {
                    // If we do not have any more bytes to refill, exit early.
                    break;
                }
                uint8_t next_byte = static_cast<uint8_t>(program.files->get_char());
                //                std::cout << "next_byte: " << std::bitset<8>(next_byte) << std::endl;
                buffer = std::bitset<8>(next_byte);
                bits_remaining = 8;
            }
            // Get the next bit from the buffer (from MSB side)
            bool bit = buffer[bits_remaining - 1];
            bits_remaining--;
            result = (result << 1) | bit;

            // Print bits of result
            //            std::cout << "Result: " << std::bitset<16>(result) << " | count: " << count << " |
            //            bits_remaining: " << bits_remaining << std::endl;
        }

        //        std::cout << "Buffer bytes in bits:" << std::endl;
        //        for (std::size_t i = 0; i < buffer_size; i++) {
        //            // Create a bitset representing the 8 bits of the byte.
        //            std::bitset<8> bits(static_cast<unsigned char>(buffer[i]));
        //            // Print the byte index, its character value (if printable) and the bits.
        //            std::cout << "Byte: " << bits << std::endl;
        //        }

        return result;
    }

    bool has_remaining_bits() const {
        const auto is_at_the_end = program.files->buffer_head == program.files->buffer_size;
        const auto is_at_the_end_with_remaining_bits = header->final_padding_bits == bits_remaining;
        const auto result = is_at_the_end && is_at_the_end_with_remaining_bits;
        return !result;
    }

  private:
    Program &program;
    std::bitset<8> buffer;
    int bits_remaining;
    CompressionHeader *header;
};

//------------------------------------------------------------------------------
// Functions
//------------------------------------------------------------------------------
void shift_buffers_and_read_new_char(Program &program) {
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

    //    DEBUG_PRINT("Start%c", '\n');

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
    //    DEBUG_PRINT_LITE("After initialization%c", '\n');
    //    buffers->debug_print_window();
    //    buffers->debug_print_lookahead();
    //    DEBUG_PRINT_LITE("------------------%c", '\n');
}

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
        //        DEBUG_PRINT_LITE("is_compressed: %d | offset: %zu | length: %zu\n---------------\n", match.found,
        //        match.offset,
        //                         match.length);
    }

    // Process end
    //    process_end(program, bitset_writer);

    bitset_writer.flush();
    bitset_writer.write_all_to_file();
}

void decompress_compressed(Program &program, BitsetReader &bitset_reader) {
    //    DEBUG_PRINT("Start%c", '\n');
    Buffer *buffers = program.buffers;

    uint32_t offset = bitset_reader.read_bits(OFFSET_SIZE_BITS);
    uint32_t length = bitset_reader.read_bits(LENGTH_SIZE_BITS);

//    if (offset == 8191) {
//        DEBUG_PRINT_LITE("%c\n", buffers->window[8192]);
//    }

    std::cout << "is_compressed: " << 1 << " | offset: " << offset << " | length: " << length << std::endl;

    // For each character in the match, copy from the sliding window.
    for (uint32_t i = 0; i < length; i++) {
        // Compute the starting position:
        // The token's offset is defined relative to the end of the current window.
        // It is assumed that the window contains at least 'offset' characters.
        if (buffers->window.empty() || buffers->window.size() < offset) {
            throw std::runtime_error("Invalid offset during decompression.");
        }

        std::size_t pos = buffers->window.size() - offset;

        DEBUG_PRINT_LITE("Window size: %zu | offset: %d | pos: %d\n", buffers->window.size(), offset, pos);

        // For overlapping copies, recompute the source index on every iteration.
        char char1 = buffers->window[pos];
        std::bitset<8> bits(static_cast<unsigned char>(char1));
        std::cout << "Decompressed bits: " << bits << " | char: " << char1 << std::endl;

        // Write the character to the output file.
        program.files->write_char(static_cast<uint8_t>(char1));
        // Append the character to the sliding window.
        buffers->window.push_back(char1);
        // If the window exceeds its maximum size, remove the oldest character.
        if (buffers->window.size() >= buffers->max_window_size) {
            buffers->window.pop_front();
        }
    }
}

void decompress_character(Program &program, BitsetReader &bitset_reader) {
    //    DEBUG_PRINT("Start%c", '\n');
    Buffer *buffers = program.buffers;

    // Get first char
    char char1 = bitset_reader.read_bits(CHARACTER_SIZE_BITS);
    std::bitset<8> bits(static_cast<unsigned char>(char1));
    std::cout << "Decompressed bits: " << bits << " | char: " << char1 << std::endl;

    // Update window
    program.files->write_char(static_cast<uint8_t>(char1));
    buffers->window.push_back(char1);
    if (buffers->window.size() > buffers->max_window_size) {
        buffers->window.pop_front();
    }
}

// --- Changed code in decompress ---
void decompress(Program &program) {
    uint8_t header_byte = static_cast<uint8_t>(program.files->get_char());
    CompressionHeader header;
    header.final_padding_bits = header_byte & 0x07; // Lower 3 bits
    std::cout << "Parsed header: final_padding_bits = " << int(header.final_padding_bits) << std::endl;

    BitsetReader bitset_reader(program, &header);
    Buffer *buffers = program.buffers;
    buffers->window.clear();

    // Continue while there are still bytes or unread bits
    while (bitset_reader.has_remaining_bits()) {
        uint32_t flag = bitset_reader.read_bits(FLAG_SIZE_BITS);
        // If no flag could be read, exit the loop.
        if (flag == 0 && !bitset_reader.has_remaining_bits() &&
            program.files->buffer_head >= program.files->buffer_size) {
            break;
        }
        if (flag == 1) { // Compressed token.
            decompress_compressed(program, bitset_reader);
        } else { // Literal token.
            DEBUG_PRINT_LITE("is_compressed: %d | offset: %zu | length: %zu\n", 0, 0, 0);
            decompress_character(program, bitset_reader);
            if (!bitset_reader.has_remaining_bits()) {
                DEBUG_PRINT_LITE("No more bits to decompress%c", '\n');
                bitset_reader.has_remaining_bits();
                break;
            }
            decompress_character(program, bitset_reader);
        }
    }
}

// NEW: Function to calculate and print compression ratio.
void print_compression_ratio(const std::string &originalFile, const std::string &compressedFile) {
    std::uintmax_t originalSize = std::filesystem::file_size(originalFile);
    std::uintmax_t compressedSize = std::filesystem::file_size(compressedFile);
    double ratio = 100.0 * static_cast<double>(compressedSize) / originalSize;
    std::cout << "Original file size: " << originalSize << " bytes" << std::endl;
    std::cout << "Compressed file size: " << compressedSize << " bytes" << std::endl;
    std::cout << "Compression ratio: " << ratio << "%\n";
}

//------------------------------------------------------------------------------
// Function
//------------------------------------------------------------------------------
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
    //    char c = '\n';
    //    std::cout << "The ASCII value of '" << c << "' is: " << static_cast<int>(c) << std::endl;
    //    exit(0);

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

    // Print the compresstion ratio
    if (program->args->get<bool>("-c") && DEBUG) {
        std::string inputFile = program->args->get<std::string>("-i");
        std::string outputFile = program->args->get<std::string>("-o");
        print_compression_ratio(inputFile, outputFile);
    }

    // ------------------
    // Clean up
    // ------------------
    delete program->buffers;
    delete program;
    return 0;
}
