/***
 * cpddl
 * -------
 * Copyright (c)2017 Daniel Fiser <danfis@danfis.cz>,
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

#include <boruvka/alloc.h>
#include "pddl/cond_arr.h"

void pddlCondArrInit(pddl_cond_arr_t *ca)
{
    bzero(ca, sizeof(*ca));
}

void pddlCondArrFree(pddl_cond_arr_t *ca)
{
    if (ca->cond)
        BOR_FREE(ca->cond);
}

void pddlCondArrAdd(pddl_cond_arr_t *ca, const pddl_cond_t *c)
{
    if (ca->size >= ca->alloc){
        if (ca->alloc == 0)
            ca->alloc = 1;
        ca->alloc *= 2;
        ca->cond = BOR_REALLOC_ARR(ca->cond, const pddl_cond_t *, ca->alloc);
    }
    ca->cond[ca->size++] = c;
}

void pddlCondArrInitCopy(pddl_cond_arr_t *dst, const pddl_cond_arr_t *src)
{
    *dst = *src;
    if (src->cond != NULL){
        dst->cond = BOR_ALLOC_ARR(const pddl_cond_t *, dst->alloc);
        memcpy(dst->cond, src->cond, sizeof(pddl_cond_t *) * src->size);
    }
}
