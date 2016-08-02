/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if !defined(AVB_INSIDE_LIBAVB_H) && !defined(AVB_COMPILATION)
#error "Never include this file directly, include libavb.h instead."
#endif

#ifndef AVB_SYSDEPS_H_
#define AVB_SYSDEPS_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Change these includes to match your platform to bring in the
 * equivalent types available in a normal C runtime, as well as
 * printf()-format specifiers such as PRIx64.
 */
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

/* If you don't have gcc or clang, these attribute macros may need to
 * be adjusted.
 */
#define AVB_ATTR_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#define AVB_ATTR_PACKED __attribute__((packed))
#define AVB_ATTR_FORMAT_PRINTF(format_idx, arg_idx) \
  __attribute__((format(printf, format_idx, arg_idx)))
#define AVB_ATTR_NO_RETURN __attribute__((noreturn))

#ifdef AVB_ENABLE_DEBUG
/* Aborts the program if |expr| is false.
 *
 * This has no effect unless AVB_ENABLE_DEBUG is defined.
 */
#define avb_assert(expr)                                                  \
  do {                                                                    \
    if (!(expr)) {                                                        \
      avb_error("assert fail: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
    }                                                                     \
  } while (0)
#else
#define avb_assert(expr)
#endif

/* Size in bytes used for word-alignment.
 *
 * Change this to match your architecture - must be a power of two.
 */
#define AVB_WORD_ALIGNMENT_SIZE 8

/* Aborts the program if |addr| is not word-aligned.
 *
 * This has no effect unless AVB_ENABLE_DEBUG is defined.
 */
#define avb_assert_word_aligned(addr) \
  avb_assert((((uintptr_t)addr) & (AVB_WORD_ALIGNMENT_SIZE - 1)) == 0)

/* Compare |n| bytes in |src1| and |src2|.
 *
 * Returns an integer less than, equal to, or greater than zero if the
 * first |n| bytes of |src1| is found, respectively, to be less than,
 * to match, or be greater than the first |n| bytes of |src2|. */
int avb_memcmp(const void* src1, const void* src2,
               size_t n) AVB_ATTR_WARN_UNUSED_RESULT;

/* Compare two strings.
 *
 * Return an integer less than, equal to, or greater than zero if |s1|
 * is found, respectively, to be less than, to match, or be greater
 * than |s2|.
 */
int avb_strcmp(const char* s1, const char* s2);

/* Copy |n| bytes from |src| to |dest|. */
void* avb_memcpy(void* dest, const void* src, size_t n);

/* Set |n| bytes starting at |s| to |c|.  Returns |dest|. */
void* avb_memset(void* dest, const int c, size_t n);

/* Compare |n| bytes starting at |s1| with |s2| and return 0 if they
 * match, 1 if they don't.  Returns 0 if |n|==0, since no bytes
 * mismatched.
 *
 * Time taken to perform the comparison is only dependent on |n| and
 * not on the relationship of the match between |s1| and |s2|.
 *
 * Note that unlike avb_memcmp(), this only indicates inequality, not
 * whether |s1| is less than or greater than |s2|.
 */
int avb_safe_memcmp(const void* s1, const void* s2,
                    size_t n) AVB_ATTR_WARN_UNUSED_RESULT;

#ifdef AVB_ENABLE_DEBUG
/* printf()-style function, used for diagnostics.
 *
 * This has no effect unless AVB_ENABLE_DEBUG is defined.
 */
#define avb_debug(format, ...)                                             \
  do {                                                                     \
    avb_print("%s:%d: %s(): DEBUG: " format, __FILE__, __LINE__, __func__, \
              ##__VA_ARGS__);                                              \
  } while (0)
#else
#define avb_debug(format, ...)
#endif

/* Prints out a message (defined by |format|, printf()-style).
 */
void avb_print(const char* format, ...) AVB_ATTR_FORMAT_PRINTF(1, 2);

/* Prints out a message (defined by |format|, printf()-style). This is
 * typically used if a runtime-error occurs.
 */
#define avb_warning(format, ...)                                             \
  do {                                                                       \
    avb_print("%s:%d: %s(): WARNING: " format, __FILE__, __LINE__, __func__, \
              ##__VA_ARGS__);                                                \
  } while (0)

/* Prints out a message (defined by |format|, printf()-style) and
 * calls avb_abort().
 */
#define avb_error(format, ...)                                             \
  do {                                                                     \
    avb_print("%s:%d: %s(): ERROR: " format, __FILE__, __LINE__, __func__, \
              ##__VA_ARGS__);                                              \
    avb_abort();                                                           \
  } while (0)

/* Aborts the program or reboots the device. */
void avb_abort(void) AVB_ATTR_NO_RETURN;

/* Allocates |size| bytes. Returns NULL if no memory is available,
 * otherwise a pointer to the allocated memory.
 *
 * The memory is not initialized.
 *
 * The pointer returned is guaranteed to be word-aligned.
 *
 * The memory should be freed with avb_free() when you are done with it.
 */
void* avb_malloc_(size_t size) AVB_ATTR_WARN_UNUSED_RESULT;

/* Frees memory previously allocated with avb_malloc(). */
void avb_free(void* ptr);

/* Returns the lenght of |str|, excluding the terminating NUL-byte. */
size_t avb_strlen(const char* str) AVB_ATTR_WARN_UNUSED_RESULT;

#ifdef __cplusplus
}
#endif

#endif /* AVB_SYSDEPS_H_ */
