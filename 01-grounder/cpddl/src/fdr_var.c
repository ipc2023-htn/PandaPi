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
#include <boruvka/sort.h>
#include "pddl/fdr_var.h"
#include "assert.h"

struct vars_mgroup {
    bor_iset_t uncovered; /*!< The set of uncovered facts from the mgroup */
    const pddl_mgroup_t *mgroup; /*!< The original mgroup */
};
typedef struct vars_mgroup vars_mgroup_t;

struct vars_mgroups {
    vars_mgroup_t *mgroup;
    int mgroup_size;
    const pddl_mgroups_t *mgroups;
    int has_uncovered;
};
typedef struct vars_mgroups vars_mgroups_t;

static void varsMGroupsInit(vars_mgroups_t *vmgs, const pddl_mgroups_t *mgs)
{
    bzero(vmgs, sizeof(*vmgs));
    vmgs->mgroups = mgs;
    vmgs->has_uncovered = 0;
    vmgs->mgroup_size = mgs->mgroup_size;
    vmgs->mgroup = BOR_CALLOC_ARR(vars_mgroup_t, vmgs->mgroup_size);
    for (int i = 0; i < vmgs->mgroup_size; ++i){
        vmgs->mgroup[i].mgroup = mgs->mgroup + i;
        borISetUnion(&vmgs->mgroup[i].uncovered, &mgs->mgroup[i].mgroup);
        if (borISetSize(&vmgs->mgroup[i].uncovered) > 0)
            vmgs->has_uncovered = 1;
    }
}

static void varsMGroupsFree(vars_mgroups_t *vmgs)
{
    for (int i = 0; i < vmgs->mgroup_size; ++i)
        borISetFree(&vmgs->mgroup[i].uncovered);
    if (vmgs->mgroup != NULL)
        BOR_FREE(vmgs->mgroup);
}

/** Cover the given facts */
static void varsMGroupsCover(vars_mgroups_t *vmgs, const bor_iset_t *_facts)
{
    // Copy the set in case _facts points to .uncovered of some vars_mgroup
    BOR_ISET(facts);
    borISetUnion(&facts, _facts);
    vmgs->has_uncovered = 0;
    for (int i = 0; i < vmgs->mgroup_size; ++i){
        vars_mgroup_t *m = vmgs->mgroup + i;
        borISetMinus(&m->uncovered, &facts);
        if (borISetSize(&m->uncovered) > 0)
            vmgs->has_uncovered = 1;
    }
    borISetFree(&facts);
}

/** Return the first vars_mgroup that has uncovered one of the given facts */
static const vars_mgroup_t *varsMGroupsFindCovering(const vars_mgroups_t *vmgs,
                                                    const bor_iset_t *facts)
{
    if (borISetSize(facts) == 0)
        return NULL;

    for (int i = 0; i < vmgs->mgroup_size; ++i){
        const vars_mgroup_t *m = vmgs->mgroup + i;
        if (borISetSize(&m->uncovered) > 0
                && !borISetIsDisjunct(&m->uncovered, facts)){
            return m;
        }
    }
    return NULL;
}

static int varsMGroupUncoveredDescCmp(const void *a, const void *b, void *_)
{
    const vars_mgroup_t *m1 = a;
    const vars_mgroup_t *m2 = b;
    int cmp = borISetSize(&m2->uncovered) - borISetSize(&m1->uncovered);
    if (cmp == 0)
        cmp = borISetCmp(&m1->uncovered, &m2->uncovered);
    if (cmp == 0)
        cmp = m1->mgroup->lifted_mgroup_id - m2->mgroup->lifted_mgroup_id;
    return cmp;
}

/** Sort mgroups in a descending order by the number of uncovered facts */
static void varsMGroupsSortUncoveredDesc(vars_mgroups_t *vmgs)
{
    borSort(vmgs->mgroup, vmgs->mgroup_size, sizeof(*vmgs->mgroup),
            varsMGroupUncoveredDescCmp, NULL);

}




void pddlFDRValInit(pddl_fdr_val_t *val)
{
    bzero(val, sizeof(*val));
}

void pddlFDRValFree(pddl_fdr_val_t *val)
{
    if (val->name != NULL)
        BOR_FREE(val->name);
}

void pddlFDRVarInit(pddl_fdr_var_t *var)
{
    bzero(var, sizeof(*var));
}

void pddlFDRVarFree(pddl_fdr_var_t *var)
{
    for (int i = 0; i < var->val_size; ++i)
        pddlFDRValFree(var->val + i);
    if (var->val != NULL)
        BOR_FREE(var->val);
}

/** Find facts that must be encoded as binary because they appear as delete
 *  effect, but it does not switch to any other fact (considering
 *  mutexes/mutex groups) and it may or may not be part of the state in
 *  which the operator is applied in.
 *  Consider the following example:
 *      operator: pre: f_1, add: f_2, del: f_3
 *      mutex group: { f_3, f_4 }
 *  Now, how can we encode del: f_3 using the given mutex group?
 *  If we encode it as "none-of-those", then the resulting state from the
 *  application of the operator on the state {f_1, f_4} would be incorrect.
 *  This is why f_3 must be encoded separately from all other mutex groups,
 *  otherwise we would need to create potentially exponentially more
 *  operators.
 */
static void factsRequiringBinaryEncoding(const pddl_strips_t *strips,
                                         const pddl_mgroups_t *mgs,
                                         const pddl_mutex_pairs_t *mutex,
                                         bor_iset_t *binfs)
{
    const pddl_strips_op_t *op;
    int fact_id;

    int *mg_facts = BOR_CALLOC_ARR(int, strips->fact.fact_size);
    for (int mi = 0; mi < mgs->mgroup_size; ++mi){
        int fact_id;
        BOR_ISET_FOR_EACH(&mgs->mgroup[mi].mgroup, fact_id)
            mg_facts[fact_id] = 1;
    }

    PDDL_STRIPS_OPS_FOR_EACH(&strips->op, op){
        BOR_ISET_FOR_EACH(&op->del_eff, fact_id){
            if (!mg_facts[fact_id])
                continue;

            // This is the most common case -- the delete effect is
            // required by the precondition.
            if (borISetIn(fact_id, &op->pre))
                continue;

            // If the fact is mutex with the precondition than this delete
            // effect can be safely ignored (in fact it could have been
            // pruned away before), because this fact could not be part of
            // the state on which the operator is applied.
            if (pddlMutexPairsIsMutexFactSet(mutex, fact_id, &op->pre))
                continue;

            borISetAdd(binfs, fact_id);
        }
    }

    if (mg_facts != NULL)
        BOR_FREE(mg_facts);
}

static int needNoneOfThoseOp(const bor_iset_t *group,
                             const pddl_strips_op_t *op,
                             const pddl_mutex_pairs_t *mutex)
{
    if (!borISetIsDisjunct(group, &op->add_eff))
        return 0;

    int ret = 0;
    BOR_ISET(inter);
    borISetIntersect2(&inter, group, &op->del_eff);
    if (borISetSize(&inter) > 0){
        int fact_id;
        BOR_ISET_FOR_EACH(&inter, fact_id){
            if (!pddlMutexPairsIsMutexFactSet(mutex, fact_id, &op->pre)){
                ret = 1;
                break;
            }
        }
    }
    borISetFree(&inter);
    return ret;
}

static int needNoneOfThose(const bor_iset_t *group,
                           const pddl_strips_t *strips,
                           const pddl_mutex_pairs_t *mutex)
{
    if (borISetIsDisjunct(group, &strips->init))
        return 1;
    if (borISetSize(group) == 1)
        return 1;

    const pddl_strips_op_t *op;
    PDDL_STRIPS_OPS_FOR_EACH(&strips->op, op){
        if (needNoneOfThoseOp(group, op, mutex))
            return 1;
    }
    return 0;
}

struct var {
    bor_iset_t fact; /*!< List of STRIPS facts the variable will be created
                          from */
    int none_of_those; /*!< True if "none-of-those" is required */
};
typedef struct var var_t;

struct vars {
    var_t *var;
    int var_size;
    int var_alloc;

    bor_iset_t covered; /*!< Facts that are already covered by the vars
                             above. */
    vars_mgroups_t mgroups;
};
typedef struct vars vars_t;

static void varsInit(vars_t *vars, const pddl_mgroups_t *mgroups)
{
    bzero(vars, sizeof(*vars));
    vars->var_alloc = 8;
    vars->var = BOR_CALLOC_ARR(var_t, vars->var_alloc);
    varsMGroupsInit(&vars->mgroups, mgroups);
}

static void varsFree(vars_t *vars)
{
    for (int i = 0; i < vars->var_size; ++i)
        borISetFree(&vars->var[i].fact);
    if (vars->var != NULL)
        BOR_FREE(vars->var);
    borISetFree(&vars->covered);
    varsMGroupsFree(&vars->mgroups);
}

static void varsAdd(vars_t *vars,
                    const pddl_strips_t *strips,
                    const pddl_mutex_pairs_t *mutex,
                    const bor_iset_t *facts)
{
    var_t *var;

    if (vars->var_size == vars->var_alloc){
        vars->var_alloc *= 2;
        vars->var = BOR_REALLOC_ARR(vars->var, var_t, vars->var_alloc);
    }

    var = vars->var + vars->var_size++;
    bzero(var, sizeof(*var));
    borISetUnion(&var->fact, facts);
    var->none_of_those = needNoneOfThose(&var->fact, strips, mutex);

    borISetUnion(&vars->covered, &var->fact);
    varsMGroupsCover(&vars->mgroups, facts);
}

static void findEssentialFacts(const pddl_strips_t *strips,
                               const pddl_mgroups_t *mgroup,
                               bor_iset_t *essential)
{
    int *fact_mgroups;

    fact_mgroups = BOR_CALLOC_ARR(int, strips->fact.fact_size);
    for (int i = 0; i < mgroup->mgroup_size; ++i){
        int fact;
        BOR_ISET_FOR_EACH(&mgroup->mgroup[i].mgroup, fact)
            ++fact_mgroups[fact];
    }

    for (int fact_id = 0; fact_id < strips->fact.fact_size; ++fact_id){
        if (fact_mgroups[fact_id] == 1)
            borISetAdd(essential, fact_id);
    }

    BOR_FREE(fact_mgroups);
}

static void allocateEssential(vars_t *vars,
                              const pddl_strips_t *strips,
                              const pddl_mgroups_t *mg,
                              const pddl_mutex_pairs_t *mutex)
{
    BOR_ISET(essential);
    findEssentialFacts(strips, mg, &essential);

    while (vars->mgroups.has_uncovered){
        varsMGroupsSortUncoveredDesc(&vars->mgroups);
        ASSERT(borISetSize(&vars->mgroups.mgroup[0].uncovered) > 0);

        const vars_mgroup_t *ess;
        ess = varsMGroupsFindCovering(&vars->mgroups, &essential);
        if (ess != NULL){
            borISetMinus(&essential, &ess->uncovered);
            varsAdd(vars, strips, mutex, &ess->uncovered);
        }else{
            const vars_mgroup_t *m = vars->mgroups.mgroup + 0;
            borISetMinus(&essential, &m->uncovered);
            varsAdd(vars, strips, mutex, &m->uncovered);
        }
    }

    borISetFree(&essential);
}

static void allocateLargest(vars_t *vars,
                            const pddl_strips_t *strips,
                            const pddl_mgroups_t *mg,
                            const pddl_mutex_pairs_t *mutex)
{
    while (vars->mgroups.has_uncovered){
        varsMGroupsSortUncoveredDesc(&vars->mgroups);
        ASSERT(borISetSize(&vars->mgroups.mgroup[0].uncovered) > 0);
        const vars_mgroup_t *m = vars->mgroups.mgroup + 0;
        varsAdd(vars, strips, mutex, &m->uncovered);
    }
}

static void allocateLargestMulti(vars_t *vars,
                                 const pddl_strips_t *strips,
                                 const pddl_mgroups_t *mg,
                                 const pddl_mutex_pairs_t *mutex)
{
    while (vars->mgroups.has_uncovered){
        varsMGroupsSortUncoveredDesc(&vars->mgroups);
        ASSERT(borISetSize(&vars->mgroups.mgroup[0].uncovered) > 0);
        const vars_mgroup_t *m = vars->mgroups.mgroup + 0;
        varsAdd(vars, strips, mutex, &m->mgroup->mgroup);
    }
}

static void allocateUncoveredSingleFacts(vars_t *vars,
                                         const pddl_strips_t *strips,
                                         const pddl_mutex_pairs_t *mutex)
{
    BOR_ISET(var_facts);

    int *covered = BOR_CALLOC_ARR(int, strips->fact.fact_size);
    int fact_id;
    BOR_ISET_FOR_EACH(&vars->covered, fact_id)
        covered[fact_id] = 1;

    for (int fact_id = 0; fact_id < strips->fact.fact_size; ++fact_id){
        if (covered[fact_id])
            continue;

        borISetEmpty(&var_facts);
        borISetAdd(&var_facts, fact_id);
        covered[fact_id] = 1;

        int neg_of = strips->fact.fact[fact_id]->neg_of;
        if (neg_of >= 0 && !covered[neg_of]){
            borISetAdd(&var_facts, neg_of);
            covered[neg_of] = 1;
        }
        varsAdd(vars, strips, mutex, &var_facts);
    }

    if (covered != NULL)
        BOR_FREE(covered);
    borISetFree(&var_facts);
}

static int allocateVars(vars_t *vars,
                        const pddl_strips_t *strips,
                        const pddl_mgroups_t *mg,
                        const pddl_mutex_pairs_t *mutex,
                        unsigned flags)
{
    BOR_ISET(var_facts);
    BOR_ISET(binary_facts);
    int fact;

    // Find facts that must be encoded in binary no mather what and create
    // variables from them
    factsRequiringBinaryEncoding(strips, mg, mutex, &binary_facts);
    BOR_ISET_FOR_EACH(&binary_facts, fact){
        borISetEmpty(&var_facts);
        borISetAdd(&var_facts, fact);
        varsAdd(vars, strips, mutex, &var_facts);
    }

    if (flags == PDDL_FDR_VARS_ESSENTIAL_FIRST){
        allocateEssential(vars, strips, mg, mutex);
    }else if (flags == PDDL_FDR_VARS_LARGEST_FIRST){
        allocateLargest(vars, strips, mg, mutex);
    }else if (flags == PDDL_FDR_VARS_LARGEST_FIRST_MULTI){
        allocateLargestMulti(vars, strips, mg, mutex);
    }else{
        // TODO
        fprintf(stderr, "Error: Unspecified method for variable allocation.\n");
        return -1;
    }

    allocateUncoveredSingleFacts(vars, strips, mutex);

    borISetFree(&var_facts);
    borISetFree(&binary_facts);
    return 0;
}

static void createVars(pddl_fdr_vars_t *fdr_vars,
                       const vars_t *vars,
                       const pddl_strips_t *strips)
{
    fdr_vars->strips_id_size = strips->fact.fact_size;
    fdr_vars->strips_id_to_val = BOR_CALLOC_ARR(bor_iset_t,
                                                strips->fact.fact_size);
    fdr_vars->var_size = vars->var_size;
    fdr_vars->var = BOR_CALLOC_ARR(pddl_fdr_var_t, vars->var_size);

    // Determine number of global IDs
    fdr_vars->global_id_size = 0;
    for (int i = 0; i < vars->var_size; ++i){
        fdr_vars->global_id_size += borISetSize(&vars->var[i].fact);
        fdr_vars->global_id_size += (vars->var[i].none_of_those ? 1 : 0);
    }
    fdr_vars->global_id_to_val = BOR_CALLOC_ARR(pddl_fdr_val_t *,
                                                fdr_vars->global_id_size);

    int global_id = 0;
    for (int var_id = 0; var_id < vars->var_size; ++var_id){
        var_t *v = vars->var + var_id;
        pddl_fdr_var_t *var = fdr_vars->var + var_id;
        var->var_id = var_id;

        // Compute number of values in the variable's domain
        var->val_size = borISetSize(&v->fact) + (v->none_of_those ? 1 : 0);
        var->val = BOR_CALLOC_ARR(pddl_fdr_val_t, var->val_size);

        // Set variable, value, and global ID of the values and set up
        // mapping from global ID to the variable value.
        for (int val_id = 0; val_id < var->val_size; ++val_id){
            pddl_fdr_val_t *val = var->val + val_id;
            pddlFDRValInit(val);
            val->var_id = var->var_id;
            val->val_id = val_id;
            val->global_id = global_id++;
            fdr_vars->global_id_to_val[val->global_id] = val;
        }

        // Set up value names from STRIPS fact names and the mapping from
        // STRIPS ID to the variable values
        for (int val_id = 0; val_id < borISetSize(&v->fact); ++val_id){
            int fact = borISetGet(&v->fact, val_id);
            pddl_fdr_val_t *val = var->val + val_id;
            if (strips->fact.fact[fact]->name != NULL)
                val->name = BOR_STRDUP(strips->fact.fact[fact]->name);
            val->strips_id = fact;
            borISetAdd(&fdr_vars->strips_id_to_val[fact], val->global_id);
        }

        // Set up "none-of-those" value
        var->val_none_of_those = -1;
        if (v->none_of_those){
            var->val_none_of_those = var->val_size - 1;
            pddl_fdr_val_t *val = var->val + var->val_none_of_those;
            val->name = BOR_STRDUP("none-of-those");
            val->strips_id = -1;
        }
    }
}

int pddlFDRVarsInitFromStrips(pddl_fdr_vars_t *fdr_vars,
                              const pddl_strips_t *strips,
                              const pddl_mgroups_t *mg,
                              const pddl_mutex_pairs_t *_mutex,
                              unsigned flags)
{
    vars_t vars;
    pddl_mutex_pairs_t mutex;

    pddlMutexPairsInitCopy(&mutex, _mutex);
    for (int fact_id = 0; fact_id < strips->fact.fact_size; ++fact_id){
        const pddl_fact_t *fact = strips->fact.fact[fact_id];
        if (fact->neg_of > fact_id)
            pddlMutexPairsAdd(&mutex, fact_id, fact->neg_of);
    }

    bzero(fdr_vars, sizeof(*fdr_vars));

    varsInit(&vars, mg);
    if (allocateVars(&vars, strips, mg, &mutex, flags) != 0){
        varsFree(&vars);
        pddlMutexPairsFree(&mutex);
        return -1;
    }

    createVars(fdr_vars, &vars, strips);
    varsFree(&vars);
    pddlMutexPairsFree(&mutex);

    return 0;
}

void pddlFDRVarsFree(pddl_fdr_vars_t *vars)
{
    if (vars->global_id_to_val != NULL)
        BOR_FREE(vars->global_id_to_val);
    for (int i = 0; i < vars->strips_id_size; ++i)
        borISetFree(vars->strips_id_to_val + i);
    if (vars->strips_id_to_val != NULL)
        BOR_FREE(vars->strips_id_to_val);
    for (int i = 0; i < vars->var_size; ++i)
        pddlFDRVarFree(vars->var + i);
    if (vars->var != NULL)
        BOR_FREE(vars->var);
}

static void pddlFDRValCopy(pddl_fdr_val_t *dst, const pddl_fdr_val_t *src)
{
    bzero(dst, sizeof(*dst));
    if (src->name != NULL)
        dst->name = BOR_STRDUP(src->name);
    dst->var_id = src->var_id;
    dst->val_id = src->val_id;
    dst->global_id = src->global_id;
    dst->strips_id = src->strips_id;
}

static void pddlFDRVarCopy(pddl_fdr_var_t *dst, const pddl_fdr_var_t *src)
{
    bzero(dst, sizeof(*dst));
    dst->var_id = src->var_id;
    dst->val_size = src->val_size;
    dst->val = BOR_CALLOC_ARR(pddl_fdr_val_t, dst->val_size);
    for (int i = 0; i < dst->val_size; ++i)
        pddlFDRValCopy(dst->val + i, src->val + i);
    dst->val_none_of_those = src->val_none_of_those;
}

void pddlFDRVarsInitCopy(pddl_fdr_vars_t *dst, const pddl_fdr_vars_t *src)
{
    bzero(dst, sizeof(*dst));
    dst->var_size = src->var_size;
    dst->var = BOR_CALLOC_ARR(pddl_fdr_var_t, dst->var_size);
    for (int i = 0; i < dst->var_size; ++i)
        pddlFDRVarCopy(dst->var + i, src->var + i);

    dst->global_id_size = src->global_id_size;
    if (dst->global_id_size > 0){
        dst->global_id_to_val = BOR_ALLOC_ARR(pddl_fdr_val_t *,
                                              dst->global_id_size);
        for (int i = 0; i < src->global_id_size; ++i){
            const pddl_fdr_val_t *sval = src->global_id_to_val[i];
            pddl_fdr_val_t *val = dst->var[sval->var_id].val + sval->val_id;
            dst->global_id_to_val[i] = val;
        }
    }

    dst->strips_id_size = src->strips_id_size;
    if (dst->strips_id_size > 0){
        dst->strips_id_to_val = BOR_ALLOC_ARR(bor_iset_t, dst->strips_id_size);
        for (int i = 0; i < src->strips_id_size; ++i)
            borISetUnion(&dst->strips_id_to_val[i], &src->strips_id_to_val[i]);
    }
}

void pddlFDRVarsPrintDebug(const pddl_fdr_vars_t *vars, FILE *fout)
{
    fprintf(fout, "Vars (%d):\n", vars->var_size);
    for (int vi = 0; vi < vars->var_size; ++vi){
        const pddl_fdr_var_t *var = vars->var + vi;
        fprintf(fout, "  Var %d:\n", var->var_id);
        for (int vali = 0; vali < var->val_size; ++vali){
            const pddl_fdr_val_t *val = var->val + vali;
            fprintf(fout, "    %d: %s (%d)\n", val->val_id, val->name,
                    val->global_id);
        }
    }
}
