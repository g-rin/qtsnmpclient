#pragma once

#include <QtGlobal>

namespace qtsnmpclient {

constexpr bool is_powerof2( const quint64 v ) {
    return v && ((v & (v - 1)) == 0);
}

// Qt5 QByteArray internals QArrayData size: ~=16 byte for x32, ~=24 bytes for x64
constexpr static int internalHeaderSizeForArray = 32; // bytes

constexpr static int max_size_powerof2 = 32*1024*1024; // 32 MB
constexpr static int min_size_powerof2 = 4;

static_assert(is_powerof2(max_size_powerof2), "max_size_powerof2 is not power of 2");
static_assert(is_powerof2(min_size_powerof2), "min_size_powerof2 is not power of 2");

constexpr static int max_size_default = max_size_powerof2 - internalHeaderSizeForArray;
constexpr static int min_size_default = min_size_powerof2 - internalHeaderSizeForArray;

inline int defragAllocationSizeForQByteArray( const int data_size ) {
    Q_ASSERT( data_size < max_size_default );
    if ( 0 == data_size ) {
        return 0;
    }

    if ( data_size <= min_size_default ) {
        return min_size_default;
    }

    // find near size
    int malloc_size = min_size_powerof2;
    while ( malloc_size < (data_size + internalHeaderSizeForArray) ) {
        // 7 fragments from 8192 bytes to 32 Mb
        malloc_size *= 4;
        // otherwise malloc_size *= 2  -> 13 fragments from 8192 bytes to 32 Mb -> more fragmentation
    }

    return malloc_size - internalHeaderSizeForArray;
}

} // qtsnmpclient
