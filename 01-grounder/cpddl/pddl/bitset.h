/***
 * cpddl
 * -------
 * Copyright (c)2019 Daniel Fiser <danfis@danfis.cz>,
 * AI Center, Department of Computer Science,
 * Faculty of Electrical Engineering, Czech Technical University in Prague.
 * All rights reserved.
 *
 * This file is part of cpddl.
 *
 * Distributed under the OSI-approved BSD License (the "License");
 * see accompanying file BDS-LICENSE for details or see
 * <http://www.opensource.org/licenses/bsd-license.php>.
 *
 * This software is distributed WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the License for more information.
 */

#ifndef __PDDL_BITSET_H__
#define __PDDL_BITSET_H__

#include <pddl/strips.h>
#include <pddl/mutex_pair.h>
#include <pddl/mgroup.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

//typedef unsigned short pddl_bitset_word_t;
typedef unsigned long long pddl_bitset_word_t;
#define PDDL_BITSET_WORD_BITSIZE (sizeof(pddl_bitset_word_t) * 8)

#define PDDL_BITSET_POPCOUNT(X) __builtin_popcountl(X)
#define PDDL_BITSET_FFS(X) __builtin_ffsl(X)

struct pddl_bitset {
    pddl_bitset_word_t *bitset;
    int wordsize;
    int bitsize;
    pddl_bitset_word_t last_word_mask;

    int it_id;
    int it_word;
};
typedef struct pddl_bitset pddl_bitset_t;

/**
 * Initializes bitset of the given size.
 */
void pddlBitsetInit(pddl_bitset_t *b, int bitsize);

/**
 * Frees allocated memory.
 */
void pddlBitsetFree(pddl_bitset_t *b);

/**
 * Sets the specified bit to 1.
 */
_bor_inline void pddlBitsetSetBit(pddl_bitset_t *b, int bit)
{
    int word = bit / PDDL_BITSET_WORD_BITSIZE;
    pddl_bitset_word_t shift = bit % PDDL_BITSET_WORD_BITSIZE;
    b->bitset[word] |= ((pddl_bitset_word_t)1) << shift;
}

_bor_inline void pddlBitsetClearBit(pddl_bitset_t *b, int bit)
{
    int word = bit / PDDL_BITSET_WORD_BITSIZE;
    pddl_bitset_word_t shift = bit % PDDL_BITSET_WORD_BITSIZE;
    b->bitset[word] &= ~(((pddl_bitset_word_t)1) << shift);
}

/**
 * Zeroize the whole bitset.
 */
_bor_inline void pddlBitsetZeroize(pddl_bitset_t *b)
{
    bzero(b->bitset, sizeof(pddl_bitset_word_t) * b->wordsize);
}

/**
 * Negate the bitset.
 */
_bor_inline void pddlBitsetNeg(pddl_bitset_t *b)
{
    for (int i = 0; i < b->wordsize; ++i)
        b->bitset[i] = ~b->bitset[i];
    b->bitset[b->wordsize - 1] &= b->last_word_mask;
}

/**
 * Copy the bitset from src to dst.
 * It is assumed that both bitsets have the same size.
 */
_bor_inline void pddlBitsetCopy(pddl_bitset_t *dst, const pddl_bitset_t *src)
{
    memcpy(dst->bitset, src->bitset,
           sizeof(pddl_bitset_word_t) * src->wordsize);
}

_bor_inline void pddlBitsetAnd(pddl_bitset_t *dst, const pddl_bitset_t *src)
{
    for (int i = 0; i < src->wordsize; ++i)
        dst->bitset[i] &= src->bitset[i];
}

_bor_inline void pddlBitsetOr(pddl_bitset_t *dst, const pddl_bitset_t *src)
{
    for (int i = 0; i < src->wordsize; ++i)
        dst->bitset[i] |= src->bitset[i];
}

/**
 * Computes dst = s1 & s2 but terminates as soon as the number of ones is
 * higher than one and returns the detected number of ones.
 */
_bor_inline int pddlBitsetAnd2Cnt1(pddl_bitset_t *dst,
                                   const pddl_bitset_t *s1,
                                   const pddl_bitset_t *s2)
{
    int cnt = 0;
    for (int i = 0; i < s1->wordsize; ++i){
        dst->bitset[i] = s1->bitset[i] & s2->bitset[i];
        cnt += PDDL_BITSET_POPCOUNT(dst->bitset[i]);
        if (cnt > 1)
            return cnt;
    }
    return cnt;
}

/**
 * Returns number of ones in the bitset.
 */
_bor_inline int pddlBitsetCount(const pddl_bitset_t *b)
{
    int cnt = 0;
    for (int i = 0; i < b->wordsize; ++i)
        cnt += PDDL_BITSET_POPCOUNT(b->bitset[i]);
    return cnt;
}


/**
 * Starts bit iterator
 */
_bor_inline void pddlBitsetItStart(pddl_bitset_t *b)
{
    b->it_id = -1;
    b->it_word = 0;
}

/**
 * Returns next position of a set bit using the current iterator.
 */
_bor_inline int pddlBitsetItNext(pddl_bitset_t *b)
{
    int ffs = PDDL_BITSET_FFS(b->bitset[b->it_word]);
    while (ffs <= 0){
        if (++b->it_word == b->wordsize)
            return -1;
        ffs = PDDL_BITSET_FFS(b->bitset[b->it_word]);
        b->it_id = b->it_word * PDDL_BITSET_WORD_BITSIZE - 1;
    }
    b->bitset[b->it_word] >>= (pddl_bitset_word_t)ffs;
    b->it_id += ffs;
    return b->it_id;
}

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_BITSET_H__ */
