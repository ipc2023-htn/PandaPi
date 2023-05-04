/***
 * cpddl
 * -------
 * Copyright (c)2016 Daniel Fiser <danfis@danfis.cz>,
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

/**
 * This module implementes disambiguation as described in:
 *
 * Alcázar, V., Borrajo, D., Fernández, S., & Fuentetaja, R. (2013).
 * Revisiting regression in planning. In Proceedings of the Twenty-Third
 * International Joint Conference on Artificial Intelligence (IJCAI), pp.
 * 2254–2260.
 */

#ifndef __PDDL_DISAMBIGUATION_H__
#define __PDDL_DISAMBIGUATION_H__

#include <pddl/strips.h>
#include <pddl/mutex_pair.h>
#include <pddl/mgroup.h>
#include <pddl/bitset.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_disambiguate_fact {
    pddl_bitset_t mgroup; /*!< mgroups that does not contian this fact */
    pddl_bitset_t fact; /*!< facts that are not mutex with this fact */
};
typedef struct pddl_disambiguate_fact pddl_disambiguate_fact_t;

struct pddl_disambiguate_mgroup {
    pddl_bitset_t fact; /*!< Facts contained in the mutex group */
};
typedef struct pddl_disambiguate_mgroup pddl_disambiguate_mgroup_t;

struct pddl_disambiguate {
    pddl_disambiguate_fact_t *fact;
    int fact_size;
    pddl_disambiguate_mgroup_t *mgroup;
    int mgroup_size;

    /** Preallocated bitset for internal computation */
    pddl_bitset_t cur_mgroup;
    pddl_bitset_t cur_mgroup_it;
    pddl_bitset_t cur_allowed_facts;
    pddl_bitset_t cur_allowed_facts_from_mgroup;
};
typedef struct pddl_disambiguate pddl_disambiguate_t;


/**
 * Initialize disambiguation object.
 * Returns 0 on success, -1 if there are no exactly-one mutex groups in
 * which case dis is not properly intitialized.
 */
int pddlDisambiguateInit(pddl_disambiguate_t *dis,
                         int fact_size,
                         const pddl_mutex_pairs_t *mutex,
                         const pddl_mgroups_t *mgroup);

/**
 * Free allocated memory.
 */
void pddlDisambiguateFree(pddl_disambiguate_t *dis);

/**
 * Disambiguate a set of facts.
 * Return 0 if nothing was changed,
 *        1 if some facts were added
 *        -1 if the set was detected to be mutex
 */
int pddlDisambiguateSet(pddl_disambiguate_t *dis, bor_iset_t *set);

/**
 * Update structure with additional mutex {f1, f2}.
 */
void pddlDisambiguateAddMutex(pddl_disambiguate_t *dis, int f1, int f2);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_DISAMBIGUATION_H__ */
