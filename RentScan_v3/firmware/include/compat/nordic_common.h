/**
 * @file nordic_common.h
 * @brief Compatibility layer for nordic_common.h from nRF SDK
 */

#ifndef NORDIC_COMMON_COMPAT_H__
#define NORDIC_COMMON_COMPAT_H__

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

/* Compatibility defines - use Zephyr's existing macros */
#define UNUSED_VARIABLE(X)       UNUSED(X)
#define UNUSED_PARAMETER(X)      UNUSED(X)

/* ARRAY_SIZE, MIN and MAX are already defined in Zephyr,
 * so we don't redefine them here to avoid warnings
 */

/* Concatenates two parameters */
#ifndef CONCAT_2
#define CONCAT_2(p1, p2)         UTIL_CAT(p1, p2)
#endif

/* Concatenates three parameters */
#ifndef CONCAT_3
#define CONCAT_3(p1, p2, p3)     UTIL_CAT3(p1, p2, p3)
#endif

/* Set a bit in a value */
#define SET_BIT(W, B)            ((W) |= (1UL << (B)))

/* Clear a bit in a value */
#define CLR_BIT(W, B)            ((W) &= ~(1UL << (B)))

/* Check if a bit is set */
#define IS_SET(W, B)             (((W) >> (B)) & 1)

/* Get field bits from a value */
#define GET_FIELD(W, B, M)       (((W) >> (B)) & (M))

/* Set field bits in a value */
#define SET_FIELD(W, B, M, V)    ((W) = ((W) & ~((M) << (B))) | (((V) & (M)) << (B)))

#endif // NORDIC_COMMON_COMPAT_H__