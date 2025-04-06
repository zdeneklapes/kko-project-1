//
// Created by Zdeněk Lapeš on 26/03/2025.
//


//------------------------------------------------------------------------------
// Includes
//------------------------------------------------------------------------------
#include <iostream>
#include <fstream>
#include <vector>
#include "include/argparse/argparse.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <cassert>
#include <cstring>

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------
static const std::size_t WINDOW_SIZE = 4096; // LZSS typical
static const std::size_t LOOKAHEAD_SIZE = 20;   // LZSS typical
static const std::size_t MIN_MATCH_LENGTH = 5;
static const std::size_t HASH_TABLE_SIZE = 8192; // Adjust as needed


//------------------------------------------------------------------------------
// Macros
//------------------------------------------------------------------------------
#define CYCLIC_INCREMENT(head, size) ((head) = ((head) + 1) % (size))
#define CYCLIC_DECREMENT(head, size) ((head) = ((head) - 1) % (size))
#define DEBUG (1)
#define DEBUG_LITE (DEBUG)
#define DEBUG_PRINT_LITE(fmt, ...) \
            do { if (DEBUG_LITE) fprintf(stderr, fmt, __VA_ARGS__); } while (0)
#define DEBUG_PRINT(fmt, ...) \
        do { if (DEBUG) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
                                __LINE__, __func__, __VA_ARGS__); } while (0)


//------------------------------------------------------------------------------
// Structs
//------------------------------------------------------------------------------

enum Compressed {
    YES = 0,
    NO = 1,
};

/**
 * Simple structure to store match information.
 */
struct lz_match {
    enum Compressed is_compressed = NO;
    std::size_t offset = 0;
    std::size_t length = 0;
};

/**
 * Buffer
 */
struct Buffer {
    char *window = nullptr;
    char *lookahead = nullptr;
    std::size_t window_head = 0;
    std::size_t lookahead_head = 0;

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
};


//------------------------------------------------------------------------------
// Forward declarations
//------------------------------------------------------------------------------
class Buffer;

class Program;


class HashTable {
public:
    // Constructor and destructor.
    HashTable();

    ~HashTable();

    // Initializes the hash table using the current state of the sliding window.
    void init_search_structures(Program &program);

    // Inserts the substring starting at the given window index into the hash table.
    void add_string(std::size_t character_index_in_sliding_window, Program &program);

    // Removes the substring starting at the given window index from the hash table.
    void remove_string(std::size_t character_index_in_sliding_window, Program &program);

    // Finds the best match for the current lookahead buffer in the sliding window.
    lz_match find_match(Program &program);

    // The hash table: maps hash keys to a set of window positions that share that hash.
    std::map<std::size_t, std::set<std::size_t>> hash_table;
};

class File;

std::size_t wrap_index(std::size_t i, std::size_t buffer_size);

std::size_t compute_hash(std::size_t pos, char *buffer, std::size_t buffer_size);


//------------------------------------------------------------------------------
// Classes
//------------------------------------------------------------------------------
class Program {
public:
    // Heap-allocated pointer to the ArgumentParser instance.
    argparse::ArgumentParser *args;
    File *files;
    Buffer *buffers;
    HashTable *hash_table;

    // Constructor and Destructor.
    Program() : args(nullptr), buffers(new Buffer()), hash_table(new HashTable()) {}

    ~Program() {
        delete args;
        delete buffers;
        delete files; // Close files
        delete hash_table;
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
        args->add_argument("-v", "--verbose").help("increase output verbosity").default_value(false).implicit_value(true);

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
        std::cout << "Program arguments:" << std::endl;
        DEBUG_PRINT_LITE("-c: %d\n", args->get<bool>("-c"));
        DEBUG_PRINT_LITE("-d: %d\n", args->get<bool>("-d"));
        DEBUG_PRINT_LITE("-m: %d\n", args->get<bool>("-m"));
        DEBUG_PRINT_LITE("-a: %d\n", args->get<bool>("-a"));
        DEBUG_PRINT_LITE("-i: %s\n", args->get<std::string>("-i").c_str());
        DEBUG_PRINT_LITE("-o: %s\n", args->get<std::string>("-o").c_str());
        DEBUG_PRINT_LITE("-w: %d\n", args->get<int>("-w"));
        DEBUG_PRINT_LITE("-v: %d\n", args->get<bool>("-v"));
    }
};

class File {
public:
    // Retrieve input/output filenames from Program
    std::ifstream in;
    std::ofstream out;
    uint8_t current_char = '\0';
    bool EOF_reached = false;  // track if we hit EOF
#if DEBUG
    std::size_t current_pos = 0;
#endif

    File(std::string in_filepath, std::string out_filepath) {
        // Check if file exists
        if (!std::filesystem::exists(in_filepath)) {
            throw std::runtime_error("Input file does not exist");
        }
        in = std::ifstream(in_filepath, std::ios::binary);
        out = std::ofstream(out_filepath, std::ios::binary);
    }

    ~File() {
        // Explicitly flush and close the output stream.
        if (out.is_open()) {
            out.flush();
            out.close();
        }
        // Close the input stream.
        if (in.is_open()) {
            // (ifstream typically does not buffer output, but we still close it)
            in.close();
        }

    }

    bool read_char() {
#if DEBUG
        current_pos++;
#endif
        int c = this->in.get();
        if (c == EOF) {
            current_char = static_cast<uint8_t>(c);
            EOF_reached = true;
            return false; // no more data
        }
        current_char = static_cast<uint8_t>(c);
        return true;
    }

    bool write_char(uint8_t in_byte) {
        // Debug print with a correct format (one %c and a newline)
        DEBUG_PRINT_LITE("Writing byte: %c\n", in_byte);

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


// Constructor: (if you need to initialize members, do it here)
HashTable::HashTable() {
    // You may leave this empty if nothing special is needed.
}

// Destructor
HashTable::~HashTable() {
    // Clean-up if required.
}

void HashTable::init_search_structures(Program &program) {
    // Assume that Program has a pointer 'buffers' of type Buffer*
    // and that the sliding window (buffers->window) is pre-filled with a known character,
    // for example, a space ' '.
    auto *buffers = program.buffers;
    hash_table.clear();
    std::size_t hash_key = compute_hash(0, buffers->window, WINDOW_SIZE);
    // Insert the substring starting at position 0.
    hash_table[hash_key].insert(0);
}

void HashTable::add_string(std::size_t character_index_in_sliding_window, Program &program) {
    auto *buffers = program.buffers;
    std::size_t hash_key = compute_hash(character_index_in_sliding_window, buffers->window, WINDOW_SIZE);
    hash_table[hash_key].insert(character_index_in_sliding_window);
}

void HashTable::remove_string(std::size_t character_index_in_sliding_window, Program &program) {
    auto *buffers = program.buffers;
    std::size_t hash_key = compute_hash(character_index_in_sliding_window, buffers->window, WINDOW_SIZE);
    auto it = hash_table.find(hash_key);
    if (it != hash_table.end()) {
        it->second.erase(character_index_in_sliding_window);
        if (it->second.empty()) {
            hash_table.erase(it);
        }
    }
}

lz_match HashTable::find_match(Program &program) {
    auto *buffers = program.buffers;
    lz_match best{NO, 0, 0};

    // Compute the hash of the lookahead buffer (starting at index 0)
    std::size_t hash_key = compute_hash(0, buffers->lookahead, LOOKAHEAD_SIZE);
    auto mapIter = hash_table.find(hash_key);
    if (mapIter == hash_table.end()) {
        best.length = 1; // per original behavior when no match exists
        return best;
    }

    // Iterate over candidate positions in the sliding window.
    const auto &positions = mapIter->second;
    for (auto pos: positions) {
        std::size_t match_len = 0;
        while (match_len < LOOKAHEAD_SIZE) {
            std::size_t idx = wrap_index(pos + match_len, WINDOW_SIZE);
            if (buffers->window[idx] != buffers->lookahead[match_len]) {
                break;
            }
            ++match_len;
        }
        if (match_len > best.length) {
            best.length = match_len;
            best.offset = pos;
            if (match_len == LOOKAHEAD_SIZE) {
                break; // perfect match found
            }
        }
    }

    return best;
}

void init_lookahead_buffer(Program &program) {
    DEBUG_PRINT_LITE("===Loading lookahead buffer%c", '\n');
    auto *buffers = program.buffers;
    buffers->lookahead_head = 0;
    while ((buffers->lookahead_head + 1 < LOOKAHEAD_SIZE) && !program.files->EOF_reached) {
        program.files->read_char();
        buffers->lookahead[buffers->lookahead_head] = program.files->current_char;
        CYCLIC_INCREMENT(buffers->lookahead_head, LOOKAHEAD_SIZE);
    }
    DEBUG_PRINT_LITE("===Loaded lookahead buffer%c", '\n');
}

char update_buffers(Program &program) {
    auto *buffers = program.buffers;
    auto *files = program.files;

    char output_char = buffers->lookahead[wrap_index(buffers->lookahead_head+1, LOOKAHEAD_SIZE)];

    // Update windows
    buffers->window[buffers->window_head] = output_char;

    if (files->EOF_reached) {
        buffers->lookahead[buffers->lookahead_head] = '\0';
    } else {
        buffers->lookahead[buffers->lookahead_head] = files->current_char;
    }
//            buffers->lookahead_buffer[buffers->lookahead_head] = static_cast<int>(files->current_char) == EOF ? '\0' : files->current_char;

    // Update heads
    CYCLIC_INCREMENT(buffers->window_head, WINDOW_SIZE);
    CYCLIC_INCREMENT(buffers->lookahead_head, LOOKAHEAD_SIZE);

    return output_char;
}

std::string process_compression_result(Program &program, lz_match &match) {
    DEBUG_PRINT_LITE("===Processing compression result%c", '\n');
    auto output = std::string();

    auto *buffers = program.buffers;
    auto *hash_table = program.hash_table;
    auto *files = program.files;

    if (match.is_compressed == YES) {
        assert(match.length >= MIN_MATCH_LENGTH);

        for (std::size_t i = 0; i < match.length && !files->EOF_reached; ++i) { // NOTE: Here can not be EOF_reached condition
            // TODO: Specify format for compressed output
            // TODO: Update heads
            //  TODO:          update_buffers(program);
            // TODO: Recalculate hash table
        }
    } else {
        assert(match.length < MIN_MATCH_LENGTH);
        for (std::size_t i = 0; i < match.length && !files->EOF_reached; ++i) {
            // Read new bytes and shift window and lookahead
            files->read_char();
//            DEBUG_PRINT_LITE("Reading new byte: %c%c", files->current_char, '\n');

            output += update_buffers(program);
            // TODO: Recalculate hash table
        }
    }

    return output;
}

std::string process_end(Program &program) {
    DEBUG_PRINT_LITE("Handle end%c", '\n');
    auto *buffers = program.buffers;
//    lz_match match = {NO, 0, strlen(program.buffers->lookahead_buffer)}; // TODO: remove
    std::string output;
//    auto length = strlen(buffers->lookahead_buffer);
    for (; buffers->lookahead[wrap_index(buffers->lookahead_head+1, LOOKAHEAD_SIZE)] != '\0';) {
        // Read new bytes and shift window and lookahead
        output += buffers->lookahead[wrap_index(buffers->lookahead_head+1, LOOKAHEAD_SIZE)];
        update_buffers(program);
        // TODO: Recalculate hash table
    }
    DEBUG_PRINT_LITE("Output size: %zu%c", output.size(), '\n');
    return output;
}

//------------------------------------------------------------------------------
// LZSS
//------------------------------------------------------------------------------
// TODO:   static void compress_adaptive(const Program &program);
// TODO:   static decompress(const Program &program);
static void compress_static(Program &program) {
    DEBUG_PRINT_LITE("===Start compression%c", '\n');
    HashTable *hash_table = program.hash_table;
    hash_table->init_search_structures(program);
    Buffer *buffers = program.buffers;
    init_lookahead_buffer(program);

    // Initialize the hash table based on the current (filled) sliding window.
    hash_table->init_search_structures(program);

    while (true) {
        // Read the next byte from input
        // Place it in the ring buffer
        lz_match match = {NO, 0, MIN_MATCH_LENGTH - 1}; // TODO: remove
//            lz_match match = hash_table.find_match();

        // Process the compression result
        auto output = process_compression_result(program, match);
        DEBUG_PRINT_LITE("Output size: %zu%c", output.size(), '\n');
        DEBUG_PRINT_LITE("Output: %s%c", output.c_str(), '\n');
        for (auto byte: output) {
            program.files->write_char(byte);
        }

        // If we didn’t manage to read all of ‘match.length’ bytes (EOF), exit the loop
        DEBUG_PRINT_LITE("Current position: %zu%c", program.files->current_pos, '\n');
        DEBUG_PRINT_LITE("Lookahead length: %zu%c", strlen(program.buffers->lookahead), '\n');
        if (program.files->EOF_reached) {
            DEBUG_PRINT_LITE("EOF reached%c", '\n');
            break;
        }
    }

    auto output = process_end(program);
    for (auto byte: output) {
        program.files->write_char(byte);
    }
}

//------------------------------------------------------------------------------
// Function
//------------------------------------------------------------------------------
// Wrap the index so that it “circles around” the buffer.
std::size_t wrap_index(std::size_t i, std::size_t buffer_size) {
    return i % buffer_size;
}

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
    auto *file = new File(input_file, output_file);
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
