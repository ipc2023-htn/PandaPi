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

#ifndef __PDDL_MGROUP_H__
#define __PDDL_MGROUP_H__

#include <boruvka/iset.h>
#include <pddl/lifted_mgroup.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct pddl_mgroup {
    bor_iset_t mgroup; /*!< Set of facts forming the mutex group */
    int lifted_mgroup_id; /*!< ID refering to the corresponding lifted
                               mutex group in pddl_mgroups_t or
                               -1 if there is none */
    int is_exactly_one; /*!< True if the mutex groups is "exactly-one" */
    int is_fam_group; /*!< True if it is fam-group */
    int is_goal; /*!< Has non-empty intersection with the goal */
};
typedef struct pddl_mgroup pddl_mgroup_t;

struct pddl_mgroups {
    pddl_lifted_mgroups_t lifted_mgroup;
    pddl_mgroup_t *mgroup;
    int mgroup_size;
    int mgroup_alloc;
};
typedef struct pddl_mgroups pddl_mgroups_t;

/**
 * Initialize an empty set of mutex groups.
 */
void pddlMGroupsInitEmpty(pddl_mgroups_t *mg);

/**
 * Initialize dst as a copy of src.
 */
void pddlMGroupsInitCopy(pddl_mgroups_t *dst, const pddl_mgroups_t *src);

/**
 * Ground lifted mutex groups using reachable facts.
 */
void pddlMGroupsGround(pddl_mgroups_t *mg,
                       const pddl_t *pddl,
                       const pddl_lifted_mgroups_t *lifted_mg,
                       const pddl_strips_t *strips);

/**
 * Free allocated memory.
 */
void pddlMGroupsFree(pddl_mgroups_t *mg);

/**
 * Adds a new mutex group consisting of the given set of facts.
 */
pddl_mgroup_t *pddlMGroupsAdd(pddl_mgroups_t *mg, const bor_iset_t *fact);

/**
 * Sorts mutex groups and removes duplicates.
 */
void pddlMGroupsSortUniq(pddl_mgroups_t *mg);

/**
 * Sort mutex groups by their size in descending order.
 */
void pddlMGroupsSortBySizeDesc(pddl_mgroups_t *mg);

/**
 * Sets .is_exactly_one flags for "exactly-one" mutex groups.
 * Returns the number of exactly-one mutex groups found.
 */
int pddlMGroupsSetExactlyOne(pddl_mgroups_t *mgs, const pddl_strips_t *strips);

/**
 * Sets .is_goal flags for mutex groups having non-empty intersection with
 * the goal. Returns the number of "goal" mutex groups found.
 */
int pddlMGroupsSetGoal(pddl_mgroups_t *mgs, const pddl_strips_t *strips);

/**
 * Adds to {set} all facts from all exactly-one mutex groups.
 */
void pddlMGroupsGatherExactlyOneFacts(const pddl_mgroups_t *mgs,
                                      bor_iset_t *set);

/**
 * Remove the specified facts and remap the rest to the new IDs.
 * Note that the flags are not reset to 0.
 */
void pddlMGroupsReduce(pddl_mgroups_t *mgs, const bor_iset_t *rm_facts);

/**
 * Removes all mutex groups containing at most size facts.
 */
void pddlMGroupsRemoveSmall(pddl_mgroups_t *mgs, int size);

/**
 * Removes empty mutex groups
 */
void pddlMGroupsRemoveEmpty(pddl_mgroups_t *mgs);

/**
 * Returns mutex group cover number, i.e., minimal number of mutex groups
 * needed to cover all facts.
 */
int pddlMGroupsCoverNumber(const pddl_mgroups_t *mgs, int fact_size);

/**
 * Debug print out
 */
void pddlMGroupsPrint(const pddl_t *pddl,
                      const pddl_strips_t *strips,
                      const pddl_mgroups_t *mg,
                      FILE *fout);
void pddlMGroupPrint(const pddl_t *pddl,
                     const pddl_strips_t *strips,
                     const pddl_mgroup_t *mg,
                     FILE *fout);
#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_MGROUP_H__ */
