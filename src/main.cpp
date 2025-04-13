/**
 * @file lz_codec.cpp
 * @brief Dictionary-based LZSS compression and decompression with optional delta encoding and adaptive scanning.
 *
 * This project provides a command-line tool for compressing and decompressing grayscale image data using a dictionary
 * compression method (LZSS), with support for preprocessing (delta encoding) and adaptive image traversal.
 *
 * @author Zdeněk Lapeš
 * @date 26/03/2025
 */

// TODO
// - test on merlin
// - comment everything: headers for file, function class and everything else including parameters, describe each define
// and each constant properly
// - Finish docs

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
static const std::size_t FLAG_SIZE_BITS = 1;
static const std::size_t OFFSET_SIZE_BITS = 13; // 2^13 = 8192 bytes for search buffer
static const std::size_t LENGTH_SIZE_BITS = 5;  // 2^5 = 32 bytes for look-ahead buffer
static const std::size_t MIN_MATCH_LENGTH = 3;  // At least match 3 characters to do compression
static const std::size_t CHARACTER_SIZE_BITS = 8;
static const std::size_t ADAPTIVE_BLOCK_WIDTH = 16;
static const std::size_t ADAPTIVE_BLOCK_HEIGHT = 16;

//------------------------------------------------------------------------------
// Macros
//------------------------------------------------------------------------------
#define DEBUG_SHIFTING_BUFFERS_AND_READ_NEW_CHAR (0) /// Enable detailed buffer shift logging.
#define DEBUG_BRUTE_FORCE (0)                        /// Enable internal match search debug printouts.
#define DEBUG_BRUTE_FORCE_RESULT (0)                 /// Enable final match result debug output.
#define DEBUG_READ_HEADER (0)                        /// Enable header read debug logging.
#define DEBUG_WRITE_HEADER (0)                       /// Enable header write debug logging.
#define DEBUG_PRE_PROCESSING (0)                     /// Enable delta preprocessing debug logging.
#define INFO (0)                                     /// Enable informational logs.
#define VERBOSE (0)                                  /// Enable verbose mode.
#define DEBUG (0)                                    /// Enable all debug logging.

/**
 * @brief Simple debug logging macro (prints formatted message if DEBUG is enabled).
 */
#define DEBUG_PRINT_LITE(fmt, ...)                                                                                     \
    do {                                                                                                               \
        if (DEBUG)                                                                                                     \
            fprintf(stderr, fmt, __VA_ARGS__);                                                                         \
    } while (0)

/**
 * @brief Debug logging macro with source file, line, and function context.
 */
#define DEBUG_PRINT(fmt, ...)                                                                                          \
    do {                                                                                                               \
        if (DEBUG)                                                                                                     \
            fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, __LINE__, __func__, __VA_ARGS__);                            \
    } while (0)

/**
 * @brief Applies delta encoding on the input buffer in-place.
 *
 * Each byte is replaced by the difference between itself and the previous byte.
 *
 * @param[in,out] data Input data to be encoded (modified in place).
 */
void delta_encode(std::vector<uint8_t> &data) {
    if (DEBUG_PRE_PROCESSING) {
        std::cout << "Delta encoding" << std::endl;
    }
    for (size_t i = data.size() - 1; i > 0; --i) {
        data[i] = static_cast<uint8_t>(data[i] - data[i - 1]);
    }
}

/**
 * @brief Decodes a buffer that was previously delta-encoded.
 *
 * Reconstructs original data by performing cumulative sum of the differences.
 *
 * @param[in,out] data Delta-encoded data (modified in place).
 */
void delta_decode(std::vector<uint8_t> &data) {
    if (DEBUG_PRE_PROCESSING) {
        std::cout << "Delta decoding" << std::endl;
    }
    for (size_t i = 1; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>(data[i] + data[i - 1]);
    }
}

//------------------------------------------------------------------------------
// Structs
//------------------------------------------------------------------------------
/**
 * @struct lz_match
 * @brief Represents a found LZSS match in the sliding window.
 */
struct lz_match {
    bool found = false;
    std::size_t offset = 0;
    std::size_t length = 0;
};

/**
 * @class CompressionHeader
 * @brief Header structure to store metadata about compression.
 */
class CompressionHeader {
  public:
    unsigned padding_bits_count : 3; ///< Number of padding bits in last byte.
    unsigned mode : 1;               ///< 0 = static scan, 1 = adaptive.
    unsigned passage : 1;            ///< 0 = horizontal pass, 1 = vertical.
    unsigned is_file_compressed : 1; ///< 0 = raw copy, 1 = compressed.
    unsigned is_preprocessed : 1;    ///< 0 = no delta, 1 = delta encoded.
    unsigned width : 16;             ///< Image width in pixels.

    /**
     * @brief Check if static scanning mode.
     * @return true if mode == 0
     */
    bool get_is_static() const { return mode == 0; }

    /**
     * @brief Check if adaptive scanning mode.
     * @return true if mode == 1
     */
    bool get_is_adaptive() const { return mode == 1; }
    /**
     * @brief Check if horizontal adaptive scan.
     * @return true if passage == 0
     */
    bool get_is_horizontal() const { return passage == 0; }

    /**
     * @brief Check if vertical adaptive scan.
     * @return true if passage == 1
     */
    bool get_is_vertical() const { return passage == 1; }

    /**
     * @brief Check if file is compressed.
     * @return true if compressed
     */
    bool get_is_compressed() const { return is_file_compressed == 1; }

    /**
     * @brief Check if delta preprocessing was applied.
     * @return true if preprocessed
     */
    bool get_is_preprocessed() const { return is_preprocessed == 1; }

    /**
     * @brief Get image width from header.
     * @return width as integer
     */
    int get_width() const { return width; }
};

//------------------------------------------------------------------------------
// Forward declarations
//------------------------------------------------------------------------------

struct Buffer;

class Program;

class File;

//------------------------------------------------------------------------------
// Classes
//------------------------------------------------------------------------------
/**
 * @class Program
 * @brief Wrapper class that stores the entire program state (arguments, file handlers, buffers).
 */
class Program {
  public:
    argparse::ArgumentParser *args = nullptr; ///< Command-line argument parser
    File *files = nullptr;                    ///< Pointer to file manager
    Buffer *buffers = nullptr;                ///< Pointer to buffer system

    /**
     * @brief Constructor.
     */
    Program() {}

    /**
     * @brief Destructor (cleans up parser).
     */
    ~Program() {
        delete args; // TODO[fixme]: Segmentation fault
    }

    /**
     * @brief Parses arguments passed to the executable.
     * @param argc Argument count
     * @param argv Argument vector
     */
    void parse_arguments(int argc, char **argv) {
        args = new argparse::ArgumentParser("lz_codec");

        // Define arguments
        args->add_argument("-c").help("activate compression mode").default_value(false).implicit_value(true);
        args->add_argument("-d").help("activate decompression mode").default_value(false).implicit_value(true);
        args->add_argument("-m")
            .help("activate model for preprocessing input data")
            .default_value(false)
            .implicit_value(true);
        args->add_argument("-a").help("activate adaptive scanning mode").default_value(false).implicit_value(true);
        args->add_argument("-i").help("input file name").required();
        args->add_argument("-o").help("output file name").required();
        args->add_argument("-w")
            .help("image width (required for compression; must be >= 1)")
            .scan<'i', int>()
            .default_value(-1);

        // Parse the command-line arguments.
        try {
            args->parse_args(argc, argv);
            const auto output_file = args->get<std::string>("-o");
            const auto output_dir = std::filesystem::path(output_file).parent_path();
            // Check that output dir exists
            if (!std::filesystem::exists(output_dir)) {
                // Create output dir
                std::filesystem::create_directories(output_dir);
            }
        } catch (const std::runtime_error &err) {
            std::cerr << err.what() << std::endl;
            std::cerr << *args;
            exit(1);
        }
    }

    /**
     * @brief Retrieves image width from arguments.
     * @throws std::runtime_error if width <= 0
     * @return image width
     */
    int get_width() {
        int width = args->get<int>("-w");
        if (width <= 0) {
            throw std::runtime_error("Width must be > 0 when using adaptive mode.");
        }
        return width;
    }

    /**
     * @brief Whether static (non-adaptive) compression is requested.
     * @return true if -c and not -a
     */
    bool is_static_compress() {
        const bool is_sequential = args->get<bool>("-c") && !args->get<bool>("-a");
        return is_sequential;
    }

    /**
     * @brief Whether preprocessing model (delta) is enabled.
     * @return true if -m
     */
    bool is_preprocess() {
        const bool is_preprocess = args->get<bool>("-m");
        return is_preprocess;
    }

    /**
     * @brief Whether adaptive compression mode is requested.
     * @return true if -c and -a
     */
    bool is_adaptive_compress() {
        const bool is_adaptive = args->get<bool>("-c") && args->get<bool>("-a");
        return is_adaptive;
    }

    /**
     * @brief Whether decompression mode is selected.
     * @return true if -d
     */
    bool is_decompress() {
        const bool is_decompress = args->get<bool>("-d");
        return is_decompress;
    }

    /**
     * @brief Print arguments to stdout (only if verbose).
     */
    void print_arguments() {
        std::cout << "Program arguments:" << std::endl;
        std::cout << "-c | compress: " << args->get<bool>("-c") << std::endl;
        std::cout << "-d | pre_decompress: " << args->get<bool>("-d") << std::endl;
        std::cout << "-m | model: " << args->get<bool>("-m") << std::endl;
        std::cout << "-a | adaptive scanning: " << args->get<bool>("-a") << std::endl;
        std::cout << "-i | input file: " << args->get<std::string>("-i") << std::endl;
        std::cout << "-o | output file: " << args->get<std::string>("-o") << std::endl;
        std::cout << "-w | width: " << args->get<int>("-w") << std::endl;
    }
};

/**
 * @struct Buffer
 * @brief Holds the sliding window and lookahead buffer for LZSS compression.
 *
 * This structure is used during compression to store the current sliding window
 * and the lookahead buffer. It provides debug utilities and a brute-force matching method.
 */
struct Buffer {
    std::deque<uint8_t> window;                               ///< Sliding window for LZSS matching.
    std::deque<uint8_t> lookahead;                            ///< Lookahead buffer for incoming characters.
    std::size_t max_window_size = (1 << OFFSET_SIZE_BITS);    ///< Maximum size of the sliding window.
    std::size_t max_lookahead_size = (1 << LENGTH_SIZE_BITS); ///< Maximum size of the lookahead buffer.

    /**
     * @brief Default constructor. Initializes sizes and optionally prints debug info.
     */
    Buffer() {
        if (DEBUG) {
            DEBUG_PRINT_LITE("max_window_size: %zu | max_lookahead_size: %zu\n", max_window_size, max_lookahead_size);
        }
    }

    /**
     * @brief Destructor.
     */
    ~Buffer() {}

    /**
     * @brief Prints both window and lookahead buffer for debugging.
     * @param msg Message to prefix the debug output with.
     */
    void debug_print_buffers(const std::string &msg) {
        std::cout << "----------------" << std::endl << msg << std::endl;
        this->debug_print_window();
        this->debug_print_lookahead();
    }

    /**
     * @brief Prints the contents of the window buffer.
     */
    void debug_print_window() {
        std::string output(window.begin(), window.end());
        std::cout << "Window (size: " << window.size() << "):\n" << output << std::endl;
    }

    /**
     * @brief Prints the contents of the lookahead buffer.
     */
    void debug_print_lookahead() {
        std::string output(lookahead.begin(), lookahead.end());
        std::cout << "Lookahead (size: " << lookahead.size() << "):\n" << output << std::endl;
    }

    /**
     * @brief Finds the longest match between the window and the lookahead buffer.
     * @return A lz_match struct containing match information (offset, length, found).
     */
    lz_match brute_force_search() {
        lz_match match = {false, 0, 0};

        if (VERBOSE && DEBUG_BRUTE_FORCE) {
            std::cout << "Brute force" << std::endl;
        }

        if (DEBUG_BRUTE_FORCE) {
            debug_print_buffers("Buffers: ");
        }

        // Iterate over each possible starting position in the window.
        for (std::size_t i = 0; i < window.size(); i++) {
            std::size_t match_length = 0;
            // Compare the window starting at 'i' with the lookahead buffer.
            // NOTE: here must be -1 in: lookahead.size()-1 (because when decompressing it overflow the 5 bits so max
            // match length is 31 chars nto whole 32 chars)
            bool can_continue = match_length < lookahead.size() - 1 && (i + match_length) < window.size() &&
                                window[i + match_length] == lookahead[match_length];
            while (can_continue) {
                if (DEBUG_BRUTE_FORCE) {
                    std::cout << "|match_length: " << match_length << " | i: " << i
                              << " | window[i+match_length]: " << window[i + match_length]
                              << " | lookahead[match_length]: " << lookahead[match_length] << "|\n";
                }
                match_length++;
                can_continue = match_length < lookahead.size() - 1 && (i + match_length) < window.size() &&
                               window[i + match_length] == lookahead[match_length];
            }
            // Update best_match if a longer sequence is found.
            if (match_length > match.length) {
                match.found = true;
                match.length = match_length;
                // Offset is defined as the distance from the end of the window.
                match.offset = window.size() - i - 1;
            }

            // Stop if nothing longer can be found
            if (match.length == max_lookahead_size - 1) {
                break;
            }
        }
        if (match.length < MIN_MATCH_LENGTH) {
            match.found = false;
        }

        if (DEBUG_BRUTE_FORCE_RESULT) {
            std::cout << "|is_compressed: " << match.found << " | offset: " << match.offset
                      << " | length: " << match.length << "|" << std::endl;
        }
        return match;
    }
};

/**
 * @class File
 * @brief Manages input/output file reading and writing during compression/decompression.
 *
 * Provides functionality for block-based reading, writing, transposition, delta preprocessing,
 * and handling adaptive compression blocks.
 */
class File {
  public:
    /**
     * @brief Constructor. Loads input file and prepares output stream.
     * @param in_filepath Path to input file.
     * @param out_filepath Path to output file.
     * @param program Reference to the current program configuration.
     */
    File(std::string in_filepath, std::string out_filepath, Program &program) : program(program) {
        // Check if file exists
        if (!std::filesystem::exists(in_filepath)) {
            throw std::runtime_error("Input file does not exist");
        }
        out = std::ofstream(out_filepath, std::ios::binary);
        auto [buffer, buffer_size] = read_binary_file_to_char_array(in_filepath);
        this->buffer = reinterpret_cast<uint8_t *>(buffer);
        this->buffer_size = buffer_size;

        const bool is_empty_file = buffer_size == 0;
        if (is_empty_file) {
            this->EOF_reached = true;
        }

        // Print each byte in its bit representation.
        if (DEBUG) {
            std::cout << "Buffer bytes in bits (buffer size: " << buffer_size << "):" << std::endl;
            for (std::size_t i = 0; i < buffer_size; i++) {
                // Create a bitset representing the 8 bits of the byte.
                std::bitset<8> bits(static_cast<unsigned char>(buffer[i]));
                // Print the byte index, its character value (if printable) and the bits.
                std::cout << "buffer[" << i << "]: " << bits << std::endl;
            }
        }
    }

    /**
     * @brief Destructor. Closes file streams and frees buffer memory.
     */
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

        delete[] buffer;
    }

    /**
     * @brief Reads a binary file and returns a character array with its contents.
     * @param filename Path to the input file.
     * @return Tuple with pointer to data and its size.
     */
    std::tuple<char *, std::size_t> read_binary_file_to_char_array(const std::string &filename) {
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

    /**
     * @brief Prepares blocks for adaptive compression, including transposition and optional delta encoding.
     * @param image_width Width of the image in pixels.
     */
    void prepare_adaptive_blocks_for_compression(int image_width) {
        const int block_size = ADAPTIVE_BLOCK_WIDTH * ADAPTIVE_BLOCK_HEIGHT;

        if (DEBUG) {
            DEBUG_PRINT_LITE("Preparing adaptive blocks - image width: %d | buffer_size: %zu\n", image_width,
                             buffer_size);
        }

        adaptive_blocks.clear();
        std::vector<uint8_t> block;

        for (std::size_t i = 0; i < buffer_size; ++i) {
            block.push_back(buffer[i]);

            // If block is full, optionally transpose and store
            if (block.size() == block_size) {
                if (read_vertically) {
                    block = transpose_block(block);
                }

                // Delta encode if preprocessing is enabled
                if (program.is_preprocess()) {
                    delta_encode(block);
                }

                adaptive_blocks.push_back(block);
                block.clear();
            }
        }

        // Handle last incomplete block (if any)
        if (!block.empty()) {
            if (read_vertically) {
                block = transpose_block(block);
            }

            if (program.is_preprocess()) {
                delta_encode(block);
            }

            adaptive_blocks.push_back(block);
        }

        if (DEBUG) {
            DEBUG_PRINT_LITE("Adaptive blocks (count: %zu):\n", adaptive_blocks.size());
            for (std::size_t i = 0; i < adaptive_blocks.size(); ++i) {
                std::cout << "Block " << i << " (size: " << adaptive_blocks[i].size() << "): ";
                for (char c : adaptive_blocks[i]) {
                    std::cout << c;
                }
                std::cout << std::endl;
            }
        }
    }

    /**
     * @brief Prepares blocks for adaptive compression, including transposition and optional delta encoding.
     * @param image_width Width of the image in pixels.
     */
    void prepare_adaptive_blocks_for_decompression(int image_width, CompressionHeader &header) {
        const int block_size = ADAPTIVE_BLOCK_WIDTH * ADAPTIVE_BLOCK_HEIGHT;

        DEBUG_PRINT_LITE("Preparing adaptive blocks from written data - image width: %d | total bytes: %zu\n",
                         image_width, written_data.size());

        adaptive_blocks.clear();
        std::vector<uint8_t> block;

        for (std::size_t i = 0; i < written_data.size(); ++i) {
            block.push_back(written_data[i]);

            if (block.size() == block_size) {
                if (read_vertically) {
                    block = transpose_block(block);
                }

                // Decode if preprocessing was used
                if (header.get_is_preprocessed()) {
                    delta_decode(block);
                }

                adaptive_blocks.push_back(block);
                block.clear();
            }
        }

        // Handle last partial block
        if (!block.empty()) {
            if (read_vertically) {
                block = transpose_block(block);
            }

            if (program.is_preprocess()) {
                delta_decode(block);
            }

            adaptive_blocks.push_back(block);
        }

        if (DEBUG) {
            DEBUG_PRINT_LITE("Adaptive blocks prepared: %zu blocks\n", adaptive_blocks.size());
        }
    }

    /**
     * @brief Transposes a single image block (e.g., from row-major to column-major).
     * @param block Block of pixels to transpose.
     * @return Transposed block.
     */
    std::vector<uint8_t> transpose_block(const std::vector<uint8_t> &block) {
        std::vector<uint8_t> result(ADAPTIVE_BLOCK_HEIGHT * ADAPTIVE_BLOCK_WIDTH, 0);
        for (std::size_t y = 0; y < ADAPTIVE_BLOCK_HEIGHT; ++y) {
            for (std::size_t x = 0; x < ADAPTIVE_BLOCK_WIDTH; ++x) {
                result[x * ADAPTIVE_BLOCK_HEIGHT + y] = block[y * ADAPTIVE_BLOCK_WIDTH + x];
            }
        }
        return result;
    }

    /**
     * @brief Reads next character from adaptive block, initializing if necessary.
     * @return Next character.
     */
    char get_char_adaptive() {
        // Lazy initialization of adaptive blocks if they haven't been prepared
        if (adaptive_blocks.empty()) {
            const int image_width = program.args->get<int>("-w");
            if (image_width <= 0) {
                throw std::runtime_error("Image width (-w) must be set and > 0 for adaptive reading.");
            }
            prepare_adaptive_blocks_for_compression(image_width);
        }

        std::vector<uint8_t> &block = adaptive_blocks[current_block_index];
        if (current_block_pos >= block.size()) {
            current_block_index++;
            current_block_pos = 0;
        }

        bool is_last_block = current_block_index + 1 >= adaptive_blocks.size();
        block = adaptive_blocks[current_block_index];
        bool is_over_block_size = current_block_pos + 1 >= block.size();
        const bool is_EOF_reached = is_last_block && is_over_block_size;
        if (is_EOF_reached) {
            EOF_reached = true;
        }
        const char _char = block[current_block_pos];
        current_block_pos++;
        return _char;
    }

    /**
     * @brief Reads next character sequentially from buffer.
     * @return Next character.
     */
    char get_char_sequential() {
        current_char = buffer[buffer_head];
        buffer_head++;
        if (buffer_head == buffer_size) {
            EOF_reached = true;
        }
        return current_char;
    }

    /**
     * @brief Reads the next character depending on compression mode.
     * @return Next character.
     */
    char get_char() {
        if (program.is_static_compress()) {
            const char _char = get_char_sequential();
            //            DEBUG_PRINT_LITE("STATIC get_char(): %c%c", current_char, '\n');
            return _char;
        }
        if (program.is_adaptive_compress()) {
            const char _char = get_char_adaptive();
            //            DEBUG_PRINT_LITE("ADAPTIVE get_char(): %c%c", current_char, '\n');
            return _char;
        }
        // For decompression
        const char _char = get_char_sequential();
        //        DEBUG_PRINT_LITE("DEFAULT get_char(): %c%c", current_char, '\n');
        return _char;
    }

    /**
     * @brief Resets internal pointers to simulate beginning of file read.
     */
    void seek_to_beginning_of_file() {
        EOF_reached = false;
        buffer_head = 0;
        current_block_index = 0;
        current_block_pos = 0;
        current_char = buffer[0];
        adaptive_blocks.clear();
    }

    /**
     * @brief Writes a single byte to internal buffer (not immediately to file).
     * @param in_byte Byte to write.
     */
    void write_char(uint8_t in_byte) {
        written_data.push_back(in_byte); // store before writing to file
    }


    /**
     * @brief Writes all adaptive blocks into the output file.
     * @param width Width of the image.
     */
    void write_decompressed_file(const int width) {
        if (!out.is_open()) {
            throw std::runtime_error("Output stream is not open.");
        }

        const int blocks_per_row = width / ADAPTIVE_BLOCK_WIDTH;
        const int total_blocks = adaptive_blocks.size();
        const int blocks_per_col = total_blocks / blocks_per_row;

        DEBUG_PRINT_LITE("Writing; block_per_row: %d | block_per_col: %d | total_blocks: %d\n", blocks_per_row,
                         blocks_per_col, total_blocks);

        for (auto block : adaptive_blocks) {

            for (const auto &char_in_block : block) {
                out.put(static_cast<char>(char_in_block));
            }
        }

        out.flush();
    }

    /**
     * @brief Flushes written_data to the output file for non-compressed data.
     */
    void flush_to_file_not_compressed() {
        if (!out.is_open()) {
            throw std::runtime_error("Output stream is not open.");
        }

        for (uint8_t byte : written_data) {
            out.put(static_cast<char>(byte));
        }

        out.flush(); // ensure all data is written
    }

    /**
     * @brief Validates if the image dimensions are compatible with block sizes.
     * @throws std::runtime_error if dimensions are invalid.
     */
    void is_image_format_ok() {
        const int width = program.args->get<int>("-w");

        if (width <= 0) {
            throw std::runtime_error("Invalid image width");
        }

        if (width % ADAPTIVE_BLOCK_WIDTH != 0) {
            throw std::runtime_error("Image width is not divisible by block width");
        }

        int height = static_cast<int>(buffer_size / width);

        if ((buffer_size % width) != 0) {
            throw std::runtime_error("Image buffer size: " + std::to_string(buffer_size) +
                                     " is not divisible by image width: " + std::to_string(width));
        }

        if (height % ADAPTIVE_BLOCK_HEIGHT != 0) {
            throw std::runtime_error("Image height is not divisible by block height");
        }
    }

    Program &program;                                  ///< Reference to associated program context.
    std::ofstream out;                                 ///< Output file stream.
    uint8_t current_char = '\0';                       ///< Most recently read character.
    bool EOF_reached = false;                          ///< Flag indicating if EOF was reached.
    uint8_t *buffer = nullptr;                         ///< Raw buffer from input file.
    std::size_t buffer_size;                           ///< Size of input buffer.
    std::vector<std::vector<uint8_t>> adaptive_blocks; ///< Image blocks (used in adaptive mode).
    unsigned long long int buffer_head = 0;            ///< Pointer to current byte in input.
    unsigned long long int current_block_index = 0;    ///< Current adaptive block index.
    unsigned long long int current_block_pos = 0;      ///< Position in current adaptive block.
    unsigned long long int block_size = 16 * 16;       ///< Block size (number of pixels).
    bool read_vertically = false;                      ///< Whether vertical transposition is enabled.
    std::vector<uint8_t> written_data;                 ///< Buffer storing output before writing.
};

/**
 * @class BitsetWriter
 * @brief Handles writing individual bits to a byte buffer and flushing to file.
 *
 * This class accumulates bits into a buffer (bitset<8>) and flushes them as bytes.
 * It also constructs and writes a compression header based on program parameters.
 */
class BitsetWriter {
  public:
    /**
     * @brief Constructs a BitsetWriter using a reference to the program.
     * @param program The Program object managing global state.
     */
    BitsetWriter(Program &program) : program(program), bits_filled(0), buffer(0), final_padding_bits(0) {}

    /**
     * @brief Writes `count` least significant bits from `bits` to the buffer.
     * @param bits The input bits as a 32-bit integer.
     * @param count The number of bits to write from MSB to LSB.
     */
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

    /**
     * @brief Retrieves the flushed byte stream.
     * @return Reference to vector of flushed bytes.
     */
    const std::vector<uint8_t> &get_flushed_bytes() const { return flushed_bytes; }

    /**
     * @brief Flushes remaining bits in the buffer by padding with zero bits.
     */
    void flush() {
        DEBUG_PRINT_LITE("!!!!!!!!!!!!!!!!!!!!!!!Flushing!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!%c", '\n');
        if (bits_filled > 0) {
            // Before flushing, record the final padding bits.
            flush_byte(true);
        }
    }

    /**
     * @brief Finalizes the buffer and writes the header + data to file.
     * @param is_vertical Whether data is read vertically (used in header).
     */
    void flush_to_file_after_compression(const bool is_vertical = false) {
        if (VERBOSE) {
            std::cout << "Flushing buffer after compression" << std::endl;
        }

        this->flush(); // Add padding bits

        if (VERBOSE) {
            std::cout << "1" << std::endl;
        }

        // Create and populate the header.
        CompressionHeader header;
        header.padding_bits_count = final_padding_bits; // Only 3 bits are used.
        header.mode = program.args->get<bool>("-a");
        header.passage = is_vertical;
        header.is_file_compressed = program.files->buffer_size > flushed_bytes.size();
        //        header.is_file_compressed = true;
        //                header.is_file_compressed = false;
        header.is_preprocessed = program.args->get<bool>("-m");
        //        header.is_preprocessed = true;
        //        header.is_preprocessed = false;
        const auto width = program.get_width();
        header.width = static_cast<unsigned>(width);

        if (VERBOSE) {
            std::cout << "2" << std::endl;
        }

        // Write the header as one byte. We pack header in the lower 3 bits.
        // We assume that header occupies the lower 3 bits and the upper bits are 0.
        // Split header into 3 bytes
        uint8_t byte1 = (header.padding_bits_count & 0b00000111) | ((header.mode & 0b1) << 3) |
                        ((header.passage & 0b1) << 4) | ((header.is_file_compressed & 0b1) << 5) |
                        ((header.is_preprocessed & 0b1) << 6);
        uint8_t byte2 = static_cast<uint8_t>((header.width >> 0) & 0xFF); // Lower 8 bits of width
        uint8_t byte3 = static_cast<uint8_t>((header.width >> 8) & 0xFF); // Upper 8 bits of width

        if (VERBOSE) {
            std::cout << "3" << std::endl;
        }

        if (DEBUG_WRITE_HEADER) {
            std::cout << "Padding: " << header.padding_bits_count << " | mode: " << bool(header.mode)
                      << " | passage: " << bool(header.passage)
                      << " | is_file_compressed: " << bool(header.is_file_compressed) << " | width: " << header.width
                      << "\n";
            std::cout << "Header bytes:\n";
            std::cout << "  byte1: " << std::bitset<8>(byte1) << "\n";
            std::cout << "  byte2: " << std::bitset<8>(byte2) << "\n";
            std::cout << "  byte3: " << std::bitset<8>(byte3) << "\n";
        }

        // BEFORE clean
        program.files->written_data.clear();

        if (VERBOSE) {
            std::cout << "4" << std::endl;
        }

        // Write 3 header bytes to file
        program.files->write_char(byte1);
        program.files->write_char(byte2);
        program.files->write_char(byte3);

        if (VERBOSE) {
            std::cout << "5" << std::endl;
        }

        // Write each flushed byte.
        if (header.get_is_compressed()) {
            if (VERBOSE) {
                std::cout << "Compressed" << std::endl;
            }
            for (std::size_t i = 0; i < flushed_bytes.size(); i++) {
                if (DEBUG) {
                    std::cout << "flushed_bytes[" << i << "]: " << std::bitset<8>(flushed_bytes[i]) << std::endl;
                }
                program.files->write_char(flushed_bytes[i]);
            }
        } else {
            if (VERBOSE) {
                std::cout << "Not compressed" << std::endl;
            }

            std::ifstream in_file(program.args->get<std::string>("-i"), std::ios::binary);
            if (!in_file.is_open()) {
                throw std::runtime_error("Failed to reopen input file for uncompressed copy.");
            }

            char byte;
            while (in_file.get(byte)) {
                program.files->write_char(static_cast<uint8_t>(byte));
            }

            in_file.close();
        }
        program.files->flush_to_file_not_compressed();

        if (VERBOSE) {
            std::cout << "END Flushing buffer after compression" << std::endl;
        }
    }

  private:
    /**
     * @brief Flushes current buffer to a byte and stores it in flushed_bytes.
     * @param is_final If true, padding bit count is calculated and stored.
     */
    void flush_byte(bool is_final) {
        if (is_final) {
            final_padding_bits = 8 - bits_filled;
        } else {
            final_padding_bits = 0;
        }
        if (buffer.size() < 8) {
            throw std::runtime_error("Buffer size is smaller than 8");
        }
        // Print debug information.
        //        std::cout << "Compressed bits (buffer: " << bits_filled << "): " << buffer << " | Real value: " <<
        //        static_cast<uint8_t>(buffer.to_ulong()) << std::endl; std::cout << "Compressed bits: " << buffer <<
        //        std::endl;

        // Convert the bitset to an unsigned long and cast it to uint8_t.
        uint8_t byte = static_cast<uint8_t>(buffer.to_ulong());
        // Store the byte in our vector.
        flushed_bytes.push_back(byte);
        //        if (DEBUG) {
        //            std::cout << "Compressed bits: " << buffer << " | flushed char in bits: " << std::bitset<8>(byte)
        //                      << " | flushed char in uint8_t: " << byte << std::endl;
        //        }
        // Reset the buffer and the counter.
        bits_filled = 0;
        buffer.reset();
    }

    Program &program;                   ///< Reference to Program context for file access and arguments.
    int bits_filled;                    ///< Current number of bits written to buffer (0–7).
    std::bitset<8> buffer;              ///< 8-bit buffer storing current byte being built.
    std::vector<uint8_t> flushed_bytes; ///< Flushed full bytes written from buffer.
    int final_padding_bits;             ///< Number of zero bits padded in the final flushed byte.
};

/**
 * @class BitsetReader
 * @brief Reads bits sequentially from a byte stream using internal bit buffering.
 *
 * This class reads bytes from file (via Program), extracts bits from MSB to LSB,
 * and reconstructs values of requested bit-width. It is used for decompression.
 */
class BitsetReader {
  public:
    /**
     * @brief Constructs a BitsetReader.
     * @param program Reference to global Program context.
     * @param header Compression header, used for interpreting padding bits.
     */
    BitsetReader(Program &program, CompressionHeader &header) : program(program), bits_remaining(0), header(header) {}

    /**
     * @brief Reads `count` bits from the stream (MSB first).
     * @param count Number of bits to read.
     * @return The bits interpreted as an unsigned integer.
     */
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

    /**
     * @brief Checks if reader is exactly at EOF (including accounting for padding bits).
     * @return True if EOF is reached and no meaningful bits remain.
     */
    bool is_at_the_end_of_file() const {
        const auto is_at_the_end = program.files->buffer_head >= program.files->buffer_size;
        const auto is_at_the_end_exact_remaining_bits = header.padding_bits_count == bits_remaining;
        const auto result = is_at_the_end && is_at_the_end_exact_remaining_bits;
        //        const auto result = is_at_the_end;
        return result;
    }

  private:
    Program &program;          ///< Reference to Program for reading characters.
    std::bitset<8> buffer;     ///< 8-bit buffer holding current byte of bits.
    int bits_remaining;        ///< Number of bits remaining unread in `buffer`.
    CompressionHeader &header; ///< Reference to the compression header.
};

/**
 * @brief Initializes the lookahead buffer with characters from the input file.
 *
 * Fills the lookahead buffer up to its maximum capacity, stopping early if the end
 * of the file is reached.
 *
 * @param program Reference to the global Program instance.
 */
void init_lookahead_buffer(Program &program) {
    Buffer *buffers = program.buffers;

    for (std::size_t i = 0; i < buffers->max_lookahead_size && !program.files->EOF_reached; i++) {
        char char_to_add = program.files->get_char();
        //        DEBUG_PRINT_LITE("Adding char to lookahead: %c%c", char_to_add, '\n');
        buffers->lookahead.push_back(char_to_add); // TODO[fixme]: SIGSEGV
    }

    if (DEBUG) {
        buffers->debug_print_buffers("After initialization");
    }
}

/**
 * @brief Initializes the lookahead buffer with characters from the input file.
 *
 * Fills the lookahead buffer up to its maximum capacity, stopping early if the end
 * of the file is reached.
 *
 * @param program Reference to the global Program instance.
 */
namespace StaticProcessor {
/**
 * @brief Shifts characters between buffers and reads the next character from the file.
 *
 * Moves the front character of the lookahead buffer to the sliding window,
 * pops the oldest window character if necessary, and fetches a new character from file.
 *
 * @param program Reference to the global Program instance.
 */
void shift_buffers_and_read_new_char(Program &program) {
    auto *buffers = program.buffers;
    auto *files = program.files;

    //    if (DEBUG) {
    //        DEBUG_PRINT_LITE("Updating buffers BEFORE%c", '\n');
    //        buffers->debug_print_window();
    //        buffers->debug_print_lookahead();
    //    }

    char char_to_add = buffers->lookahead.front();
    buffers->lookahead.pop_front();
    if (buffers->window.size() >= buffers->max_window_size) {
        buffers->window.pop_front();
    }
    buffers->window.push_back(char_to_add);

    if (!files->EOF_reached) {
        const auto _char = files->get_char();
        buffers->lookahead.push_back(_char);
    }

    //    if (DEBUG) {
    //        DEBUG_PRINT_LITE("Updating buffers AFTER%c", '\n');
    //        buffers->debug_print_window();
    //        buffers->debug_print_lookahead();
    //        DEBUG_PRINT_LITE("------------------%c", '\n');
    //    }
}

/**
 * @brief Writes compressed (match) token using BitsetWriter and updates buffers.
 *
 * Writes a flag bit, match offset, and match length into the bitstream.
 * Then updates buffers based on the match length.
 *
 * @param program Reference to the global Program instance.
 * @param match The LZ match data (offset, length).
 * @param bitset_writer Writer to emit compressed bits into the output stream.
 */
void compress_compressed(Program &program, lz_match &match, BitsetWriter &bitset_writer) {
    bitset_writer.write_bits(1, FLAG_SIZE_BITS);
    bitset_writer.write_bits(match.offset, OFFSET_SIZE_BITS);
    bitset_writer.write_bits(match.length, LENGTH_SIZE_BITS);

    // Update buffers
    for (std::size_t i = 0; i < match.length; ++i) {
        shift_buffers_and_read_new_char(program);
    }
}

/**
 * @brief Writes literal characters as uncompressed tokens into the output.
 *
 * Emits two characters as literal tokens with a flag and 8-bit encoding each.
 * Advances buffers accordingly.
 *
 * @param program Reference to the global Program instance.
 * @param bitset_writer Writer to emit bits.
 */
void compress_literal(Program &program, BitsetWriter &bitset_writer) {
    Buffer *buffers = program.buffers;

    //    if (DEBUG) {
    //            DEBUG_PRINT_LITE("Updating buffers literal BEFORE%c", '\n');
    //            buffers->debug_print_window();
    //            buffers->debug_print_lookahead();
    //    }

    bitset_writer.write_bits(0, FLAG_SIZE_BITS);

    // Read
    char char1 = buffers->lookahead.front();
    shift_buffers_and_read_new_char(program);
    bitset_writer.write_bits(char1, CHARACTER_SIZE_BITS);
    //    if (DEBUG) {
    //            DEBUG_PRINT_LITE("char1: %c | char1_bits: %s\n", char1,
    //            std::bitset<8>(static_cast<uint8_t>(char1)).to_string().c_str());
    //    }
    //    char>(char1)).to_string().c_str());

    if (buffers->lookahead.empty()) {
        if (DEBUG) {
            std::cout << "Finish lookahead is empty" << std::endl;
        }
        return;
    }

    char char2 = buffers->lookahead.front();
    shift_buffers_and_read_new_char(program);
    bitset_writer.write_bits(char2, CHARACTER_SIZE_BITS);
    //    if (DEBUG) {
    //            DEBUG_PRINT_LITE("char2: %c | char2_bits: %s\n", char2,
    //            std::bitset<8>(static_cast<uint8_t>(char2)).to_string().c_str());
    //    }
    //    char>(char2)).to_string().c_str());

    // if (DEBUG) {
    //         DEBUG_PRINT_LITE("Updating buffers literal AFTER%c", '\n');
    //         buffers->debug_print_window();
    //         buffers->debug_print_lookahead();
    //     }
}

/**
 * @brief Performs full LZSS compression in static mode.
 *
 * Initializes buffers, processes input with matching or literal encoding,
 * and flushes output data and header using BitsetWriter.
 *
 * @param program Reference to the global Program instance.
 */
void compress(Program &program) {
    //    DEBUG_PRINT("%c", '\n');
    Buffer *buffers = program.buffers;
    File *files = program.files;
    BitsetWriter bitset_writer(program);

    if (program.is_preprocess() && program.is_static_compress()) {
        const auto data = new std::vector<uint8_t>(files->buffer, files->buffer + files->buffer_size);
        delta_encode(*data);
        memcpy(files->buffer, data->data(), files->buffer_size);
        delete data;
    }

    init_lookahead_buffer(program);

    int tmp_i = 0;
    while (!program.buffers->lookahead.empty()) {
        tmp_i++;
        lz_match match = buffers->brute_force_search();

        if (DEBUG_SHIFTING_BUFFERS_AND_READ_NEW_CHAR) {
            buffers->debug_print_buffers("==Before shifting buffers and reading new char | tmp_i: " +
                                         std::to_string(tmp_i));
        }

        if (match.found) {
            StaticProcessor::compress_compressed(program, match, bitset_writer);
        } else {
            StaticProcessor::compress_literal(program, bitset_writer);
        }

        if (DEBUG_SHIFTING_BUFFERS_AND_READ_NEW_CHAR) {
            buffers->debug_print_buffers("==After shifting buffers and reading new char | tmp_i: " +
                                         std::to_string(tmp_i));
        }
    }

    // Process end
    //    process_end(program, bitset_writer);

    bitset_writer.flush_to_file_after_compression();
}

/**
 * @brief Handles decompression of a compressed match sequence.
 *
 * Reconstructs the sequence using the given offset and length by copying from
 * the sliding window buffer. Appends results to both output and window buffer.
 *
 * @param program Reference to the global Program instance.
 * @param offset Offset of the matched string from the current position.
 * @param length Length of the matched sequence.
 */
void decompress_compressed(Program &program, std::size_t offset, std::size_t length) {
    //    DEBUG_PRINT("Start%c", '\n');
    Buffer *buffers = program.buffers;

    // For each character in the match, copy from the sliding window.
    for (uint32_t i = 0; i < length; i++) {
        // Compute the starting position:
        // The token's offset is defined relative to the end of the current window.
        // It is assumed that the window contains at least 'offset' characters.
        if (buffers->window.empty() || buffers->window.size() < offset) {
            throw std::runtime_error("Invalid offset during decompression.");
        }

        std::size_t pos = buffers->window.size() - offset - 1;

        //        DEBUG_PRINT_LITE("Window size: %zu | offset: %d | pos: %d\n", buffers->window.size(), offset, pos);

        // For overlapping copies, recompute the source index on every iteration.
        char char1 = buffers->window[pos];
        std::bitset<8> bits(static_cast<unsigned char>(char1));
        if (DEBUG) {
            std::cout << "Decompressed bits: " << bits << " | char: " << char1 << std::endl;
        }

        // Write the character to the output file.
        program.files->write_char(static_cast<uint8_t>(char1));
        // Append the character to the sliding window.
        buffers->window.push_back(char1);
        // If the window exceeds its maximum size, remove the oldest character.
        if (buffers->window.size() > buffers->max_window_size) {
            buffers->window.pop_front();
        }
    }
}

/**
 * @brief Decompresses a single literal character from the input stream.
 *
 * Reads one 8-bit character, writes it to output, and appends it to the window.
 *
 * @param program Reference to the global Program instance.
 * @param bitset_reader Reader to fetch bits from the input stream.
 * @return The decoded character.
 */
char decompress_character(Program &program, BitsetReader &bitset_reader) {
    Buffer *buffers = program.buffers;

    // Get first char
    char char1 = bitset_reader.read_bits(CHARACTER_SIZE_BITS);
    std::bitset<8> bits(static_cast<unsigned char>(char1));

    if (DEBUG) {
        std::cout << "Decompressed bits: " << bits << " | char: " << char1 << std::endl;
    }

    // Update window
    program.files->write_char(static_cast<uint8_t>(char1));
    buffers->window.push_back(char1);
    if (buffers->window.size() > buffers->max_window_size) {
        buffers->window.pop_front();
    }
    return char1;
}

/**
 * @brief Performs full decompression of a static-mode LZSS encoded stream.
 *
 * Reads tokens using BitsetReader, processes either compressed or literal
 * sequences, and reconstructs the original file. Handles optional preprocessing.
 *
 * @param program Reference to the global Program instance.
 * @param header CompressionHeader object containing encoding metadata.
 */
void decompress(Program &program, CompressionHeader &header) {
    if (DEBUG) {
        DEBUG_PRINT_LITE("Decompress static%c", '\n');
    }
    BitsetReader bitset_reader(program, header);
    Buffer *buffers = program.buffers;
    buffers->window.clear();

    // Continue while there are still bytes or unread bit
    std::size_t tmp_i = 0;
    while (!bitset_reader.is_at_the_end_of_file()) {
        tmp_i++;
        uint32_t flag = bitset_reader.read_bits(FLAG_SIZE_BITS);

        // If no flag could be read, exit the loop.
        if (bitset_reader.is_at_the_end_of_file()) {
            if (INFO) {
                DEBUG_PRINT_LITE("!!!!!!!!!!Is at the end OUTER%c", '\n');
            }
            break;
        }

        if (flag == 1) { // Compressed token.
            uint32_t offset = bitset_reader.read_bits(OFFSET_SIZE_BITS);
            std::bitset<3> offset_bits(offset);
            uint32_t length = bitset_reader.read_bits(LENGTH_SIZE_BITS);
            std::bitset<3> length_bits(length);

            if (DEBUG) {
                std::cout << "------------------\nis_compressed: " << 1 << " | offset: " << offset
                          << " | offset bits: " << offset_bits << " | length: " << length
                          << " | length bits: " << length_bits << " | whole sequence: 1" << offset_bits << length_bits
                          << " | tmp_i: " << tmp_i << std::endl;
            }

            decompress_compressed(program, offset, length);
        } else { // Literal token.
                 //            if (DEBUG) {
                 //            DEBUG_PRINT_LITE("--------------\nis_compressed: %d\n", 0);
                 //            }
            decompress_character(program, bitset_reader);
            const bool should_stop = bitset_reader.is_at_the_end_of_file();
            if (should_stop) {
                if (INFO) {
                    DEBUG_PRINT_LITE("!!!!!!!!!!Is at the end INNER%c", '\n');
                }
                bitset_reader.is_at_the_end_of_file();
                break;
            }
            decompress_character(program, bitset_reader);
        }
    }

    if (DEBUG) {
        std::cout << "Width: " << header.width << std::endl;
    }

    if (header.get_is_preprocessed()) {
        delta_decode(program.files->written_data);
    }

    program.files->flush_to_file_not_compressed();
}
} // namespace StaticProcessor

/**
 * @namespace AdaptiveProcessor
 * @brief Contains compression and decompression logic for adaptive mode.
 *
 * This namespace handles adaptive scanning modes (horizontal and vertical),
 * evaluates both methods, and chooses the more efficient one (in terms of size).
 */

namespace AdaptiveProcessor {
/**
 * @brief Performs adaptive compression using horizontal scanning order.
 *
 * Initializes the file and buffer state, reads input block by block in horizontal
 * order, performs LZSS compression, and returns a BitsetWriter containing the result.
 *
 * @param program Reference to the global Program instance.
 * @return BitsetWriter containing the compressed byte stream.
 */
BitsetWriter compress_horizontal(Program &program) {
    auto *buffers = program.buffers;
    auto *file = program.files;
    BitsetWriter bitset_writer(program);

    if (DEBUG) {
        DEBUG_PRINT_LITE("==========================================================\ncompression horizontal %c", '\n');
    }

    file->seek_to_beginning_of_file();
    file->read_vertically = false;
    buffers->lookahead.clear();
    buffers->window.clear();
    init_lookahead_buffer(program);

    //    if (DEBUG) {
    //        buffers->debug_print_window();
    //        buffers->debug_print_lookahead();
    //    }

    int tmp_i = 0;
    while (!buffers->lookahead.empty()) {
        tmp_i++;
        lz_match match = buffers->brute_force_search();

        if (DEBUG_SHIFTING_BUFFERS_AND_READ_NEW_CHAR) {
            buffers->debug_print_buffers("==Before shifting buffers and reading new char | tmp_i: " +
                                         std::to_string(tmp_i));
        }

        if (match.found) {
            StaticProcessor::compress_compressed(program, match, bitset_writer);
        } else {
            StaticProcessor::compress_literal(program, bitset_writer);
        }

        if (DEBUG_SHIFTING_BUFFERS_AND_READ_NEW_CHAR) {
            buffers->debug_print_buffers("==After shifting buffers and reading new char | tmp_i: " +
                                         std::to_string(tmp_i));
        }
    }

    return bitset_writer;
}

/**
 * @brief Performs adaptive compression using vertical scanning order.
 *
 * Sets the program and file to read vertically, fills buffers, and performs LZSS
 * compression. Returns a BitsetWriter with the result.
 *
 * @param program Reference to the global Program instance.
 * @return BitsetWriter containing the vertically compressed byte stream.
 */
BitsetWriter compress_vertical(Program &program) {
    auto *buffers = program.buffers;
    auto *file = program.files;
    BitsetWriter bitset_writer(program);

    if (DEBUG) {
        DEBUG_PRINT_LITE("==========================================================\ncompression vertical %c", '\n');
    }

    file->seek_to_beginning_of_file();
    file->read_vertically = true;
    buffers->lookahead.clear();
    buffers->window.clear();
    init_lookahead_buffer(program);

    int tmp_i = 0;
    while (!buffers->lookahead.empty()) {
        tmp_i++;
        lz_match match = buffers->brute_force_search();

        if (DEBUG_SHIFTING_BUFFERS_AND_READ_NEW_CHAR) {
            buffers->debug_print_buffers("==Before shifting buffers and reading new char | tmp_i: " +
                                         std::to_string(tmp_i));
        }

        if (match.found) {
            StaticProcessor::compress_compressed(program, match, bitset_writer);
        } else {
            StaticProcessor::compress_literal(program, bitset_writer);
        }

        if (DEBUG_SHIFTING_BUFFERS_AND_READ_NEW_CHAR) {
            buffers->debug_print_buffers("==After shifting buffers and reading new char | tmp_i: " +
                                         std::to_string(tmp_i));
        }
    }

    return bitset_writer;
}

/**
 * @brief Chooses the better adaptive compression strategy (horizontal or vertical).
 *
 * Compares the size of horizontal and vertical compressed outputs, and writes the
 * more efficient one to the output file. This function performs both compressions
 * and selects the smallest output.
 *
 * @param program Reference to the global Program instance.
 */
void compress(Program &program) {
    BitsetWriter horizontal_writer = compress_horizontal(program);
    BitsetWriter vertical_writer = compress_vertical(program);
    //    horizontal_writer.write_all_to_file(false);
    //    vertical_writer.write_all_to_file(true);

    if (horizontal_writer.get_flushed_bytes().size() <= vertical_writer.get_flushed_bytes().size()) {
        if (DEBUG) {
            DEBUG_PRINT_LITE("Writing horizontal%c", '\n');
        }
        horizontal_writer.flush_to_file_after_compression(false);
    } else {
        if (DEBUG) {
            DEBUG_PRINT_LITE("Writing vertical%c", '\n');
        }
        vertical_writer.flush_to_file_after_compression(true);
    }
}

/**
 * @brief Performs adaptive decompression based on the metadata in the header.
 *
 * Reads compressed tokens (either literal or matched sequences) using BitsetReader,
 * and reconstructs the original image, considering transposed order if needed.
 * Final decompressed image is written block-by-block in raster scan order.
 *
 * @param program Reference to the global Program instance.
 * @param header CompressionHeader containing metadata like width, mode, and preprocessing flags.
 */
void decompress(Program &program, CompressionHeader &header) {
    //    if (DEBUG) {
    //        DEBUG_PRINT_LITE("Decompress adaptive%c", '\n');
    //    }

    BitsetReader bitset_reader(program, header);
    auto *file = program.files;
    auto *buffers = program.buffers;
    buffers->window.clear();

    //    if (DEBUG) {
    //        DEBUG_PRINT_LITE("Decompress static%c", '\n');
    //    }

    // Continue while there are still bytes or unread bit
    std::size_t tmp_i = 0;
    while (!bitset_reader.is_at_the_end_of_file()) {
        tmp_i++;
        uint32_t flag = bitset_reader.read_bits(FLAG_SIZE_BITS);

        // If no flag could be read, exit the loop.
        if (bitset_reader.is_at_the_end_of_file()) {
            //            if (DEBUG) {
            //            DEBUG_PRINT_LITE("!!!!!!!!!!Is at the end OUTER%c", '\n');
            //            }
            break;
        }

        if (flag == 1) { // Compressed token.
            uint32_t offset = bitset_reader.read_bits(OFFSET_SIZE_BITS);
            std::bitset<3> offset_bits(offset);
            uint32_t length = bitset_reader.read_bits(LENGTH_SIZE_BITS);
            std::bitset<3> length_bits(length);

            if (DEBUG) {
                std::cout << "------------------\nis_compressed: " << 1 << " | offset: " << offset
                          << " | offset bits: " << offset_bits << " | length: " << length
                          << " | length bits: " << length_bits << " | whole sequence: 1" << offset_bits << length_bits
                          << " | tmp_i: " << tmp_i << std::endl;
            }

            StaticProcessor::decompress_compressed(program, offset, length);
        } else { // Literal token.
            DEBUG_PRINT_LITE("--------------\nis_compressed: %d\n", 0);
            StaticProcessor::decompress_character(program, bitset_reader);
            const bool should_stop = bitset_reader.is_at_the_end_of_file();
            //            DEBUG_PRINT_LITE("should_stop: %d\n", should_stop);
            if (should_stop) {
                DEBUG_PRINT_LITE("!!!!!!!!!!Is at the end INNER%c", '\n');
                bitset_reader.is_at_the_end_of_file();
                break;
            }
            StaticProcessor::decompress_character(program, bitset_reader);
        }
    }

    file->adaptive_blocks.clear();
    if (DEBUG) {
        DEBUG_PRINT_LITE("Width: %d | Height: %zu\n", header.width, file->adaptive_blocks.size());
    }
    file->prepare_adaptive_blocks_for_decompression(header.width, header);
    if (DEBUG) {
        DEBUG_PRINT_LITE("written_data size: %zu\n", file->written_data.size());
    }

    // Clear written_data before restoring to correct order
    //    file->written_data.clear();

    // If originally transposed, reverse it
    if (header.get_is_vertical()) {
        for (auto &block : file->adaptive_blocks) {
            block = file->transpose_block(block);
        }
    }

    // Write pixels block by block in raster scan order
    file->write_decompressed_file(header.width);
}
} // namespace AdaptiveProcessor

/**
 * @brief Handles decompression of files that were not compressed.
 *
 * This function copies the file byte-by-byte using `get_char()` and `write_char()`
 * and flushes the result to disk.
 *
 * @param program Reference to the main Program object.
 * @param header The decompression header parsed from the file.
 */
void decompress_not_compressed(Program &program, CompressionHeader &header) {
    if (VERBOSE) {
        std::cout << "Decompress not compressed" << std::endl;
    }
    BitsetReader bitset_reader(program, header);
    auto *buffers = program.buffers;
    buffers->window.clear();
    while (!program.files->EOF_reached) {
        program.files->write_char(program.files->get_char());
    }
    program.files->flush_to_file_not_compressed();
}

/**
 * @brief Reads and reconstructs the compression header from the compressed file.
 *
 * This reads the first 3 bytes of the input file, extracts bitfields representing
 * compression flags, and builds a `CompressionHeader` object for use during decompression.
 *
 * @param program Reference to the main Program object.
 * @return CompressionHeader Structure filled with parsed metadata.
 */
CompressionHeader pre_decompress(Program &program) {
    uint8_t byte1 = static_cast<uint8_t>(program.files->get_char());
    uint8_t byte2 = static_cast<uint8_t>(program.files->get_char());
    uint8_t byte3 = static_cast<uint8_t>(program.files->get_char());

    if (VERBOSE) {
        std::cout << "Decompressing header" << std::endl;
    }

    CompressionHeader header;
    header.padding_bits_count = byte1 & 0b00000111;             // bits 0-2
    header.mode = (byte1 >> 3) & 0b1;                           // bit 3
    header.passage = (byte1 >> 4) & 0b1;                        // bit 4
    header.is_file_compressed = (byte1 >> 5) & 0b1;             // bit 5
    header.is_preprocessed = (byte1 >> 6) & 0b1;                // bit 6
    header.width = static_cast<unsigned>(byte2 | (byte3 << 8)); // 16-bit width

    std::bitset<8> b1(byte1), b2(byte2), b3(byte3);

    if (DEBUG_READ_HEADER) {
        std::cout << "Header bytes:\n";
        std::cout << "  byte1: " << b1 << " | padding_bits_count: " << int(header.padding_bits_count)
                  << " | mode: " << bool(header.mode) << " | passage: " << bool(header.passage)
                  << " | is_compressed: " << bool(header.is_file_compressed) << " | width: " << header.width
                  << " | is_preprocessed: " << bool(header.is_preprocessed) << std::endl;
        std::cout << "  byte2: " << b2 << "\n";
        std::cout << "  byte3: " << b3 << " | width: " << header.width << "\n";
    }

    return header;
}

/**
 * @brief Calculates and prints the compression ratio between two files.
 *
 * The ratio is calculated as `(compressed / original) * 100`, and printed if `DEBUG` is enabled.
 *
 * @param originalFile Path to the original (uncompressed) file.
 * @param compressedFile Path to the compressed file.
 */
void print_compression_ratio(const std::string &originalFile, const std::string &compressedFile) {
    std::uintmax_t originalSize = std::filesystem::file_size(originalFile);
    std::uintmax_t compressedSize = std::filesystem::file_size(compressedFile);
    double ratio = 100.0 * static_cast<double>(compressedSize) / originalSize;
    if (DEBUG) {
        std::cout << "Original file size: " << originalSize << " bytes" << std::endl;
        std::cout << "Compressed file size: " << compressedSize << " bytes" << std::endl;
        std::cout << "Compression ratio: " << ratio << "%\n";
    }
}

/**
 * @brief Initializes the `Program` structure including files and buffers.
 *
 * This function allocates and initializes:
 * - the main `Program` object
 * - its `File` wrapper (which loads the file into memory)
 * - its `Buffer` for LZSS compression
 *
 * Also verifies format if adaptive mode is used.
 *
 * @param argc Argument count from `main()`.
 * @param argv Argument vector from `main()`.
 * @return Pointer to a fully initialized `Program` object.
 */
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

    if (program->is_adaptive_compress()) {
        file->is_image_format_ok();
        if (DEBUG) {
            std::cout << "Image format is ok" << std::endl;
        }
    }

    return program;
}

/**
 * @brief Utility to print ASCII value of a given character.
 *
 * @param _char Character to convert to ASCII.
 */
void print_char_ascii_value(char _char) {
    std::cout << "The ASCII value of '" << _char << "' is: " << static_cast<int>(_char) << std::endl;
}

/**
 * @brief Entry point for the LZSS compression and decompression tool.
 *
 * Parses command-line arguments and determines which mode to run:
 * - Compression (static or adaptive)
 * - Decompression (based on header mode)
 *
 * Includes error handling, logging (via DEBUG), and memory cleanup.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return 0 on success, 1 on error.
 */
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
        if (program->is_static_compress()) {
            StaticProcessor::compress(*program);
        } else if (program->is_adaptive_compress()) {
            AdaptiveProcessor::compress(*program);
        } else if (program->is_decompress()) {
            CompressionHeader header = pre_decompress(*program);
            if (DEBUG) {
                std::cout << "Padding: " << int(header.padding_bits_count) << " | Mode: " << bool(header.mode)
                          << std::endl;
            }
            if (!header.get_is_compressed()) {
                decompress_not_compressed(*program, header);
            } else if (header.get_is_static()) {
                StaticProcessor::decompress(*program, header);
            } else if (header.get_is_adaptive()) {
                AdaptiveProcessor::decompress(*program, header);
            } else {
                throw std::runtime_error("Bad decompression format - Bad mode");
            }
        } else {
            throw std::runtime_error("Invalid arguments - run with -h for help.");
        }
    } catch (const std::exception &err) {
        // ------------------
        // Clean up and exit with error
        // ------------------
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        delete program->files;
        delete program->buffers;
        delete program;
        return 1;
    }

    // Print the compresstion ratio
    //    if (program->args->get<bool>("-c") && DEBUG) {
    //        std::string inputFile = program->args->get<std::string>("-i");
    //        std::string outputFile = program->args->get<std::string>("-o");
    //        print_compression_ratio(inputFile, outputFile);
    //    }

    // ------------------
    // Clean up
    // ------------------
    delete program->files;
    delete program->buffers;
    delete program;
    return 0;
}
