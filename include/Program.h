#ifndef PROGRAM_H
#define PROGRAM_H

#include "../third_party/argparse/include/argparse/argparse.hpp"
#include <string>
#include <iostream>
#include <fstream>
#include <cstdlib> // for exit()

class File {
public:
    // Retrieve input/output filenames from Program
    std::ifstream m_in;
    std::ofstream m_out;

    std::ifstream *m_search_buffer_start_ptr;
    std::ofstream *m_search_buffer_end_ptr;

    std::ofstream *m_prediction_buffer_start_ptr;
    std::ofstream *m_prediction_buffer_end_ptr;

    File(std::string in_filepath, std::string out_filepath);

    ~File();

    bool read_char(std::istream &in, uint8_t &out_byte);

    bool write_char(std::ostream &out, uint8_t in_byte);
};

// Program class for processing command-line arguments.
class Program {
public:
    // Heap-allocated pointer to the ArgumentParser instance.
    argparse::ArgumentParser *m_args;
    File *m_file;

    // Constructor and Destructor.
    Program();

    ~Program();

    // Parse command-line arguments.
    void parse_arguments(int argc, char **argv);

    void print_arguments();
};

#endif // PROGRAM_H
