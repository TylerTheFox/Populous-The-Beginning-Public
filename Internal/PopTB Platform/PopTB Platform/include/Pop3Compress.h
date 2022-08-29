#pragma once
#include <cstddef>

class Pop3Compress
{
public:
    Pop3Compress() = delete;

    // Memory Compress
    static size_t compress(const char* input, size_t input_length, char*& compressed, int extendBytes);

    // Memory Decompress
    static size_t decompress(const char* input, size_t input_length, char*& decompressed, int extendBytes);
};
