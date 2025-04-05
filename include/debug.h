//
// Created by Zdeněk Lapeš on 04/04/2025.
//

#ifndef DEBUG_H
#define DEBUG_H

#define DEBUG (1)
#define DEBUG_LITE (DEBUG)
#define DEBUG_PRINT_LITE(fmt, ...) \
            do { if (DEBUG_LITE) fprintf(stderr, fmt, __VA_ARGS__); } while (0)
#define DEBUG_PRINT(fmt, ...) \
        do { if (DEBUG) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
                                __LINE__, __func__, __VA_ARGS__); } while (0)

#endif //DEBUG_H
