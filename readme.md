# ftg toolbox #

Single C file libraries for C or C++, written by Frogtoss Games, and made public.

## ftg_base89 ##

[ftg_base89.h](https://github.com/frogtoss/ftg_toolbox_public/blob/main/ftg_base89.h)

An implementation of [Printable Base89 Lut Encoding](https://www.frogtoss.com/labs/printable-base89-lut-encoding.html) in C.

This header file lets you encode lookup tables into UTF-8 strings, which you can use for your own purposes including theming or semantic markup.  It includes a reasonably high performance set of functions to process strings containing these encodings, as well as pack and unpack Base89 LUT Encoded sequences.

## ftg_bitbuffer ##

[ftg_bitbuffer.h](https://github.com/frogtoss/ftg_toolbox_public/blob/main/ftg_bitbuffer.h)

Tightly pack values by bits into a stream of bytes for reading / writing.

For example, a 1-bit bool and a 32-bit integer are packed into 33
bits.

Bitbuffers are intended for small amounts of data, such as a few
hundred network packets where size is important enough to remove
padding bits, and the cpu overhead of packing/unpacking intermixed
types is not a huge cost.

 - Compiles C99 warnings-free on clang and visual c++

 - Pack integers with arbitrary numbers of bits

 - Supports quantized floating point packing

 - Possible to avoid all heap allocations and copies on read


