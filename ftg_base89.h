/* ftg_base89  - public domain library
   no warranty implied; use at your own risk

   Implementation of functions that process Printable Base89 Lookup
   Table strings

   Description of Printable Base89 LUT Encoding:
   https://www.frogtoss.com/labs/printable-base89-lut-encoding.html

   This is tested with Visual Studio, Clang and GCC.  Additionally, it
   works for compiling wasm32.  It assumes little endian, and will
   have issues on big endian architectures.

   USAGE

   Do this:
   #define FTG_IMPLEMENT_BASE89

   before you include this file in one C or C++ file to create the
   implementation.

   It should look like this:
   #include ...
   #include ...
   #include ...
   #define FTG_IMPLEMENT_BASE89

   // optional
   #define B89_CHAR unsigned char

   #include "ftg_base89.h"


   SIMPLE API USAGE

   char buf[4];
   b89_pack(192, buf);             // index 192 now stored in base89, using 4 bytes

   unsigned int index;
   b89_unpack(buf, 1000, &index);  // 'index' now set to 192


   ITERATOR USAGE - FAST STRING PROCESSING

   #define CODE_RESET  "\x03\x27\x26\x26"
   #define CODE_NAME   "\x03\x28\x26\x26"
   #define NAME(x) CODE_NAME x CODE_RESET

   const char str[] = "Hello, " NAME("guy");

   my_print_str()
   {
       b89_iter_t it;
       b89_iter_init(&it, str, strlen(str), 3);

       // loop until end of string
       while (b89_iter_next(&it)) {
           switch (it.event.type) {
           case B89_EVENT_TEXT:
               // emit span of normal text
               fwrite(it.event.text.ptr, 1, it.event.text.len, stdout);
               break;

           case B89_EVENT_CODE:
               // it's a base89 code

               // index 0 is always a malformed or out of range code
               assert(it.event.code.index != 0);

               // do something with the code, which is guaranteed
               // to be in range
               some_look_up(it.event.code.index);

               break;
           }
       }
   }

   REVISION HISTORY

      0.1  Oct 21, 2025   Initial version

   LICENSE

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifndef B89__INCLUDE_BASE89_H
#    define B89__INCLUDE_BASE89_H

#    ifdef B89_BASE89_STATIC
#        define B89DEF static
#    else
#        define B89DEF extern
#    endif

#    ifdef B89_BASE89_STATIC
#        define B89DEFDATA static
#    else
#        define B89DEFDATA
#    endif


// include ftg_core.h ahead of this header to debug it
#    ifdef FTG_ASSERT
#        define B89__ASSERT(exp) FTG_ASSERT(exp)
#        define B89__ASSERT_FAIL(exp) FTG_ASSERT_FAIL(exp)
#    else
#        define B89__ASSERT(exp) ((void)0)
#        define B89__ASSERT_FAIL(exp) ((void)0)
#    endif

#    include <stddef.h>
#    include <string.h>  // for memcpy
#    include <limits.h>

#    define B89_ST 0x03

#    ifndef B89_CHAR
#        define B89_CHAR char
#    endif


typedef enum {
    B89_EVENT_TEXT,  // plain text
    B89_EVENT_CODE,  // code
} b89_event_type_t;

// always return index 0 on error
#    define B89_ERROR_INDEX 0
#    define B89_CODE_MAX 704968

typedef union {
    b89_event_type_t type;

    // event type is a span of text ptr, 'len' long
    struct {
        b89_event_type_t type;
        const B89_CHAR*  ptr;
        size_t           len;
    } text;

    // event type is a Base89 LUT Index
    struct {
        b89_event_type_t type;
        unsigned int     index;
    } code;
} b89_event_t;


typedef struct {
    B89_CHAR*    pos;
    B89_CHAR*    end;
    b89_event_t  event;
    unsigned int max_index;
} b89_iter_t;


// begin iterating over *str.  str_bytes can include null terminator, but it does not have to
B89DEF void b89_iter_init(b89_iter_t*     it,
                          const B89_CHAR* str,
                          size_t          str_bytes,
                          unsigned int    max_index);

// next iteration.  it->event becomes the next event.  returns 0 if there are no more events
B89DEF int b89_iter_next(b89_iter_t* it);

// pack index into four bytes, stored at *out_bytes
B89DEF void b89_pack(unsigned int index, B89_CHAR* out_bytes);

// unpack four bytes into an unsigned int index.  Index 0 is returned
// if there was an unpacking error.
B89DEF void b89_unpack(const B89_CHAR* bytes, unsigned int max_index, unsigned int* out_index);


// API declaration starts here

//
// End of header file
//
#endif /* B89__INCLUDE_BASE89_H */

/* implementation */
#if defined(FTG_IMPLEMENT_BASE89)

B89DEF void
b89_iter_init(b89_iter_t* it, const B89_CHAR* str, size_t str_bytes, unsigned int max_index)
{
    B89__ASSERT(it);
    B89__ASSERT(str);

    it->pos = (B89_CHAR*)str;
    it->end = it->pos + str_bytes;
    it->max_index = max_index;
}

#    if defined(_MSC_VER)
#        include <stdint.h>
typedef uint32_t B89__u32;
typedef uint64_t B89__u64;

#    elif defined(__clang__) || defined(__GNUC__)
#        if __SIZEOF_INT__ == 4
typedef unsigned int B89__u32;
#        elif __SIZEOF_LONG__ == 4
typedef unsigned long B89__u32;
#        else
#            error "No 32-bit type available"
#        endif

#        if __SIZEOF_LONG_LONG__ == 8
typedef unsigned long long B89__u64;
#        elif __SIZEOF_LONG__ == 8
typedef unsigned long B89__u64;
#        else
#            error "No 64-bit type available"
#        endif

#    else
#        error "Unsupported compiler: cannot determine 32-bit/64-bit types."
#    endif

#    if defined(_MSC_VER)
#        define B89__UNREACHABLE __assume(0)
#    elif defined(__GNUC__) || defined(__clang__)
#        define B89__UNREACHABLE __builtin_unreachable()
#    else
#        define FTG__UNREACHABLE
#    endif

#    if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#        define B89__INLINE inline
#    elif defined(_MSC_VER)
#        define B89__INLINE __inline
#    elif defined(__GNUC__) || defined(__clang__)
#        define B89__INLINE __inline__
#    else
#        define B89__INLINE
#    endif

#    if defined(__GNUC__) || defined(__clang__) || defined(__wasm)
#        define B89__HAS_CTZ 1
#    elif defined(_MSC_VER)
#        define B89__HAS_BITSCANFORWARD 1
#    else
#        define B89__SLOW_MATCH_INDEX 1
#    endif


#    if B89__HAS_CTZ
static B89__INLINE size_t
get_first_match_index(B89__u64 has_match)
{
    return (size_t)__builtin_ctzll(has_match) / 8;
}
#    elif B89__HAS_BITSCANFORWARD

#        include <intrin.h>
static B89__INLINE size_t
get_first_match_index(B89__u64 has_match)
{
    unsigned long index;
    _BitScanForward64(&index, has_match);
    return (size_t)index / 8;
}
#    else

// byte crawl fallback -- pre-condition is that *in contains 0x03 within 8 bytes
static B89__INLINE size_t
get_first_match_index(const B89_CHAR* in)
{
    for (size_t i = 0; i < 8; i++) {
        if (in[i] == B89_ST) {
            return i;
        }
    }

    B89__UNREACHABLE;
}
#    endif



static void
scan_string_seg(B89_CHAR** p_in, const B89_CHAR* end)
{
    const B89__u64 ST_64BIT = 0x0303030303030303ULL;
    const B89__u64 HAS_ZERO_SUB = 0x0101010101010101ULL;
    const B89__u64 HAS_ZERO_HIGH = 0x8080808080808080ULL;

    B89_CHAR* in = *p_in;

    while (in + 8 <= end) {
        B89__u64 qword;
        memcpy(&qword, in, 8);

        B89__u64 diff = qword ^ ST_64BIT;
        B89__u64 has_match = (diff - HAS_ZERO_SUB) & ~diff & HAS_ZERO_HIGH;

        if (has_match) {
            // locate ST byte in qword, but where?

#    if B89__SLOW_MATCH_INDEX
            size_t index = get_first_match_index(in);
#    else
            size_t index = get_first_match_index(has_match);
#    endif
            *p_in = in + index;
            return;
#
        }

        in += 8;
    }

    // tail loop
    while (in < end) {
        if (*in == B89_ST) {
            *p_in = in;
            return;
        }
        in++;
    }

    *p_in = (B89_CHAR*)end;
}

#    define B89__ORD_MIN 38
#    define B89__ORD_MAX 126
#    define B89__ORD_BASE ((B89__ORD_MAX - B89__ORD_MIN) + 1) /* 89 */


static B89__INLINE B89__u32
decode_base89(B89__u32 packed_code, B89__u32 max_index)
{
    B89__u32 st = packed_code & 0xFF;
    B89__u32 c1 = (packed_code >> 8) & 0xFF;
    B89__u32 c2 = (packed_code >> 16) & 0xFF;
    B89__u32 c3 = (packed_code >> 24) & 0xFF;

    if (st != B89_ST || c1 < B89__ORD_MIN || c1 > B89__ORD_MAX || c2 < B89__ORD_MIN ||
        c2 > B89__ORD_MAX || c3 < B89__ORD_MIN || c3 > B89__ORD_MAX) {
        return 0;
    }

    c1 -= B89__ORD_MIN;
    c2 -= B89__ORD_MIN;
    c3 -= B89__ORD_MIN;

    // horner's method
    B89__u32 index = ((c3 * B89__ORD_BASE) + c2);
    index = (index * B89__ORD_BASE) + c1;

    return index <= max_index ? index : 0; /* 0 is error index */
}


B89DEF int
b89_iter_next(b89_iter_t* it)
{
    if (it->pos >= it->end)
        return 0;

    const B89_CHAR* unemitted_seg = it->pos;
    scan_string_seg(&it->pos, it->end);

    // case: scan found segment of text
    if (unemitted_seg != it->pos) {
        it->event.type = B89_EVENT_TEXT;
        it->event.text.ptr = unemitted_seg;
        it->event.text.len = it->pos - unemitted_seg;
        B89__ASSERT(it->event.text.len > 0);
        return 1;
    }

    // case: scan found B89 ST code
    if (*it->pos == B89_ST) {
        it->event.type = B89_EVENT_CODE;

        // four bytes expected in ST code -- check for truncation
        if (it->end - it->pos < 4) {
            it->event.code.index = B89_ERROR_INDEX;

            // processing terminates
            it->pos = it->end;
            return 0;
        }

        B89__u32 packed_code;
        memcpy(&packed_code, it->pos, 4);
        it->event.code.index = decode_base89(packed_code, it->max_index);

        it->pos += 4;

        return 1;
    }

    // case: end of string
    return 0;
}

B89DEF void
b89_pack(unsigned int index, B89_CHAR* out_bytes)
{
    B89__ASSERT(index <= B89_CODE_MAX);

    unsigned int c1 = index % 89;
    index /= 89;

    unsigned int c2 = index % 89;
    index /= 89;

    unsigned int c3 = index % 89;

    out_bytes[0] = B89_ST;
    out_bytes[1] = c1 + B89__ORD_MIN;
    out_bytes[2] = c2 + B89__ORD_MIN;
    out_bytes[3] = c3 + B89__ORD_MIN;
}

B89DEF void
b89_unpack(const B89_CHAR* bytes, unsigned int max_index, unsigned int* out_index)
{
    B89__u32 qword;
    memcpy(&qword, bytes, 4);

    B89__u32 result = decode_base89(qword, max_index);
    *out_index = result;
}

#    ifdef __cplusplus
}
#    endif

#    undef B89__ORD_MIN
#    undef B89__ORD_MAX
#    undef B89__ORD_BASE

#endif /* defined(B89_IMPLEMENT_BASE89) */
       // todo: call function to handle error
