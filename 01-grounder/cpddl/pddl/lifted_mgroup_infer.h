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

#ifndef __PDDL_LIFTED_MGROUP_INFER_H__
#define __PDDL_LIFTED_MGROUP_INFER_H__

#include <pddl/lifted_mgroup.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Returns true if two or more facts from the initial state are covered by
 * the candidate.
 */
int pddlLiftedMGroupIsInitTooHeavy(const pddl_lifted_mgroup_t *cand,
                                   const pddl_t *pddl);

/**
 * Returns true if the action could add two or more facts from the
 * given candidate.
 */
int pddlLiftedMGroupIsActionTooHeavy(const pddl_lifted_mgroup_t *cand,
                                     const pddl_t *pddl,
                                     int action_id);

/**
 * Returns true if the action is balanced with respect to the given mutex
 * group candidate, i.e., every add effect covered by the candidate has at
 * least one delete effect \cap precondition covered by the candidate.
 */
int pddlLiftedMGroupIsActionBalanced(const pddl_lifted_mgroup_t *cand,
                                     const pddl_t *pddl,
                                     int action_id);

/**
 * Fills dst with (partially) instantiated lifted mutex groups from src
 * that has non-empty intersection with goal.
 */
void pddlLiftedMGroupsExtractGoalAware(pddl_lifted_mgroups_t *dst,
                                       const pddl_lifted_mgroups_t *src,
                                       const pddl_t *pddl);

/**
 * Returns true if the conjuction of atoms grounded using given arguments
 * is too heavy, i.e., if the candidate can be unified with at least two
 * atoms.
 */
int pddlLiftedMGroupsIsGroundedConjTooHeavy(const pddl_lifted_mgroups_t *mgs,
                                            const pddl_t *pddl,
                                            const pddl_cond_arr_t *conj,
                                            const pddl_obj_id_t *conj_args);

/**
 * Returns true if the action (pre, add_eff, del_eff) fully grounded with
 * args deletes the given mutex group, i.e., the resulting state will have
 * empty intersection with mg.
 */
int pddlLiftedMGroupsAnyIsDeleted(const pddl_lifted_mgroups_t *mgs,
                                  const pddl_t *pddl,
                                  const pddl_cond_arr_t *pre,
                                  const pddl_cond_arr_t *add_eff,
                                  const pddl_cond_arr_t *del_eff,
                                  const pddl_obj_id_t *args);

struct pddl_lifted_mgroups_infer_limits {
    /** Maximum of generated candidates. Default: 10000 */
    int max_candidates;
    /** Maximum of proved lifted mutex groups. Default: 10000 */
    int max_mgroups;
};
typedef struct pddl_lifted_mgroups_infer_limits
            pddl_lifted_mgroups_infer_limits_t;

#define PDDL_LIFTED_MGROUPS_INFER_LIMITS_INIT \
    { \
        10000, /* .max_candidates */ \
        10000, /* .max_mgroups */ \
    }

/**
 * Find lifted fam-groups.
 */
void pddlLiftedMGroupsInferFAMGroups(
                            const pddl_t *pddl,
                            const pddl_lifted_mgroups_infer_limits_t *limit,
                            pddl_lifted_mgroups_t *lm,
                            bor_err_t *err);

/**
 * Find monotonicity invariants (as in fast-downward) and stores them in
 * inv if non-NULL. If mgroups is non-NULL, lifted mutex groups based on
 * monotinicity invariants are stored there.
 */
void pddlLiftedMGroupsInferMonotonicity(
                            const pddl_t *pddl,
                            const pddl_lifted_mgroups_infer_limits_t *limit,
                            pddl_lifted_mgroups_t *inv,
                            pddl_lifted_mgroups_t *mgroups,
                            bor_err_t *err);

/**
 * Determine which lifted mgroups are "exactly-one" type and sets the
 * corresponding .is_exactly_one flag.
 */
int pddlLiftedMGroupsSetExactlyOne(const pddl_t *pddl,
                                   pddl_lifted_mgroups_t *lm,
                                   bor_err_t *err);

/**
 * Determine which lifted mgroups are static, i.e., it is either
 * unreachable or one fact from the mgroup is set in the initial state and
 * never changed or deleted.
 */
int pddlLiftedMGroupsSetStatic(const pddl_t *pddl,
                               pddl_lifted_mgroups_t *lm,
                               bor_err_t *err);
#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_LIFTED_MGROUP_H__ */

