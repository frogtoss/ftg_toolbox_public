/* ftg_bitbuffer  - public domain library
   no warranty implied; use at your own risk

   Tightly pack values by bits into a stream of bytes.

   For example, a 1-bit bool and a 32-bit integer are packed into 33
   bits.

   Bitbuffers are intended for small amounts of data, like a few
   hundred network packets where size is important enough to remove
   padding bits, and the cpu overhead of packing/unpacking intermixed
   types is not a huge cost.

   FEATURES

    - Compiles C99 warnings-free on clang and visual c++

    - Pack integers with arbitrary numbers of bits

    - Supports quantized floating point packing

    - Possible to avoid heap allocations and copies on read

   USAGE

   Do this:
   #define FTG_IMPLEMENT_BITBUFFER

   before you include this file in one C or C++ file to create the
   implementation.

   It should look like this:
   #include ...
   #include ...
   #include ...
   #define FTG_IMPLEMENT_BITBUFFER
   #include "ftg_bitbuffer.h"

   OPTIONAL

    - Define BITBUF_ASSERT prior to include to override default assert() handler

   REVISION HISTORY

   1.0  2023-01-17   Initial version
   1.1  2023-01-20   Significant bugfix on read
   1.11 2023-01-24   Fix missing header file error

   USAGE NOTIFICATION REQUEST

   If permitted, emailing the author and notifying him that the
   software was used (and how) helps inform him of where he should
   spend his time.  This step is totally optional, but appreciated!

   AUTHOR

   Michael Labbe    https://www.frogtoss.com/pages/contact.html

   LICENSE

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to
   copy, distribute, and modify this file as you see fit by sole
   copyright holder Frogtoss Games, Inc.

   SPECIAL THANKS

   Nick Waanders - quantization functions
*/

#ifndef BITBUF__INCLUDE_BITBUFFER_H
#define BITBUF__INCLUDE_BITBUFFER_H

//// DOCUMENTATION
////
// Known limitations:
//
//  - This code does not take any action to manage endianness.
//
//  - The buffer size must be known at start; bitbuffers are not stretchy
//
//  - The floating point quantization function is not guaranteed to
//    output out_min == in_min, or out_max == in_max, except for the
//    ranges [0,1] and [-1,1]
//
//// Basic Usage
//
//            write values to the buffer
//  bitbuf_buffer_t buf = bitbuf_alloc_buffer(256);
//  bitbuf_write_bool(&buf, true);
//  bitbuf_write_int32(&buf, -32);
//  bitbuf_write_cstr(&buf, "hello, world");
//  bitbuf_write_float(&buf, -325.32f);
//
//            check for truncation during writes
//  assert(!bitbuf_has_truncated(&buf));
//
//            read values from the buffer
//
//            a bitbuf_cursor_t aligns to the next bit to read.  After
//            writing completes, it is thread-safe to have multiple
//            read cursors for a single bitbuffer
//
// bitbuf_cursor_t read = bitbuf_cursor_init(&buf);
// assert(bitbuf_read_bool(&read) == true);
// assert(bitbuf_read_int32(&read) == -32);
//
//            read a cstring, up until serialized NULL terminator
// char str[256];
// bitbuf_read_cstr(&read, 256, str);
// assert(strcmp(str, "hello, world") == 0);
//
// assert(bitbuf_read_float(&read) == -325.32f);
//
//           check for truncation during reads
//  assert(read.read_past_end == 0);

//            free allocated buffer
// bitbuf_free_buffer(&buf);

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

#if defined(__GNUC__) || defined(__clang__)
#    define BITBUF_EXT_unused __attribute__((unused))
#else
#    define BITBUF_EXT_unused
#endif

#ifdef BITBUF_BITBUFFER_STATIC
#    define BITBUFDEF static BITBUF_EXT_unused
#else
#    define BITBUFDEF extern
#endif

#if defined(BITBUF_MALLOC) && defined(BITBUF_FREE)
// okay
#elif !defined(BITBUF_MALLOC) && !defined(BITBUF_FREE)
// also okay
#else
#    error "Must define both or none of BITBUF_MALLOC and BITBUF_FREE"
#endif

#ifndef BITBUF_MALLOC
#    define BITBUF_MALLOC(size) malloc(size)
#    define BITBUF_FREE(ptr) free(ptr)
#endif

// include ftg_core.h ahead of this header to debug it
#ifdef FTG_ASSERT
#    define BITBUF__ASSERT(exp) FTG_ASSERT(exp)
#    define BITBUF__ASSERT_FAIL(exp) FTG_ASSERT_FAIL(exp)
#else
#    ifdef BITBUF_ASSERT
#        define BITBUF__ASSERT(exp) BITBUF_ASSERT(exp)
#        define BITBUF__ASSERT_FAIL(exp) BITBUF_ASSERT(0 && exp)
#    else
#        define BITBUF__ASSERT(exp) (assert(exp))
#        define BITBUF__ASSERT_FAIL(exp) (assert(exp))
#    endif
#endif

#if defined(__GNUC__) || defined(__clang__)
#    if __STDC_VERSION__ < 199901L
#        define BITBUF_INLINE __inline
#    else
#        define BITBUF_INLINE inline
#    endif
#elif defined(_MSC_VER) && (_MSC_VER >= 1700)
#    define BITBUF_INLINE __inline
#endif

#ifdef __cplusplus
extern "C" {
#endif


// API declaration starts here

typedef struct bitbuf_buffer_s bitbuf_buffer_t;

typedef struct {
    // seg == data when at beginning
    uint64_t* seg;

    // indicates how many bits into seg, <= 63
    int bits_into_seg;

    // the owning bitbuffer, or NULL if it's a writer
    const bitbuf_buffer_t* owner;

    // set to 1 if an attempt to read past the end of the buffer was
    // made
    int read_past_end;
} bitbuf_cursor_t;

struct bitbuf_buffer_s {
    uint64_t* data;
    size_t    capacity_bytes;
    int       truncated;

    bitbuf_cursor_t write;
};



// allocate a new buffer for writing
BITBUFDEF bitbuf_buffer_t bitbuf_alloc_buffer(size_t max_bytes);

// allocate a new buffer, copying *bytes into it
BITBUFDEF bitbuf_buffer_t bitbuf_alloc_buffer_with_bytes(const uint8_t* bytes,
                                                         size_t num_bytes);


// free a buffer returned from bitbuf_alloc_*
BITBUFDEF void bitbuf_free_buffer(bitbuf_buffer_t*);

// return a pointer to memory inside bitbuf_buffer_t
// out_num_bytes is set to the number of bytes in *out_data
//
// the pointer to *out_data is made invalid when bitbuf_free_buffer is called on the buffer
BITBUFDEF const uint8_t* bitbuf_get_bytes_from_buffer(const bitbuf_buffer_t*,
                                                      size_t* out_num_bytes);



// init a cursor, used for reading from a bitbuffer.
// more than one read cursor can be initialized for a bitbuffer.
// there is no need to free the cursor.
//
// it is illegal to write to a bitbuffer after initializing the first
// cursor
bitbuf_cursor_t bitbuf_cursor_init(bitbuf_buffer_t* buffer);


// checks if ANY bitbuf write so far has truncated this bitbuffer.
BITBUFDEF bool bitbuf_has_truncated(const bitbuf_buffer_t*);

// bitbuf write routines
BITBUFDEF void bitbuf_write_int64(bitbuf_buffer_t*, int64_t value);
BITBUFDEF void bitbuf_write_int32(bitbuf_buffer_t*, int32_t value);
BITBUFDEF void bitbuf_write_int16(bitbuf_buffer_t*, int16_t value);
BITBUFDEF void bitbuf_write_int8(bitbuf_buffer_t*, int8_t value);
BITBUFDEF void bitbuf_write_uint64(bitbuf_buffer_t*, uint64_t value);
BITBUFDEF void bitbuf_write_uint32(bitbuf_buffer_t*, uint32_t value);
BITBUFDEF void bitbuf_write_uint16(bitbuf_buffer_t*, uint16_t value);
BITBUFDEF void bitbuf_write_uint8(bitbuf_buffer_t*, uint8_t value);
BITBUFDEF void bitbuf_write_float(bitbuf_buffer_t*, float value);
BITBUFDEF void bitbuf_write_double(bitbuf_buffer_t*, double value);
BITBUFDEF void bitbuf_write_bool(bitbuf_buffer_t*, bool);

// write up to strlen(str) + 1 bytes to the bitbuffer, including the null terminator
BITBUFDEF void bitbuf_write_cstr(bitbuf_buffer_t* buf, const char* str);

// write n bits (up to 64)
BITBUFDEF void bitbuf_write_n_bits(bitbuf_buffer_t* buf, int num_bits, uint64_t value);

// write a quantized float, using num_bits precision that must be
// between max and min (inclusive)
BITBUFDEF void bitbuf_write_quantized_float(
    bitbuf_buffer_t* buf, int num_bits, float min, float max, float value);

// fill 0-7 bits with zeroes to align write cursor to byte/octet
//
// next write after this call is guaranteed to be byte aligned
//
// in the event that the write cursor is already on the beginning
// of a byte, no bits are written.
//
// bitbuf_skip_byte_padding is the reciprocal function
BITBUFDEF void bitbuf_pad_to_byte(bitbuf_buffer_t* buf);

// bitbuf read routines
BITBUFDEF int64_t  bitbuf_read_int64(bitbuf_cursor_t* read);
BITBUFDEF int32_t  bitbuf_read_int32(bitbuf_cursor_t* read);
BITBUFDEF int16_t  bitbuf_read_int16(bitbuf_cursor_t* read);
BITBUFDEF int8_t   bitbuf_read_int8(bitbuf_cursor_t* read);
BITBUFDEF uint64_t bitbuf_read_uint64(bitbuf_cursor_t* read);
BITBUFDEF uint32_t bitbuf_read_uint32(bitbuf_cursor_t* read);
BITBUFDEF uint16_t bitbuf_read_uint16(bitbuf_cursor_t* read);
BITBUFDEF uint8_t  bitbuf_read_uint8(bitbuf_cursor_t* read);
BITBUFDEF float    bitbuf_read_float(bitbuf_cursor_t* read);
BITBUFDEF double   bitbuf_read_double(bitbuf_cursor_t* read);
BITBUFDEF bool     bitbuf_read_bool(bitbuf_cursor_t* read);

// read n bits (up to 64)
// If non-null, *out_mask contains a bitmask for the returned bits
BITBUFDEF uint64_t bitbuf_read_n_bits(bitbuf_cursor_t* read,
                                      int              num_bits,
                                      uint64_t*        out_mask);

// read up to max_bytes from the bitbuffer including null terminator,
// putting the result in out_str.  if max_bytes is reached,
// strlen(out_str) == 0 and the read cursor points at the last
// position read (not reset).
BITBUFDEF void bitbuf_read_cstr(bitbuf_cursor_t* read, size_t max_bytes, char* out_str);


// skip byte padding generated by bitbuf_pad_to_byte
BITBUFDEF void bitbuf_skip_byte_padding(bitbuf_cursor_t* read);

// read a quantized float
BITBUFDEF float
bitbuf_read_quantized_float(bitbuf_cursor_t* read, int num_bits, float min, float max);

// advanced: initialize a buffer with *bytes, avoiding buffer allocation and
// a copy.  num_bytes must be a multiple of 8.
//
// do not call bitbuf_free_buffer() on the returned buffer.
BITBUFDEF bitbuf_buffer_t bitbuf_init_buffer_with_bytes(const uint8_t* bytes,
                                                        size_t num_bytes);

//
// End of header file
//
#endif /* BITBUF__INCLUDE_BITBUFFER_H */

#ifdef __cplusplus
}
#endif


/* implementation */
#if defined(FTG_IMPLEMENT_BITBUFFER)

#include <stdlib.h>
#include <string.h>

#define BITBUF__SEG_BITS 64

#define BITBUF__ALIGN_DOWN(n, a) ((n) & ~((a)-1))
#define BITBUF__ALIGN_UP(n, a) BITBUF__ALIGN_DOWN((n) + (a)-1, (a))
#define BITBUF__ALIGN_UP_DELTA(n, a) (((a) - (n)) & ((a)-1))

#define BITBUF__MAX(a, b) ((a) > (b) ? (a) : (b))
#define BITBUF__MIN(a, b) ((a) < (b) ? (a) : (b))

#define BITBUF__ASSERT_NO_WRITE_AFTER_READS(BUF)                               \
    BITBUF__ASSERT((BUF)->write.owner == NULL)

#define BITBUF__PUN(in_type)                                                   \
    union pun_u {                                                              \
        in_type  value;                                                        \
        uint64_t u64;                                                          \
    } pun;

#define BITBUF__READ_TYPE(in_type, bit_count)                                  \
    BITBUF__PUN(in_type)                                                       \
    pun.u64 = bitbuf__read_bits(read, bit_count);

#define BITBUF__WRITE_TYPE(BUF, in_type)                                       \
    BITBUF__PUN(in_type)                                                       \
    pun.value = value;                                                         \
    bitbuf__write_bits(BUF, pun.u64, sizeof(in_type) * 8);

#define BITBUF__DECL_WRITE(in_type, in_name)                                   \
    BITBUFDEF void bitbuf_write_##in_name(bitbuf_buffer_t* buf, in_type value) \
    {                                                                          \
        BITBUF__WRITE_TYPE(buf, in_type);                                      \
    }

#define BITBUF__DECL_WRITE_T(in_type) BITBUF__DECL_WRITE(in_type##_t, in_type)

#define BITBUF__DECL_READ(in_type, in_name)                                    \
    BITBUFDEF in_type bitbuf_read_##in_name(bitbuf_cursor_t* read)             \
    {                                                                          \
        BITBUF__READ_TYPE(in_type, sizeof(in_type) * 8);                       \
        return pun.value;                                                      \
    }

#define BITBUF__DECL_READ_T(in_type) BITBUF__DECL_READ(in_type##_t, in_type)

/* clang-format off */
static const uint64_t bitbuf__spanmasktable[65] = {
    0,
    (1ull << 1) - 1, (1ull << 2) - 1, (1ull << 3) - 1, (1ull << 4) - 1,
    (1ull << 5) - 1, (1ull << 6) - 1, (1ull << 7) - 1, (1ull << 8) - 1,
    (1ull << 9) - 1, (1ull << 10) - 1, (1ull << 11) - 1, (1ull << 12) - 1,
    (1ull << 13) - 1, (1ull << 14) - 1, (1ull << 15) - 1, (1ull << 16) - 1,
    (1ull << 17) - 1, (1ull << 18) - 1, (1ull << 19) - 1, (1ull << 20) - 1,
    (1ull << 21) - 1, (1ull << 22) - 1, (1ull << 23) - 1, (1ull << 24) - 1,
    (1ull << 25) - 1, (1ull << 26) - 1, (1ull << 27) - 1, (1ull << 28) - 1,
    (1ull << 29) - 1, (1ull << 30) - 1, (1ull << 31) - 1, (1ull << 32) - 1,
    (1ull << 33) - 1, (1ull << 34) - 1, (1ull << 35) - 1, (1ull << 36) - 1,
    (1ull << 37) - 1, (1ull << 38) - 1, (1ull << 39) - 1, (1ull << 40) - 1,
    (1ull << 41) - 1, (1ull << 42) - 1, (1ull << 43) - 1, (1ull << 44) - 1,
    (1ull << 45) - 1, (1ull << 46) - 1, (1ull << 47) - 1, (1ull << 48) - 1,
    (1ull << 49) - 1, (1ull << 50) - 1, (1ull << 51) - 1, (1ull << 52) - 1,
    (1ull << 53) - 1, (1ull << 54) - 1, (1ull << 55) - 1, (1ull << 56) - 1,
    (1ull << 57) - 1, (1ull << 58) - 1, (1ull << 59) - 1, (1ull << 60) - 1,
    (1ull << 61) - 1, (1ull << 62) - 1, 0x7fffffffffffffff, 0xffffffffffffffff,
};
/* clang-format on */

BITBUFDEF bitbuf_buffer_t
bitbuf_alloc_buffer(size_t max_bytes)
{
    bitbuf_buffer_t buffer;

    BITBUF__ASSERT(max_bytes > 0);

    // allocate to next segment
    buffer.capacity_bytes = BITBUF__ALIGN_UP(max_bytes, 8);

    buffer.data = (uint64_t*)BITBUF_MALLOC(buffer.capacity_bytes);
    memset(buffer.data, 0, buffer.capacity_bytes);

    buffer.write.seg = buffer.data;
    buffer.write.bits_into_seg = 0;
    buffer.write.owner = NULL;

    buffer.truncated = 0;

    return buffer;
}

BITBUFDEF bitbuf_buffer_t
bitbuf_alloc_buffer_with_bytes(const uint8_t* bytes, size_t num_bytes)
{
    // perf: zeroes memory, then copies over it
    bitbuf_buffer_t buffer = bitbuf_alloc_buffer(num_bytes);
    memcpy(buffer.data, bytes, sizeof(uint8_t) * num_bytes);

    size_t segment = num_bytes / sizeof(uint64_t);
    int    bits_into_seg = (num_bytes % sizeof(uint64_t)) * 8;

    buffer.write.seg = buffer.data + segment;
    buffer.write.bits_into_seg = bits_into_seg;

    return buffer;
}

BITBUFDEF bitbuf_buffer_t
bitbuf_init_buffer_with_bytes(const uint8_t* bytes, size_t num_bytes)
{
    BITBUF__ASSERT((num_bytes % 8) == 0);

    bitbuf_buffer_t buffer;

    buffer.data = (uint64_t*)bytes;
    buffer.capacity_bytes = num_bytes;

    buffer.write.seg = buffer.data;
    buffer.write.bits_into_seg = 0;
    buffer.write.owner = NULL;

    buffer.truncated = 0;

    return buffer;
}

BITBUFDEF bool
bitbuf_has_truncated(const bitbuf_buffer_t* buffer)
{
    return buffer->truncated == 1;
}

BITBUFDEF void
bitbuf_free_buffer(bitbuf_buffer_t* buffer)
{
// if this is hit, the buffer overflowed at some point, were
// truncated.  Increase bitbuffer storage size.
//
// Set (buffer->truncated = 0) prior to free if this is expected
// behaviour.
#ifndef FTGT_TESTS_ENABLED
    BITBUF__ASSERT(!bitbuf_has_truncated(buffer));
#endif

    BITBUF_FREE(buffer->data);
}

BITBUFDEF const uint8_t*
bitbuf_get_bytes_from_buffer(const bitbuf_buffer_t* buf, size_t* out_num_bytes)
{
    BITBUF__ASSERT(out_num_bytes);

    *out_num_bytes = (buf->write.seg - buf->data) * sizeof(uint64_t);
    *out_num_bytes += BITBUF__ALIGN_UP(buf->write.bits_into_seg, 8) / 8;

    return (const uint8_t*)buf->data;
}

BITBUFDEF bitbuf_cursor_t
bitbuf_cursor_init(bitbuf_buffer_t* buffer)
{
    // set the writer owner to a legal non-NULL value, indicating that
    // writing is complete.
    buffer->write.owner = buffer;

    return (bitbuf_cursor_t){
        .seg = buffer->data,
        .bits_into_seg = 0,
        .owner = buffer,
    };
}

static ptrdiff_t
bitbuf__remaining_capacity_in_bits(const bitbuf_buffer_t* buffer)
{
    BITBUF__ASSERT(buffer->write.bits_into_seg < BITBUF__SEG_BITS);

    size_t completed_seg_bits = (buffer->write.seg - buffer->data) * BITBUF__SEG_BITS;
    size_t remaining_seg_bits = BITBUF__SEG_BITS - buffer->write.bits_into_seg;
    BITBUF__ASSERT(remaining_seg_bits <= BITBUF__SEG_BITS);
    size_t total_bits_used = completed_seg_bits + buffer->write.bits_into_seg;

    size_t capacity_bits = buffer->capacity_bytes * 8;
    BITBUF__ASSERT(capacity_bits >= total_bits_used);

    return (ptrdiff_t)capacity_bits - total_bits_used;
}

static ptrdiff_t
bitbuf__bits_remaining_for_cursor(const bitbuf_buffer_t* buffer,
                                  const bitbuf_cursor_t* cursor)
{
    ptrdiff_t remaining_segs = (buffer->capacity_bytes / sizeof(uint64_t)) -
                               (cursor->seg - buffer->data);
    BITBUF__ASSERT(remaining_segs >= 0);

    int remaining_bits = BITBUF__SEG_BITS - cursor->bits_into_seg;

    return (remaining_segs * BITBUF__SEG_BITS) + remaining_bits;
}

static bool
bitbuf__is_valid_read_cursor(const bitbuf_cursor_t* cursor)
{
    return (cursor && cursor->owner && cursor->seg >= cursor->owner->data &&
            bitbuf__bits_remaining_for_cursor(cursor->owner, cursor) >= 0);
}

static BITBUF_INLINE void
bitbuf__advance_cursor(bitbuf_cursor_t* cursor)
{
    cursor->bits_into_seg = 0;
    cursor->seg++;
}

BITBUFDEF void
bitbuf__write_bits(bitbuf_buffer_t* buffer, uint64_t datum, int num_bits)
{
    BITBUF__ASSERT(num_bits <= 64);
    if (num_bits > 64)
        return;

    // if this is hit, a call to bitbuf_init_cursor() (to begin reading) has
    // occurred and a subsequent write was attempted.
    BITBUF__ASSERT_NO_WRITE_AFTER_READS(buffer);

    if (bitbuf__remaining_capacity_in_bits(buffer) < num_bits) {
        BITBUF__ASSERT_FAIL("out of space writing bits");
        buffer->truncated |= 1;
        return;
    }


    int bits_remaining_in_seg = BITBUF__SEG_BITS - buffer->write.bits_into_seg;

    // do the bits fit in the current seg?
    if (num_bits <= bits_remaining_in_seg) {
        const uint64_t SRC_MASK = bitbuf__spanmasktable[num_bits];
        *buffer->write.seg |= (datum & SRC_MASK) << buffer->write.bits_into_seg;

        buffer->write.bits_into_seg += num_bits;

        if (buffer->write.bits_into_seg == BITBUF__SEG_BITS) {
            bitbuf__advance_cursor(&buffer->write);

            BITBUF__ASSERT(bitbuf__remaining_capacity_in_bits(buffer) >= 0);
        }
    } else {
        // no - write the bits for the current segment and call recursively
        // to do the remainder
        const uint64_t SRC_MASK = bitbuf__spanmasktable[bits_remaining_in_seg];
        *buffer->write.seg |= (datum & SRC_MASK)
                              << (BITBUF__SEG_BITS - bits_remaining_in_seg);

        bitbuf__advance_cursor(&buffer->write);

        int num_bits_remaining_for_next_write = num_bits - bits_remaining_in_seg;
        BITBUF__ASSERT(num_bits_remaining_for_next_write < BITBUF__SEG_BITS);
        const uint64_t OVER_MASK =
            bitbuf__spanmasktable[num_bits_remaining_for_next_write]
            << (bits_remaining_in_seg);
        bitbuf__write_bits(buffer,
                           (datum & OVER_MASK) >> bits_remaining_in_seg,
                           num_bits_remaining_for_next_write);
    }
}

// read up to 64 bits into *out_bits
BITBUFDEF uint64_t
bitbuf__read_bits(bitbuf_cursor_t* read, int num_bits)
{
    const bitbuf_buffer_t* buffer = read->owner;

    BITBUF__ASSERT(bitbuf__is_valid_read_cursor(read));
    BITBUF__ASSERT(read->seg);

    BITBUF__ASSERT(num_bits <= 64);
    if (num_bits > 64) {
        return 0;
    }

    if (bitbuf__bits_remaining_for_cursor(buffer, read) < num_bits) {
        BITBUF__ASSERT_FAIL("read past end of buffer");
        read->read_past_end |= 1;
        return 0;
    }

    int bits_remaining_in_seg = BITBUF__SEG_BITS - read->bits_into_seg;

    // are there enough bits in the current seg?
    if (num_bits <= bits_remaining_in_seg) {
        const uint64_t DST_MASK = bitbuf__spanmasktable[num_bits] << read->bits_into_seg;

        uint64_t val = (*read->seg & DST_MASK) >> read->bits_into_seg;

        read->bits_into_seg += num_bits;

        if (read->bits_into_seg == BITBUF__SEG_BITS) {
            bitbuf__advance_cursor(read);
        }

        return val;
    } else {
        // no - read the bits for the current segment and then
        // subsequently read the rest
        const uint64_t DST_MASK = bitbuf__spanmasktable[bits_remaining_in_seg]
                                  << (BITBUF__SEG_BITS - bits_remaining_in_seg);

        uint64_t val =
            (*read->seg & DST_MASK) >> (BITBUF__SEG_BITS - bits_remaining_in_seg);

        bitbuf__advance_cursor(read);
        int next_read_num_bits = num_bits - bits_remaining_in_seg;
        BITBUF__ASSERT(next_read_num_bits < BITBUF__SEG_BITS);
        BITBUF__ASSERT(bitbuf__bits_remaining_for_cursor(buffer, read) >= num_bits);

        uint64_t OVER_MASK = bitbuf__spanmasktable[next_read_num_bits];

        val |= (*read->seg & OVER_MASK) << bits_remaining_in_seg;
        read->bits_into_seg += next_read_num_bits;

        return val;
    }
}

BITBUF__DECL_WRITE_T(int64);
BITBUF__DECL_WRITE_T(int32);
BITBUF__DECL_WRITE_T(int16);
BITBUF__DECL_WRITE_T(int8);
BITBUF__DECL_WRITE_T(uint64);
BITBUF__DECL_WRITE_T(uint32);
BITBUF__DECL_WRITE_T(uint16);
BITBUF__DECL_WRITE_T(uint8);
BITBUF__DECL_WRITE(float, float);
BITBUF__DECL_WRITE(double, double);

BITBUFDEF void
bitbuf_write_bool(bitbuf_buffer_t* buf, bool value)
{
    BITBUF__PUN(bool);
    pun.value = value;
    bitbuf__write_bits(buf, pun.u64, 1);
}

BITBUFDEF void
bitbuf_write_cstr(bitbuf_buffer_t* buf, const char* str)
{
    const char* p = str;

    while (*p) {
        // perf: write n bits to align, then power through 64-bits at a time
        // until the last 64 bits
        bitbuf__write_bits(buf, *p, 8);
        p++;
    }

    // write null terminator
    bitbuf__write_bits(buf, 0, 8);
}

BITBUFDEF void
bitbuf_write_n_bits(bitbuf_buffer_t* buf, int num_bits, uint64_t value)
{
    BITBUF__ASSERT(num_bits <= 64);
    if (num_bits > 64) {
        return;
    }

    // if this is hit, value has set bits that are being chopped off
    BITBUF__ASSERT((value & (~bitbuf__spanmasktable[num_bits])) == 0);

    bitbuf__write_bits(buf, value, num_bits);
}



BITBUFDEF void
bitbuf_write_quantized_float(bitbuf_buffer_t* buf, int num_bits, float min, float max, float value)
{
    BITBUF__ASSERT((size_t)num_bits <= (sizeof(float) * 8) - 1);
    BITBUF__ASSERT(min < max);
    BITBUF__ASSERT(value >= min && value <= max);

    const uint32_t bit_max = (uint32_t)bitbuf__spanmasktable[num_bits];

    float qf =
        BITBUF__MIN(BITBUF__MAX(((value - min) * bit_max) / (max - min), 0), bit_max);
    uint64_t qi = (uint64_t)qf;

    // expr '(value - min) * mult' performed as floating point may result in one additional
    // bit, causing qi to exceed num_bits and failing to represent saturation.
    qi = qi && (qi & bit_max) == 0 ? bit_max : qi;

    bitbuf__write_bits(buf, qi, num_bits);
}


BITBUFDEF void
bitbuf_pad_to_byte(bitbuf_buffer_t* buf)
{
    int align_bits = BITBUF__ALIGN_UP_DELTA(buf->write.bits_into_seg, 8);
    if (align_bits != 0) {
        uint64_t zero = 0;

        bitbuf__write_bits(buf, zero, align_bits);
    }
}

BITBUF__DECL_READ_T(int64);
BITBUF__DECL_READ_T(int32);
BITBUF__DECL_READ_T(int16);
BITBUF__DECL_READ_T(int8);
BITBUF__DECL_READ_T(uint64);
BITBUF__DECL_READ_T(uint32);
BITBUF__DECL_READ_T(uint16);
BITBUF__DECL_READ_T(uint8);
BITBUF__DECL_READ(float, float);
BITBUF__DECL_READ(double, double);

BITBUFDEF bool
bitbuf_read_bool(bitbuf_cursor_t* read)
{
    BITBUF__READ_TYPE(bool, 1);
    return pun.value;
}

BITBUFDEF void
bitbuf_read_cstr(bitbuf_cursor_t* read, size_t max_bytes, char* out_str)
{
    size_t i;
    for (i = 0; i < max_bytes; i++) {
        out_str[i] = (char)bitbuf__read_bits(read, 8);

        if (out_str[i] == '\0')
            return;
    }

    // null terminator not found -- terminate string
    out_str[0] = '\0';
}

BITBUFDEF uint64_t
bitbuf_read_n_bits(bitbuf_cursor_t* read, int num_bits, uint64_t* out_mask)
{
    BITBUF__ASSERT(num_bits <= 64);
    if (num_bits > 64) {
        return 0;
    }

    uint64_t datum = bitbuf__read_bits(read, num_bits);

    if (out_mask) {
        *out_mask = bitbuf__spanmasktable[num_bits];
    }

    return datum;
}

BITBUFDEF void
bitbuf_skip_byte_padding(bitbuf_cursor_t* read)
{
    BITBUF__ASSERT(bitbuf__is_valid_read_cursor(read));

    read->bits_into_seg = BITBUF__ALIGN_UP(read->bits_into_seg, 8);

    BITBUF__ASSERT(read->bits_into_seg <= BITBUF__SEG_BITS);

    if (read->bits_into_seg == BITBUF__SEG_BITS) {
        bitbuf__advance_cursor(read);
    }

    BITBUF__ASSERT(bitbuf__bits_remaining_for_cursor(read->owner, read) >= 0);
}


BITBUFDEF float
bitbuf_read_quantized_float(bitbuf_cursor_t* read, int num_bits, float min, float max)
{
    BITBUF__ASSERT((size_t)num_bits <= (sizeof(float) * 8) - 1);
    BITBUF__ASSERT(min < max);
    BITBUF__ASSERT(num_bits <= 31);

    uint64_t       value = bitbuf_read_n_bits(read, num_bits, NULL);
    const uint32_t bit_max = (uint32_t)bitbuf__spanmasktable[num_bits];

    float q = min + (((float)value / bit_max) * (max - min));
    BITBUF__ASSERT(q >= min && q <= max);

    return q;
}

// tests follow -- this is intended to be ran by a core developer
// who includes ftg_test.h, a test harness.
#ifdef FTGT_TESTS_ENABLED

struct bitbuf_testvars_s {
    bitbuf_buffer_t buf;
};

static struct bitbuf_testvars_s bitbuf__tv;

static int
bitbuf__test_setup(void)
{
    bitbuf__tv.buf = bitbuf_alloc_buffer(256);
    return 0; /* setup success */
}

static int
bitbuf__test_teardown(void)
{
    bitbuf_free_buffer(&bitbuf__tv.buf);
    return 0;
}

static int
bitbuf__test_basic(void)
{
    bitbuf_buffer_t* buf = &bitbuf__tv.buf;

    // writes
    bitbuf_write_bool(buf, true);
    bitbuf_pad_to_byte(buf);
    bitbuf_write_int64(buf, -32);
    bitbuf_write_cstr(buf, "hello, world");
    bitbuf_write_float(buf, -325.32f);
    bitbuf_write_n_bits(buf, 4, 13);
    bitbuf_pad_to_byte(buf);
    bitbuf_write_n_bits(buf, 7, 121);

    // reads
    bitbuf_cursor_t read = bitbuf_cursor_init(buf);

    TEST(bitbuf_read_bool(&read) == true);
    bitbuf_skip_byte_padding(&read);
    TEST(bitbuf_read_int64(&read) == -32);

    char            str[256];
    bitbuf_cursor_t read2 = read;
    bitbuf_read_cstr(&read, 256, str);
    TEST(strcmp(str, "hello, world") == 0);

    // no room for null terminator
    bitbuf_read_cstr(&read2, strlen("hello, world"), str);
    TEST(str[0] == '\0');

    TEST(bitbuf_read_float(&read) == -325.32f);

    uint64_t mask;
    TEST(bitbuf_read_n_bits(&read, 4, &mask) == 13);
    TEST(mask == 15);

    bitbuf_skip_byte_padding(&read);

    TEST(bitbuf_read_n_bits(&read, 7, NULL) == 121);

    return ftgt_test_errorlevel();
}

static int
bitbuf__test_buffer_overflow(void)
{
    // write non-overflow -- cursor aligns to end of only segment, but
    // does not overflow
    {
        bitbuf_buffer_t buf = bitbuf_alloc_buffer(1);
        bitbuf_write_uint64(&buf, 0xFF);
        TEST(!bitbuf_has_truncated(&buf));
        bitbuf_free_buffer(&buf);
    }

    // write overflow on non-aligned write
    {
        // this allocates 64 bits because bitbuffer rounds up to segment width.
        // we then write 65 bits, and expect a truncation.
        bitbuf_buffer_t buf = bitbuf_alloc_buffer(1);
        bitbuf_write_bool(&buf, true);
        bitbuf_write_uint64(&buf, 0xFF);

        // expect an assert to be triggered in previous bitbuf call
        TEST(ftgt_test_errorlevel());

        TEST(bitbuf_has_truncated(&buf));
        buf.truncated = 0;
        bitbuf_free_buffer(&buf);
    }

    // read non-overflow -- cursor aligns to end of single byte
    {
        bitbuf_buffer_t buf = bitbuf_alloc_buffer(1);
        bitbuf_write_uint64(&buf, 0xFF);


        bitbuf_cursor_t read = bitbuf_cursor_init(&buf);
        uint64_t        value = bitbuf_read_uint64(&read);
        TEST(value == 0xFF);

        bitbuf_free_buffer(&buf);
    }

    // read overflow
    {
        bitbuf_buffer_t buf = bitbuf_alloc_buffer(1);
        bitbuf_write_uint64(&buf, 0xFF);

        bitbuf_cursor_t read = bitbuf_cursor_init(&buf);
        uint64_t        value;
        value = bitbuf_read_uint64(&read);
        TEST(value == 0xFF);

        // read past end
        bitbuf_read_uint64(&read);
        value = bitbuf_read_uint64(&read);
        TEST(read.read_past_end == 1);
        TEST(ftgt_test_errorlevel());
        TEST(value == 0);

        bitbuf_free_buffer(&buf);
    }

    return ftgt_test_errorlevel();
}

static int
bitbuf__test_align_to_end_of_segment(void)
{
    bitbuf_buffer_t buf = bitbuf_alloc_buffer(16);

    uint64_t BIG = 0x7FFFFFFFFFFFFFFFull;

    bitbuf_write_n_bits(&buf, 63, BIG);
    bitbuf_pad_to_byte(&buf);
    bitbuf_write_int32(&buf, -500000);

    bitbuf_cursor_t read = bitbuf_cursor_init(&buf);

    uint64_t value = bitbuf_read_n_bits(&read, 63, NULL);
    TEST(value == BIG);

    bitbuf_skip_byte_padding(&read);
    TEST(bitbuf_read_int32(&read) == -500000);

    bitbuf_free_buffer(&buf);

    return ftgt_test_errorlevel();
}

static int
bitbuf__test_write_after_read(void)
{
    bitbuf_buffer_t* buf = &bitbuf__tv.buf;

    bitbuf_cursor_init(buf);
    bitbuf_write_bool(buf, false);

    TEST(ftgt_test_errorlevel());

    return ftgt_test_errorlevel();
}

static int
bitbuf__test_crop_set_bit_on_n_write(void)
{
    bitbuf_write_n_bits(&bitbuf__tv.buf, 1, 3);
    TEST(ftgt_test_errorlevel());

    return ftgt_test_errorlevel();
}

static int
bitbuf__test_cstr_overflow(void)
{
    bitbuf_buffer_t buf = bitbuf_alloc_buffer(5);

    // implementation detail dependency
    TEST(buf.capacity_bytes == 8);

    // catch assert for overflow on null terminator
    bitbuf_write_cstr(&buf, "abcdefgh");

    TEST(ftgt_test_errorlevel());
    TEST(bitbuf_has_truncated(&buf));
    bitbuf_free_buffer(&buf);

    return ftgt_test_errorlevel();
}

static int
bitbuf__test_read_buffers(void)
{
    const char STR[] = "abcdefgh";

    {
        size_t i;
        bitbuf_buffer_t buf =
            bitbuf_alloc_buffer_with_bytes((uint8_t*)STR, strlen(STR));
        bitbuf_cursor_t read = bitbuf_cursor_init(&buf);

        for (i = 0; i < strlen(STR); i++) {
            TEST(bitbuf_read_uint8(&read) == (uint8_t)STR[i]);
        }

        bitbuf_free_buffer(&buf);

        return ftgt_test_errorlevel();
    }

    {
        size_t i;
        bitbuf_buffer_t buf =
            bitbuf_init_buffer_with_bytes((uint8_t*)STR, strlen(STR));
        bitbuf_cursor_t read = bitbuf_cursor_init(&buf);

        for (i = 0; i < strlen(STR); i++) {
            TEST(bitbuf_read_uint8(&read) == (uint8_t)STR[i]);
        }

        return ftgt_test_errorlevel();
    }
}


static int
bitbuf__test_qfloat(void)
{
    const int    STORE_BITS[] = {4, 8, 16, 24, 31};
    const size_t STORE_BITS_LEN = sizeof(STORE_BITS) / sizeof(STORE_BITS[0]);
    size_t i;


    const float TEST_RANGES[][3] = {
        /* min, max */
        {+0.0f, +1.0f},
        {-1.0, 0.0f},
        {-1.0f, 1.0f},
        {-32000.f, 32000.f},
    };
    const size_t TEST_RANGES_LEN = sizeof(TEST_RANGES) / sizeof(TEST_RANGES[0]);


    for (i = 0; i < TEST_RANGES_LEN; i++) {
        size_t n;

        // todo: get this working with sizeof(float) * 3
        bitbuf_buffer_t buf = bitbuf_alloc_buffer(256);

        float in_min = TEST_RANGES[i][0];
        float in_max = TEST_RANGES[i][1];

        for (n = 0; n < STORE_BITS_LEN; n++) {
            int num_bits = STORE_BITS[n];

            bitbuf_write_quantized_float(&buf, num_bits, in_min, in_max, in_min);
            bitbuf_write_quantized_float(&buf, num_bits, in_min, in_max, in_max);
        }
        TEST(!bitbuf_has_truncated(&buf));

        bitbuf_cursor_t read = bitbuf_cursor_init(&buf);

        for (n = 0; n < STORE_BITS_LEN; n++) {
            int num_bits = STORE_BITS[n];

            float min = bitbuf_read_quantized_float(&read, num_bits, in_min, in_max);
            float max = bitbuf_read_quantized_float(&read, num_bits, in_min, in_max);

            TEST(min == in_min);
            TEST(max == in_max);
        }

        bitbuf_free_buffer(&buf);
    }

    return ftgt_test_errorlevel();
}

static int
bitbuf__test_get_bytes_from_buffer(void)
{
    bitbuf_buffer_t* buf = &bitbuf__tv.buf;

    size_t num_bytes;
    TEST(bitbuf_get_bytes_from_buffer(buf, &num_bytes) == (uint8_t*)buf->data);
    TEST(num_bytes == 0);

    bitbuf_write_bool(buf, true);
    bitbuf_get_bytes_from_buffer(buf, &num_bytes);
    TEST(num_bytes == 1);

    bitbuf_write_uint64(buf, 0xFF00FF00FF00FF00);
    bitbuf_get_bytes_from_buffer(buf, &num_bytes);
    TEST(num_bytes == 9);


    return ftgt_test_errorlevel();
}

BITBUFDEF
void
bitbuf_decl_suite(void)
{
    ftgt_suite_s* suite = ftgt_create_suite(
        NULL, "bitbuf_core", bitbuf__test_setup, bitbuf__test_teardown);
    FTGT_ADD_TEST(suite, bitbuf__test_basic);
    FTGT_ADD_TEST(suite, bitbuf__test_buffer_overflow);
    FTGT_ADD_TEST(suite, bitbuf__test_align_to_end_of_segment);
    FTGT_ADD_TEST(suite, bitbuf__test_write_after_read);
    FTGT_ADD_TEST(suite, bitbuf__test_crop_set_bit_on_n_write);
    FTGT_ADD_TEST(suite, bitbuf__test_cstr_overflow);
    FTGT_ADD_TEST(suite, bitbuf__test_read_buffers);
    FTGT_ADD_TEST(suite, bitbuf__test_qfloat);
    FTGT_ADD_TEST(suite, bitbuf__test_get_bytes_from_buffer);
}

#endif /* FTGT_TESTS_ENABLED */

#endif /* defined(BITBUF_IMPLEMENT_BITBUFFER) */

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2019 Sean Barrett
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
