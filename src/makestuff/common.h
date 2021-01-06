/*
 * Copyright (C) 2009-2012 Chris McClelland
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MAKESTUFF_COMMON_H
#define MAKESTUFF_COMMON_H

#include <stddef.h>

#ifndef __cplusplus
	#ifdef _MSC_VER
		typedef char bool;
		enum {
			false = 0,
			true = 1
		};
	#else
		#include <stdbool.h>
	#endif
#endif

#ifdef _MSC_VER
	#define WARN_UNUSED_RESULT
	#define DLLEXPORT(t) __declspec(dllexport) t __stdcall
	#define PFSZD "%Iu"
	#ifdef _WIN64
		#define PFSZH "%016IX"
		#define WORD_LENGTH 64
	#else
		#define PFSZH "%08IX"
		#define WORD_LENGTH 32
	#endif
#else
	#define WARN_UNUSED_RESULT __attribute__((warn_unused_result))
	#define DLLEXPORT(t) t
	#define PFSZD "%zu"
	#ifdef __LP64__
		#define PFSZH "%016zX"
		#define WORD_LENGTH 64
	#else
		#define PFSZH "%08zX"
		#define WORD_LENGTH 32
	#endif
#endif

#ifndef NULL
	#define NULL ((void*)0)
#endif

typedef unsigned char      uint8;
typedef unsigned short     uint16;
#ifndef __cplusplus
	#ifndef SDCC
		typedef unsigned long long uint64;
	#endif
#endif

typedef signed char        int8;
typedef signed short       int16;

#if (defined __AVR__ && defined __GNUC__) || defined SDCC
	// The embedded platforms have sizeof(int) = 2, so use long
	typedef signed long    int32;
	typedef unsigned long  uint32;
#else
	// The i686 & x86_64 have sizeof(int) = 4
	typedef signed int     int32;
	typedef unsigned int   uint32;
#endif

#ifndef __cplusplus
	#ifndef SDCC
		typedef signed long long int64;
	#endif
#endif

typedef unsigned int       bitfield;

#if defined __GNUC__
	#define swap32(x) __builtin_bswap32(x)
#elif defined WIN32
	#ifdef __cplusplus
		extern "C"
	#endif
	unsigned long  __cdecl _byteswap_ulong(unsigned long);
	#define swap32(x) _byteswap_ulong(x)
	#ifndef __cplusplus
		#define inline __inline
	#endif
#endif
#define swap16(x) ((uint16)((((x) & 0x00FF) << 8) | (((x) >> 8) & 0x00FF)))

// The C standard requires this two-level indirection thing
#undef CONCAT
#define CONCAT_INTERNAL(x, y) x ## y
#define CONCAT(x, y) CONCAT_INTERNAL(x, y)

#define STR_INTERNAL(x) #x
#define STR(x) STR_INTERNAL(x)

// The VA_NARGS() macro - count the number of arguments in a C99 variadic macro
#define VA_EXPAND(x) x
#define VA_NARGS(...) VA_EXPAND(VA_NARGS_INTERNAL(__VA_ARGS__, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 4, 3, 2, 1))
#define VA_NARGS_INTERNAL(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z, N, ...) N

#define FAIL_INTERNAL2(code, label) { retVal = code; goto label; }
#define FAIL_INTERNAL3(code, label, prefix) LIBERROR_IS_REQUIRED
#define FAIL_INTERNAL4(code, label, prefix, arg) FAIL_INTERNAL5(code, label, prefix, arg)
#define FAIL_INTERNAL5(code, label, ...) LIBERROR_IS_REQUIRED
#define FAIL_RET(...) VA_EXPAND(CONCAT(FAIL_INTERNAL, VA_NARGS(__VA_ARGS__))(__VA_ARGS__))

// The CHECK_STATUS() macro - if condition is true, set a returnCode and jump to a label (exit,
// cleanup etc). If liberror is included you can also give an error message.
#define CHECK_INTERNAL3(condition, code, label) if ( condition ) { FAIL_RET(code, label); }
#define CHECK_INTERNAL4(condition, code, label, prefix) LIBERROR_IS_REQUIRED
#define CHECK_INTERNAL5(condition, code, label, ...) LIBERROR_IS_REQUIRED
#define CHECK_STATUS(...) VA_EXPAND(CONCAT(CHECK_INTERNAL, VA_NARGS(__VA_ARGS__))(__VA_ARGS__))

#ifdef BYTE_ORDER
	#if BYTE_ORDER == 1234
		// Little-endian machines
		static inline uint16 bigEndian16(uint16 x) {
			return swap16(x);
		}
		static inline uint32 bigEndian32(uint32 x) {
			return swap32(x);
		}
		static inline uint16 littleEndian16(uint16 x) {
			return x;
		}
		static inline uint32 littleEndian32(uint32 x) {
			return x;
		}
	#elif BYTE_ORDER == 4321
		// Big-endian machines
		static inline uint16 bigEndian16(uint16 x) {
			return x;
		}
		static inline uint32 bigEndian32(uint32 x) {
			return x;
		}
		static inline uint16 littleEndian16(uint16 x) {
			return swap16(x);
		}
		static inline uint32 littleEndian32(uint32 x) {
			return swap32(x);
		}
	#else
		#error Unsupported BYTE_ORDER
	#endif
#endif

#endif
