#ifndef SHARED_H
#define SHARED_H

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>  // for std::min


static const size_t LITERAL = 0;
static const size_t MATCH = 1;
static const size_t SEARCH_SIZE = 256;
static const size_t PREDICTION_BUFFER = 15;
static const size_t THRESHOLD = 2;   // Matches of length (>= 3) we treat as a match

#endif // SHARED_H
