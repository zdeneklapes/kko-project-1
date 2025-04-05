//
// Created by Zdeněk Lapeš on 26/03/2025.
//

#include <iostream>
#include <fstream>
#include <vector>
#include "debug.h"
#include "Program.h"
#include "lzss_compressor.h"

int main(int argc, char **argv) {
    // ------------------
    // Initialization
    // ------------------
    auto *program = new Program();
    auto *file = new File(program->m_args->get<std::string>("-i"), program->m_args->get<std::string>("-o"));
    program->m_file = file;

    try {
        program->parse_arguments(argc, argv);
    } catch (const std::exception &err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        return 1;
    }

    if (DEBUG) {
        program->print_arguments();
    }

    // ------------------
    // Run
    // ------------------
    LZSSCompressor lzss_compressor;
    try {
        if (program->m_args->get<bool>("-c") && !program->m_args->get<bool>("-a")) {
            lzss_compressor.compress_static(*program);
//        } else if (program->m_args->get<bool>("-c") && program->m_args->get<bool>("-a")) {
//            lzss_compressor.compress_adaptive(*program);
        } else if (program->m_args->get<bool>("-d")) {
            DEBUG_PRINT_LITE("TODO%c", '\n');
        }
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
