/***
 * cpddl
 * -------
 * Copyright (c)2018 Daniel Fiser <danfis@danfis.cz>,
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
#include "pddl/mutex_pair.h"
#include "pddl/strips.h"

#define M(m, f1, f2) ((m)->map[(f1) * (size_t)(m)->fact_size + (f2)])

void pddlMutexPairsInit(pddl_mutex_pairs_t *m, int fact_size)
{
    bzero(m, sizeof(*m));
    m->fact_size = fact_size;
    m->map = BOR_CALLOC_ARR(char, (size_t)fact_size * fact_size);
}

void pddlMutexPairsInitStrips(pddl_mutex_pairs_t *m, const pddl_strips_t *s)
{
    pddlMutexPairsInit(m, s->fact.fact_size);
}

void pddlMutexPairsInitCopy(pddl_mutex_pairs_t *dst,
                            const pddl_mutex_pairs_t *src)
{
    pddlMutexPairsInit(dst, src->fact_size);
    memcpy(dst->map, src->map,
           sizeof(char) * (size_t)src->fact_size * src->fact_size);
    dst->num_mutex_pairs = src->num_mutex_pairs;
}

void pddlMutexPairsFree(pddl_mutex_pairs_t *m)
{
    if (m->map != NULL)
        BOR_FREE(m->map);
}

void pddlMutexPairsEmpty(pddl_mutex_pairs_t *m, int fact_size)
{
    if (fact_size > 0){
        pddlMutexPairsFree(m);
        pddlMutexPairsInit(m, fact_size);
    }else{
        bzero(m->map, sizeof(*m->map) * m->fact_size * m->fact_size);
    }
}

int pddlMutexPairsAdd(pddl_mutex_pairs_t *m, int f1, int f2)
{
    if (f1 >= m->fact_size || f2 >= m->fact_size)
        return -1;
    if (f1 == f2){
        for (int i = 0; i < m->fact_size; ++i){
            if (!M(m, f1, i) && f1 != i)
                ++m->num_mutex_pairs;
            M(m, f1, i) = M(m, i, f1) = 1;
        }
    }else{
        if (!M(m, f1, f2) && f1 != f2)
            ++m->num_mutex_pairs;
        M(m, f1, f2) = M(m, f2, f1) = 1;
    }
    return 0;
}

int pddlMutexPairsIsMutex(const pddl_mutex_pairs_t *m, int f1, int f2)
{
    return M(m, f1, f2);
}


int pddlMutexPairsIsMutexSet(const pddl_mutex_pairs_t *m, const bor_iset_t *fs)
{
    const int size = borISetSize(fs);
    for (int i = 0; i < size; ++i){
        int f1 = borISetGet(fs, i);
        for (int j = i; j < size; ++j){
            int f2 = borISetGet(fs, j);
            if (M(m, f1, f2))
                return 1;
        }
    }
    return 0;
}

int pddlMutexPairsIsMutexFactSet(const pddl_mutex_pairs_t *m,
                                 int fact, const bor_iset_t *fs)
{
    int fact2;

    BOR_ISET_FOR_EACH(fs, fact2){
        if (M(m, fact, fact2))
            return 1;
    }
    return 0;
}

int pddlMutexPairsIsMutexSetSet(const pddl_mutex_pairs_t *m,
                                const bor_iset_t *fs1, const bor_iset_t *fs2)
{
    int f1, f2;
    BOR_ISET_FOR_EACH(fs1, f1){
        BOR_ISET_FOR_EACH(fs2, f2){
            if (M(m, f1, f2))
                return 1;
        }
    }
    return 0;
}

void pddlMutexPairsRemapFacts(pddl_mutex_pairs_t *m,
                              int new_fact_size,
                              const int *remap)
{
    pddl_mutex_pairs_t old = *m;

    pddlMutexPairsInit(m, new_fact_size);
    for (int i = 0; i < old.fact_size; ++i){
        if (remap[i] < 0)
            continue;
        for (int j = i + 1; j < old.fact_size; ++j){
            if (remap[j] < 0)
                continue;
            if (pddlMutexPairsIsMutex(&old, i, j))
                pddlMutexPairsAdd(m, remap[i], remap[j]);
        }
    }

    pddlMutexPairsFree(&old);
}

void pddlMutexPairsReduce(pddl_mutex_pairs_t *m, const bor_iset_t *rm_facts)
{
    if (borISetSize(rm_facts) == 0)
        return;

    int *remap = BOR_CALLOC_ARR(int, m->fact_size);
    int new_size = pddlFactsDelFactsGenRemap(m->fact_size, rm_facts, remap);
    pddlMutexPairsRemapFacts(m, new_size, remap);
    if (remap != NULL)
        BOR_FREE(remap);
}

void pddlMutexPairsAddMGroup(pddl_mutex_pairs_t *mutex,
                             const pddl_mgroup_t *mg)
{
    const bor_iset_t *facts = &mg->mgroup;
    int size = borISetSize(facts);

    for (int i = 0; i < size; ++i){
        for (int j = i + 1; j < size; ++j){
            pddlMutexPairsAdd(mutex, borISetGet(facts, i),
                                     borISetGet(facts, j));
        }
    }
}

void pddlMutexPairsAddMGroups(pddl_mutex_pairs_t *mutex,
                              const pddl_mgroups_t *mgs)
{
    for (int mgi = 0; mgi < mgs->mgroup_size; ++mgi){
        const pddl_mgroup_t *mg = mgs->mgroup + mgi;
        pddlMutexPairsAddMGroup(mutex, mg);
    }
}
