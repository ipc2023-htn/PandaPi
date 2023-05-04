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

#ifndef __PDDL_COND_ARR_H__
#define __PDDL_COND_ARR_H__

#include <pddl/cond.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_cond_arr {
    const pddl_cond_t **cond;
    int size;
    int alloc;
};
typedef struct pddl_cond_arr pddl_cond_arr_t;

#define PDDL_COND_ARR_INIT { 0 }

void pddlCondArrInit(pddl_cond_arr_t *ca);
void pddlCondArrFree(pddl_cond_arr_t *ca);
void pddlCondArrAdd(pddl_cond_arr_t *ca, const pddl_cond_t *c);
void pddlCondArrInitCopy(pddl_cond_arr_t *dst, const pddl_cond_arr_t *src);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_COND_ARR_H__ */
