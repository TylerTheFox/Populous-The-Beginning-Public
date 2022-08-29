#include "Pop3Compress.h"
#include "../../../snappy/include/snappy-c.h"

size_t Pop3Compress::compress(const char* input, size_t input_length, char*& compressed, int extendBytes)
{
    size_t length = snappy_max_compressed_length(input_length - extendBytes);

    compressed = new char[length + extendBytes];
    snappy_status status = snappy_compress(input + extendBytes, input_length - extendBytes, compressed + extendBytes, &length);

    if (status != SNAPPY_OK)
    {
        delete[] compressed;
        compressed = nullptr;
        return 0;
    }

    return length + extendBytes;
}

size_t Pop3Compress::decompress(const char* input, size_t input_length, char*& decompressed, int extendBytes)
{
    size_t decompressedSize = 0;

    snappy_uncompressed_length(input + extendBytes, input_length - extendBytes, &decompressedSize);

    decompressed = new char[decompressedSize + extendBytes];
    snappy_status status = snappy_uncompress(input + extendBytes, input_length - extendBytes, decompressed + extendBytes, &decompressedSize);

    if (status != SNAPPY_OK)
    {
        delete[] decompressed;
        decompressed = nullptr;
        return 0;
    }

    return decompressedSize + extendBytes;
}
