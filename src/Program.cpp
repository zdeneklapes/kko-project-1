#include <fstream>

#include "../include/Program.h"
#include "../include/debug.h"

File::File(std::string in_filepath, std::string out_filepath) {
    m_in = std::ifstream(in_filepath, std::ios::binary);
    m_out = std::ofstream(out_filepath, std::ios::binary);
}

File::~File() {
    m_in.close();
    m_out.close();
}

bool File::read_char(std::istream &in, uint8_t &out_byte) {
    int c = in.get();
    if (c == EOF) {
        return false; // no more data
    }
    out_byte = static_cast<uint8_t>(c);
    return true;
}

bool File::write_char(std::ostream &out, uint8_t in_byte) {
    out.put(static_cast<char>(in_byte));
    return true;
}

Program::Program() : m_args(nullptr) {}

Program::~Program() {
    delete m_args;
    delete m_file;
}

void Program::parse_arguments(int argc, char **argv) {
    // Allocate the parser on the heap.
    m_args = new argparse::ArgumentParser("lz_codec");

    // Define mode arguments: compression (-c) and decompression (-d)
    m_args->add_argument("-c").help("activate compression mode").default_value(false).implicit_value(true);
    m_args->add_argument("-d").help("activate decompression mode").default_value(false).implicit_value(true);

    // Define additional flags: model (-m) and adaptive scanning (-a)
    m_args->add_argument("-m")
            .help("activate model for preprocessing input data")
            .default_value(false)
            .implicit_value(true);
    m_args->add_argument("-a").help("activate adaptive scanning mode").default_value(false).implicit_value(true);

    // Define file arguments: input (-i) and output (-o) file names (both
    // required)
    m_args->add_argument("-i").help("input file name").required();
    m_args->add_argument("-o").help("output file name").required();

    // Define the image width argument (-w) for compression mode.
    m_args->add_argument("-w")
            .help("image width (required for compression; must be >= 1)")
            .scan<'i', int>()
            .default_value(-1);

    // Optional verbose flag
    m_args->add_argument("-v", "--verbose").help("increase output verbosity").default_value(false).implicit_value(true);

    // Parse the command-line arguments.
    try {
        m_args->parse_args(argc, argv);
    } catch (const std::runtime_error &err) {
        std::cerr << err.what() << std::endl;
        std::cerr << *m_args;
        exit(1);
    }
}

void Program::print_arguments() {
    std::cout << "Program arguments:" << std::endl;
    DEBUG_PRINT_LITE("-c: %d\n", m_args->get<bool>("-c"));
    DEBUG_PRINT_LITE("-d: %d\n", m_args->get<bool>("-d"));
    DEBUG_PRINT_LITE("-m: %d\n", m_args->get<bool>("-m"));
    DEBUG_PRINT_LITE("-a: %d\n", m_args->get<bool>("-a"));
    DEBUG_PRINT_LITE("-i: %s\n", m_args->get<std::string>("-i").c_str());
    DEBUG_PRINT_LITE("-o: %s\n", m_args->get<std::string>("-o").c_str());
    DEBUG_PRINT_LITE("-w: %d\n", m_args->get<int>("-w"));
    DEBUG_PRINT_LITE("-v: %d\n", m_args->get<bool>("-v"));
}
