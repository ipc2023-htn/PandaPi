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

#include <boruvka/alloc.h>
#include "pddl/pddl.h"
#include "pddl/strips.h"
#include "helper.h"
#include "err.h"
#include "assert.h"

static void copyBasicInfo(pddl_strips_t *dst, const pddl_strips_t *src)
{
    if (src->domain_name)
        dst->domain_name = BOR_STRDUP(src->domain_name);
    if (src->problem_name)
        dst->problem_name = BOR_STRDUP(src->problem_name);
    if (src->domain_file)
        dst->domain_file = BOR_STRDUP(src->domain_file);
    if (src->problem_file)
        dst->problem_file = BOR_STRDUP(src->problem_file);
}

void pddlStripsInit(pddl_strips_t *strips)
{
    bzero(strips, sizeof(*strips));
    pddlFactsInit(&strips->fact);
    pddlStripsOpsInit(&strips->op);
    borISetInit(&strips->init);
    borISetInit(&strips->goal);
}

void pddlStripsMakeUnsolvable(pddl_strips_t *strips)
{
    // Remove all operators, empty the initial state and make sure that the
    // goal is non-empty.

    pddlStripsOpsFree(&strips->op);
    pddlStripsOpsInit(&strips->op);
    borISetEmpty(&strips->init);
    if (strips->fact.fact_size == 0){
        // TODO
        BOR_FATAL2("STRIPS problem does not contain any fact."
                   " Making unsolvable problem for this case is not yet"
                   " implemented.");
    }
    borISetEmpty(&strips->goal);
    borISetAdd(&strips->goal, 0);

    ASSERT_RUNTIME(strips->fact.fact_size > 0);
    for (int i = strips->fact.fact_size - 1; i >= 1; --i)
        pddlFactsDelFact(&strips->fact, i);
    strips->fact.fact_size = 1;
}

void pddlStripsFree(pddl_strips_t *strips)
{
    if (strips->domain_name)
        BOR_FREE(strips->domain_name);
    if (strips->problem_name)
        BOR_FREE(strips->problem_name);
    if (strips->domain_file)
        BOR_FREE(strips->domain_file);
    if (strips->problem_file)
        BOR_FREE(strips->problem_file);
    pddlFactsFree(&strips->fact);
    pddlStripsOpsFree(&strips->op);
    borISetFree(&strips->init);
    borISetFree(&strips->goal);
    bzero(strips, sizeof(*strips));
}

void pddlStripsInitCopy(pddl_strips_t *dst, const pddl_strips_t *src)
{
    pddlStripsInit(dst);
    copyBasicInfo(dst, src);
    dst->cfg = src->cfg;

    pddlFactsCopy(&dst->fact, &src->fact);
    pddlStripsOpsCopy(&dst->op, &src->op);
    borISetUnion(&dst->init, &src->init);
    borISetUnion(&dst->goal, &src->goal);
    dst->goal_is_unreachable = src->goal_is_unreachable;
    dst->has_cond_eff = src->has_cond_eff;
}

// TODO: Make public function
static int addNegFact(pddl_strips_t *strips, int fact_id)
{
    pddl_fact_t *fact = strips->fact.fact[fact_id];
    ASSERT_RUNTIME(fact->neg_of == -1);

    pddl_fact_t neg;
    char name[512];
    snprintf(name, 512, "NOT-%s", fact->name);
    pddlFactInit(&neg);
    neg.name = BOR_STRDUP(name);
    int neg_id = pddlFactsAdd(&strips->fact, &neg);
    pddlFactFree(&neg);

    pddl_fact_t *neg_fact = strips->fact.fact[neg_id];
    neg_fact->neg_of = fact_id;
    fact->neg_of = neg_id;
    neg_fact->is_private = fact->is_private;

    for (int opi = 0; opi < strips->op.op_size; ++opi){
        pddl_strips_op_t *op = strips->op.op[opi];
        if (borISetIn(fact_id, &op->del_eff))
            borISetAdd(&op->add_eff, neg_id);
        if (borISetIn(fact_id, &op->add_eff))
            borISetAdd(&op->del_eff, neg_id);
        for (int cei = 0; cei < op->cond_eff_size; ++cei){
            pddl_strips_op_cond_eff_t *ce = op->cond_eff + cei;
            if (borISetIn(fact_id, &ce->del_eff))
                borISetAdd(&ce->add_eff, neg_id);
            if (borISetIn(fact_id, &ce->add_eff))
                borISetAdd(&ce->del_eff, neg_id);
        }
    }

    if (!borISetIn(fact_id, &strips->init))
        borISetAdd(&strips->init, neg_id);

    return neg_id;
}

static void opCompileAwayCondEffNegPreRec(pddl_strips_t *strips,
                                          const pddl_strips_op_t *op_in,
                                          const bor_iset_t *neg_pre,
                                          int neg_pre_size,
                                          int cur_neg_pre)
{
    pddl_strips_op_t op;

    int neg_fact;
    BOR_ISET_FOR_EACH(neg_pre + cur_neg_pre, neg_fact){
        if (borISetIn(strips->fact.fact[neg_fact]->neg_of, &op_in->pre))
            continue;
        pddlStripsOpInit(&op);
        pddlStripsOpCopyWithoutCondEff(&op, op_in);
        borISetAdd(&op.pre, neg_fact);

        if (cur_neg_pre == neg_pre_size - 1){
            pddlStripsOpNormalize(&op);
            if (borISetSize(&op.add_eff) > 0)
                pddlStripsOpsAdd(&strips->op, &op);
        }else{
            opCompileAwayCondEffNegPreRec(strips, &op, neg_pre, neg_pre_size,
                                          cur_neg_pre + 1);
        }

        pddlStripsOpFree(&op);
    }
}

static void opCompileAwayCondEffNegPre(pddl_strips_t *strips,
                                       const pddl_strips_op_t *src_op,
                                       const pddl_strips_op_t *op,
                                       const int *neg_ce,
                                       int neg_ce_size)
{
    // Prepare negative preconditions
    bor_iset_t neg_pre[neg_ce_size];
    for (int i = 0; i < neg_ce_size; ++i)
        borISetInit(neg_pre + i);

    for (int i = 0; i < neg_ce_size; ++i){
        const pddl_strips_op_cond_eff_t *ce = src_op->cond_eff + neg_ce[i];
        int fact_id;
        BOR_ISET_FOR_EACH(&ce->pre, fact_id){
            int neg_fact_id = strips->fact.fact[fact_id]->neg_of;
            ASSERT_RUNTIME(neg_fact_id >= 0);
            borISetAdd(&neg_pre[i], neg_fact_id);
        }
    }

    opCompileAwayCondEffNegPreRec(strips, op, neg_pre, neg_ce_size, 0);

    for (int i = 0; i < neg_ce_size; ++i)
        borISetFree(neg_pre + i);
}

static void opCompileAwayCondEffComb(pddl_strips_t *strips,
                                     const pddl_strips_op_t *src_op,
                                     const int *neg_ce,
                                     int neg_ce_size,
                                     const int *pos_ce,
                                     int pos_ce_size)
{
    pddl_strips_op_t op;

    pddlStripsOpInit(&op);
    pddlStripsOpCopyWithoutCondEff(&op, src_op);

    // First merge in positive conditional effects
    for (int i = 0; i < pos_ce_size; ++i){
        borISetUnion(&op.pre, &src_op->cond_eff[pos_ce[i]].pre);
        borISetMinus(&op.add_eff, &src_op->cond_eff[pos_ce[i]].del_eff);
        borISetUnion(&op.del_eff, &src_op->cond_eff[pos_ce[i]].del_eff);
        borISetUnion(&op.add_eff, &src_op->cond_eff[pos_ce[i]].add_eff);
    }

    if (neg_ce_size > 0){
        // Then, recursivelly, set up negative preconditions
        opCompileAwayCondEffNegPre(strips, src_op, &op, neg_ce, neg_ce_size);
    }else{
        // Or add operator if there are no negative preconditions
        pddlStripsOpNormalize(&op);
        if (borISetSize(&op.add_eff) > 0)
            pddlStripsOpsAdd(&strips->op, &op);
    }

    pddlStripsOpFree(&op);
}

static void opCompileAwayCondEff(pddl_strips_t *strips,
                                 const pddl_strips_op_t *op)
{
    ASSERT_RUNTIME(op->cond_eff_size < sizeof(unsigned long) * 8);
    int neg_ce[op->cond_eff_size];
    int neg_ce_size;
    int pos_ce[op->cond_eff_size];
    int pos_ce_size;

    unsigned long max = 1ul << op->cond_eff_size;
    for (unsigned long comb = 0; comb < max; ++comb){
        unsigned long c = comb;
        neg_ce_size = pos_ce_size = 0;
        for (int i = 0; i < op->cond_eff_size; ++i){
            if (c & 0x1ul){
                pos_ce[pos_ce_size++] = i;
            }else{
                neg_ce[neg_ce_size++] = i;
            }
            c >>= 1ul;
        }
        opCompileAwayCondEffComb(strips, op, neg_ce, neg_ce_size,
                                 pos_ce, pos_ce_size);
    }
}

static void compileAwayCondEffCreateNegFacts(pddl_strips_t *strips)
{
    int fact_size = strips->fact.fact_size;

    for (int opi = 0; opi < strips->op.op_size; ++opi){
        const pddl_strips_op_t *op = strips->op.op[opi];
        for (int cei = 0; cei < op->cond_eff_size; ++cei){
            const pddl_strips_op_cond_eff_t *ce = op->cond_eff + cei;
            int fact_id;
            BOR_ISET_FOR_EACH(&ce->pre, fact_id){
                if (strips->fact.fact[fact_id]->neg_of == -1){
                    addNegFact(strips, fact_id);
                    ASSERT_RUNTIME(strips->fact.fact[fact_id]->neg_of != -1);
                }
            }
        }
    }

    if (strips->fact.fact_size != fact_size){
        // Sort facts if any was added
        int *fact_remap;
        fact_remap = BOR_CALLOC_ARR(int, strips->fact.fact_size);
        pddlFactsSort(&strips->fact, fact_remap);
        pddlISetRemap(&strips->init, fact_remap);
        pddlISetRemap(&strips->goal, fact_remap);
        pddlStripsOpsRemapFacts(&strips->op, fact_remap);
        BOR_FREE(fact_remap);
    }
}

void pddlStripsCompileAwayCondEff(pddl_strips_t *strips)
{
    if (!strips->has_cond_eff || strips->op.op_size == 0)
        return;

    // First create all negations that we might need
    compileAwayCondEffCreateNegFacts(strips);

    int op_size = strips->op.op_size;
    for (int opi = 0; opi < op_size; ++opi){
        const pddl_strips_op_t *op = strips->op.op[opi];
        if (op->cond_eff_size > 0)
            opCompileAwayCondEff(strips, op);
    }

    // Remove operators with conditional effects
    int *op_map;
    op_map = BOR_CALLOC_ARR(int, strips->op.op_size);
    for (int opi = 0; opi < strips->op.op_size; ++opi){
        if (strips->op.op[opi]->cond_eff_size > 0)
            op_map[opi] = 1;
    }
    pddlStripsOpsDelOps(&strips->op, op_map);
    BOR_FREE(op_map);

    // Remove duplicate operators
    pddlStripsOpsDeduplicate(&strips->op);
    // And sort operators to get deterministinc results.
    pddlStripsOpsSort(&strips->op);

    strips->has_cond_eff = 0;
}

void pddlStripsCrossRefFactsOps(const pddl_strips_t *strips,
                                void *_fact_arr,
                                unsigned long el_size,
                                long pre_offset,
                                long add_offset,
                                long del_offset)
{
    char *fact_arr = _fact_arr;
    for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
        const pddl_strips_op_t *op = strips->op.op[op_id];
        int fact_id;

        if (pre_offset >= 0){
            BOR_ISET_FOR_EACH(&op->pre, fact_id){
                char *el = fact_arr + (el_size * fact_id);
                bor_iset_t *s = (bor_iset_t *)(el + pre_offset);
                borISetAdd(s, op_id);
            }
        }

        if (add_offset >= 0){
            BOR_ISET_FOR_EACH(&op->add_eff, fact_id){
                char *el = fact_arr + (el_size * fact_id);
                bor_iset_t *s = (bor_iset_t *)(el + add_offset);
                borISetAdd(s, op_id);
            }
        }

        if (del_offset >= 0){
            BOR_ISET_FOR_EACH(&op->del_eff, fact_id){
                char *el = fact_arr + (el_size * fact_id);
                bor_iset_t *s = (bor_iset_t *)(el + del_offset);
                borISetAdd(s, op_id);
            }
        }
    }
}

void pddlStripsApplicableOps(const pddl_strips_t *strips,
                             const bor_iset_t *state,
                             bor_iset_t *app_ops)
{
    for (int i = 0; i < strips->op.op_size; ++i){
        const pddl_strips_op_t *op = strips->op.op[i];
        if (borISetIsSubset(&op->pre, state))
            borISetAdd(app_ops, i);
    }
}

static int isFAMGroup(const bor_iset_t *facts,
                      const bor_iset_t *pre,
                      const bor_iset_t *add_eff,
                      const bor_iset_t *del_eff)
{
    BOR_ISET(predel);

    borISetIntersect2(&predel, pre, del_eff);
    int add_size = borISetIntersectionSize(add_eff, facts);
    if (add_size > borISetIntersectionSize(&predel, facts)){
        borISetFree(&predel);
        return 0;
    }

    borISetFree(&predel);
    return 1;
}

static int condEffAreDisjunct(const pddl_strips_op_t *op,
                              const bor_iset_t *facts)        
{
    BOR_ISET(del_eff);
    BOR_ISET(del_eff2);
    BOR_ISET(add_eff);
    BOR_ISET(add_eff2);

    for (int cei = 0; cei < op->cond_eff_size; ++cei){
        borISetIntersect2(&del_eff, &op->cond_eff[cei].del_eff, facts);
        borISetIntersect2(&add_eff, &op->cond_eff[cei].add_eff, facts);

        for (int cei2 = cei + 1; cei2 < op->cond_eff_size; ++cei2){
            borISetIntersect2(&del_eff2, &op->cond_eff[cei2].del_eff, facts);
            borISetIntersect2(&add_eff2, &op->cond_eff[cei2].add_eff, facts);
            if (!borISetIsDisjunct(&del_eff, &del_eff2)
                    || !borISetIsDisjunct(&add_eff, &add_eff2)){
                borISetFree(&del_eff);
                borISetFree(&del_eff2);
                borISetFree(&add_eff);
                borISetFree(&add_eff2);
                return 0;
            }
        }
    }

    borISetFree(&del_eff);
    borISetFree(&del_eff2);
    borISetFree(&add_eff);
    borISetFree(&add_eff2);

    return 1;
}

static int isFAMGroupCERec(const pddl_strips_t *strips,
                           const bor_iset_t *facts,
                           const pddl_strips_op_t *op,
                           int cond_eff_i,
                           const bor_iset_t *pre_in,
                           const bor_iset_t *add_eff_in,
                           const bor_iset_t *del_eff_in)
{
    BOR_ISET(pre);
    BOR_ISET(add_eff);
    BOR_ISET(del_eff);

    for (int cei = cond_eff_i; cei < op->cond_eff_size; ++cei){
        borISetUnion2(&pre, pre_in, &op->cond_eff[cei].pre);
        borISetUnion2(&add_eff, add_eff_in, &op->cond_eff[cei].add_eff);
        borISetUnion2(&del_eff, del_eff_in, &op->cond_eff[cei].del_eff);
        if (!isFAMGroup(facts, &pre, &add_eff, &del_eff)){
            borISetFree(&pre);
            borISetFree(&add_eff);
            borISetFree(&del_eff);
            return 0;
        }

        if (cond_eff_i + 1 < op->cond_eff_size){
            if (!isFAMGroupCERec(strips, facts, op, cond_eff_i + 1,
                                 &pre, &add_eff, &del_eff)){
                borISetFree(&pre);
                borISetFree(&add_eff);
                borISetFree(&del_eff);
                return 0;
            }
        }
    }

    borISetFree(&pre);
    borISetFree(&add_eff);
    borISetFree(&del_eff);
    return 1;
}

static int isFAMGroupCE(const pddl_strips_t *strips,
                        const bor_iset_t *facts,
                        const pddl_strips_op_t *op)
{
    if (condEffAreDisjunct(op, facts)){
        BOR_ISET(pre);
        BOR_ISET(add_eff);
        BOR_ISET(del_eff);

        for (int cei = 0; cei < op->cond_eff_size; ++cei){
            borISetUnion2(&pre, &op->pre, &op->cond_eff[cei].pre);
            borISetUnion2(&add_eff, &op->add_eff, &op->cond_eff[cei].add_eff);
            borISetUnion2(&del_eff, &op->del_eff, &op->cond_eff[cei].del_eff);
            if (!isFAMGroup(facts, &pre, &add_eff, &del_eff)){
                borISetFree(&pre);
                borISetFree(&add_eff);
                borISetFree(&del_eff);
                return 0;
            }
        }

        borISetFree(&pre);
        borISetFree(&add_eff);
        borISetFree(&del_eff);
        return 1;

    }else{
        return isFAMGroupCERec(strips, facts, op, 0,
                               &op->pre, &op->add_eff, &op->del_eff);
    }
}

int pddlStripsIsFAMGroup(const pddl_strips_t *strips, const bor_iset_t *facts)
{
    for (int oi = 0; oi < strips->op.op_size; ++oi){
        const pddl_strips_op_t *op = strips->op.op[oi];
        if (!isFAMGroup(facts, &op->pre, &op->add_eff, &op->del_eff))
            return 0;

        if (op->cond_eff_size > 0 && !isFAMGroupCE(strips, facts, op))
            return 0;
    }

    return 1;
}

static void resetHasCondEffFlag(pddl_strips_t *strips)
{
    int has_cond_eff = 0;
    for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
        if (strips->op.op[op_id]->cond_eff_size > 0){
            has_cond_eff = 1;
            break;
        }
    }
    strips->has_cond_eff = has_cond_eff;
}

int pddlStripsMergeCondEffIfPossible(pddl_strips_t *strips)
{
    if (!strips->has_cond_eff)
        return 0;

    int change = 0;
    for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
        pddl_strips_op_t *op = strips->op.op[op_id];
        if (op->cond_eff_size == 0)
            continue;
        if (borISetSize(&op->add_eff) > 0 || borISetSize(&op->del_eff) > 0)
            continue;

        int can_flatten = 1;
        for (int cei = 1; cei < op->cond_eff_size; ++cei){
            if (!borISetEq(&op->cond_eff[cei - 1].pre, &op->cond_eff[cei].pre)){
                can_flatten = 0;
                break;
            }
        }

        if (can_flatten){
            borISetUnion(&op->pre, &op->cond_eff[0].pre);
            for (int cei = 0; cei < op->cond_eff_size; ++cei){
                borISetUnion(&op->del_eff, &op->cond_eff[cei].del_eff);
                borISetUnion(&op->add_eff, &op->cond_eff[cei].add_eff);
            }

            pddlStripsOpFreeAllCondEffs(op);
            pddlStripsOpNormalize(op);
            change = 1;
        }
    }

    if (change){
        resetHasCondEffFlag(strips);
        return 1;
    }
    return 0;
}

void pddlStripsReduce(pddl_strips_t *strips,
                      const bor_iset_t *del_facts,
                      const bor_iset_t *del_ops)
{
    if (del_ops != NULL && borISetSize(del_ops) > 0)
        pddlStripsOpsDelOpsSet(&strips->op, del_ops);

    if (del_facts != NULL && borISetSize(del_facts) > 0){
        pddlStripsOpsRemoveFacts(&strips->op, del_facts);

        int *remap_fact = BOR_CALLOC_ARR(int, strips->fact.fact_size);
        pddlFactsDelFacts(&strips->fact, del_facts, remap_fact);
        pddlStripsOpsRemapFacts(&strips->op, remap_fact);

        borISetMinus(&strips->init, del_facts);
        borISetRemap(&strips->init, remap_fact);
        borISetMinus(&strips->goal, del_facts);
        borISetRemap(&strips->goal, remap_fact);

        if (remap_fact != NULL)
            BOR_FREE(remap_fact);

        BOR_ISET(useless_ops);
        for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
            const pddl_strips_op_t *op = strips->op.op[op_id];
            if (borISetSize(&op->add_eff) == 0
                    && borISetSize(&op->del_eff) == 0
                    && op->cond_eff_size == 0){
                borISetAdd(&useless_ops, op_id);
            }
        }
        if (borISetSize(&useless_ops) > 0)
            pddlStripsOpsDelOpsSet(&strips->op, &useless_ops);
        borISetFree(&useless_ops);

        if (strips->has_cond_eff)
            resetHasCondEffFlag(strips);
    }
}

int pddlStripsRemoveStaticFacts(pddl_strips_t *strips, bor_err_t *err)
{
    int num = 0;
    int *nonstatic_facts = BOR_CALLOC_ARR(int, strips->fact.fact_size);

    int fact;
    BOR_ISET_FOR_EACH(&strips->init, fact)
        nonstatic_facts[fact] = -1;

    for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
        const pddl_strips_op_t *op = strips->op.op[op_id];
        int fact;
        BOR_ISET_FOR_EACH(&op->add_eff, fact){
            if (nonstatic_facts[fact] == 0)
                nonstatic_facts[fact] = 1;
        }
        BOR_ISET_FOR_EACH(&op->del_eff, fact)
            nonstatic_facts[fact] = 1;
        for (int ce_id = 0; ce_id < op->cond_eff_size; ++ce_id){
            const pddl_strips_op_cond_eff_t *ce = op->cond_eff + ce_id;
            BOR_ISET_FOR_EACH(&ce->add_eff, fact){
                if (nonstatic_facts[fact] == 0)
                    nonstatic_facts[fact] = 1;
            }
            BOR_ISET_FOR_EACH(&ce->del_eff, fact)
                nonstatic_facts[fact] = 1;
        }
    }

    BOR_ISET(del_facts);
    for (int fact_id = 0; fact_id < strips->fact.fact_size; ++fact_id){
        if (nonstatic_facts[fact_id] <= 0){
            borISetAdd(&del_facts, fact_id);
            ++num;
        }
    }

    BOR_INFO(err, "Found %d static facts", borISetSize(&del_facts));
    if (borISetSize(&del_facts) > 0){
        pddlStripsReduce(strips, &del_facts, NULL);
        BOR_INFO(err, "Removed %d static facts", borISetSize(&del_facts));
    }

    borISetFree(&del_facts);
    if (nonstatic_facts != NULL)
        BOR_FREE(nonstatic_facts);

    return num;
}

static int isDelEffMutex(const pddl_mutex_pairs_t *mutex,
                         const bor_iset_t *pre,
                         const bor_iset_t *pre2,
                         int fact_id)
{
    if (pddlMutexPairsIsMutexFactSet(mutex, fact_id, pre))
        return 1;
    if (pre2 != NULL && pddlMutexPairsIsMutexFactSet(mutex, fact_id, pre2))
        return 1;
    return 0;
}

static void findUselessDelEffs(const pddl_strips_t *strips,
                               const pddl_mutex_pairs_t *mutex,
                               const bor_iset_t *pre,
                               const bor_iset_t *pre2,
                               const bor_iset_t *del_eff,
                               bor_iset_t *useless)
{
    int del_fact;
    BOR_ISET_FOR_EACH(del_eff, del_fact){
        if (mutex != NULL && isDelEffMutex(mutex, pre, pre2, del_fact)){
            borISetAdd(useless, del_fact);

        }else if (strips->fact.fact[del_fact]->neg_of >= 0){
            int neg = strips->fact.fact[del_fact]->neg_of;
            int pre_fact;
            BOR_ISET_FOR_EACH(pre, pre_fact){
                if (pre_fact == neg){
                    borISetAdd(useless, del_fact);
                    break;
                }
            }
            if (pre2 != NULL){
                BOR_ISET_FOR_EACH(pre2, pre_fact){
                    if (pre_fact == neg){
                        borISetAdd(useless, del_fact);
                        break;
                    }
                }
            }
        }
    }
}

int pddlStripsRemoveUselessDelEffs(pddl_strips_t *strips,
                                   const pddl_mutex_pairs_t *mutex,
                                   bor_iset_t *changed_ops,
                                   bor_err_t *err)
{
    int ret = 0;
    BOR_INFO(err, "Removing useless delete effects. num mutex pairs: %d",
             (mutex != NULL ? mutex->num_mutex_pairs : -1 ));

    BOR_ISET(useless);
    for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
        pddl_strips_op_t *op = strips->op.op[op_id];

        int changed = 0;
        borISetEmpty(&useless);
        findUselessDelEffs(strips, mutex, &op->pre, NULL,
                           &op->del_eff, &useless);
        if (borISetSize(&useless) > 0){
            borISetMinus(&op->del_eff, &useless);
            changed = 1;
            if (changed_ops != NULL)
                borISetAdd(changed_ops, op_id);
        }

        for (int cei = 0; cei < op->cond_eff_size; ++cei){
            pddl_strips_op_cond_eff_t *ce = op->cond_eff + cei;
            borISetEmpty(&useless);
            findUselessDelEffs(strips, mutex, &op->pre, &ce->pre,
                               &ce->del_eff, &useless);
            if (borISetSize(&useless) > 0){
                borISetMinus(&ce->del_eff, &useless);
                changed = 1;
                if (changed_ops != NULL)
                    borISetAdd(changed_ops, op_id);
            }
        }

        if (changed)
            ++ret;
    }
    borISetFree(&useless);

    BOR_INFO(err, "Removing useless delete effects DONE."
                  " (modified ops: %d)", ret);
    return ret;
}

static void printPythonISet(const bor_iset_t *s, FILE *fout)
{
    int i;
    fprintf(fout, "set([");
    BOR_ISET_FOR_EACH(s, i)
        fprintf(fout, " %d,", i);
    fprintf(fout, "])");
}

void pddlStripsPrintPython(const pddl_strips_t *strips, FILE *fout)
{
    int f;

    fprintf(fout, "{\n");
    fprintf(fout, "'domain_file' : '%s',\n", strips->domain_file);
    fprintf(fout, "'problem_file' : '%s',\n", strips->problem_file);
    fprintf(fout, "'domain_name' : '%s',\n", strips->domain_name);
    fprintf(fout, "'problem_name' : '%s',\n", strips->problem_name);

    fprintf(fout, "'fact' : [\n");
    for (int i = 0; i < strips->fact.fact_size; ++i)
        fprintf(fout, "    '(%s)',\n", strips->fact.fact[i]->name);
    fprintf(fout, "],\n");

    fprintf(fout, "'op' : [\n");
    for (int i = 0; i < strips->op.op_size; ++i){
        const pddl_strips_op_t *op = strips->op.op[i];
        fprintf(fout, "    {\n");
        fprintf(fout, "        'name' : '%s',\n", op->name);
        fprintf(fout, "        'cost' : '%d',\n", op->cost);

        fprintf(fout, "        'pre' : ");
        printPythonISet(&op->pre, fout);
        fprintf(fout, ",\n");
        fprintf(fout, "        'add' : ");
        printPythonISet(&op->add_eff, fout);
        fprintf(fout, ",\n");
        fprintf(fout, "        'del' : ");
        printPythonISet(&op->del_eff, fout);
        fprintf(fout, ",\n");

        fprintf(fout, "        'cond_eff' : [\n");
        for (int j = 0; j < op->cond_eff_size; ++j){
            const pddl_strips_op_cond_eff_t *ce = op->cond_eff + j;
            fprintf(fout, "            {\n");
            fprintf(fout, "                'pre' : ");
            printPythonISet(&ce->pre, fout);
            fprintf(fout, ",\n");
            fprintf(fout, "                'add' : ");
            printPythonISet(&ce->add_eff, fout);
            fprintf(fout, ",\n");
            fprintf(fout, "                'del' : ");
            printPythonISet(&ce->del_eff, fout);
            fprintf(fout, ",\n");
            fprintf(fout, "            },\n");
        }
        fprintf(fout, "        ]\n");

        fprintf(fout, "    },\n");
    }
    fprintf(fout, "],\n");

    fprintf(fout, "'init' : [");
    BOR_ISET_FOR_EACH(&strips->init, f)
        fprintf(fout, "%d, ", f);
    fprintf(fout, "],\n");

    fprintf(fout, "'goal' : [");
    BOR_ISET_FOR_EACH(&strips->goal, f)
        fprintf(fout, "%d, ", f);
    fprintf(fout, "],\n");

    fprintf(fout, "'goal_is_unreachable' : %s,\n",
            (strips->goal_is_unreachable ? "True" : "False" ));
    fprintf(fout, "'has_cond_eff' : %s,\n",
            (strips->has_cond_eff ? "True" : "False" ));
    fprintf(fout, "}\n");
}

void pddlStripsPrintPDDLDomain(const pddl_strips_t *strips, FILE *fout)
{
    int fact_id;

    fprintf(fout, "(define (domain %s)\n", strips->domain_name);

    fprintf(fout, "(:predicates\n");
    for (int i = 0; i < strips->fact.fact_size; ++i)
        fprintf(fout, "    (F%d) ;; %s\n", i, strips->fact.fact[i]->name);
    fprintf(fout, ")\n");
    fprintf(fout, "(:functions (total-cost))\n");

    for (int i = 0; i < strips->op.op_size; ++i){
        const pddl_strips_op_t *op = strips->op.op[i];
        char *name = BOR_STRDUP(op->name);
        for (char *c = name; *c != 0x0; ++c){
            if (*c == ' ' || *c == '(' || *c == ')')
                *c = '_';
        }
        fprintf(fout, "(:action %s\n", name);
        fprintf(fout, "    :precondition (and");
        BOR_ISET_FOR_EACH(&op->pre, fact_id)
            fprintf(fout, " (F%d)", fact_id);
        fprintf(fout, ")\n");

        fprintf(fout, "    :effect (and");
        BOR_ISET_FOR_EACH(&op->add_eff, fact_id)
            fprintf(fout, " (F%d)", fact_id);
        BOR_ISET_FOR_EACH(&op->del_eff, fact_id)
            fprintf(fout, " (not (F%d))", fact_id);
        for (int cei = 0; cei < op->cond_eff_size; ++cei){
            const pddl_strips_op_cond_eff_t *ce = op->cond_eff + cei;
            fprintf(fout, " (when (and");
            BOR_ISET_FOR_EACH(&ce->pre, fact_id)
                fprintf(fout, " (F%d)", fact_id);
            fprintf(fout, ") (and");
            BOR_ISET_FOR_EACH(&ce->add_eff, fact_id)
                fprintf(fout, " (F%d)", fact_id);
            BOR_ISET_FOR_EACH(&ce->del_eff, fact_id)
                fprintf(fout, " (not (F%d))", fact_id);
            fprintf(fout, ")");
        }

        fprintf(fout, " (increase (total-cost) %d)", op->cost);
        fprintf(fout, ")\n");

        fprintf(fout, ")\n");
        BOR_FREE(name);
    }

    fprintf(fout, ")\n");
}

void pddlStripsPrintPDDLProblem(const pddl_strips_t *strips, FILE *fout)
{
    int fact_id;

    fprintf(fout, "(define (problem %s) (:domain %s)\n",
            strips->problem_name, strips->domain_name);

    fprintf(fout, "(:init\n");
    BOR_ISET_FOR_EACH(&strips->init, fact_id)
        fprintf(fout, "    (F%d)\n", fact_id);
    fprintf(fout, ")\n");

    fprintf(fout, "(:goal (and");
    BOR_ISET_FOR_EACH(&strips->goal, fact_id)
        fprintf(fout, " (F%d)", fact_id);
    fprintf(fout, "))\n");
    fprintf(fout, "(:metric minimize (total-cost))\n");
    fprintf(fout, ")\n");
}

void pddlStripsPrintDebug(const pddl_strips_t *strips, FILE *fout)
{
    fprintf(fout, "Fact[%d]:\n", strips->fact.fact_size);
    pddlFactsPrint(&strips->fact, "  ", "\n", fout);

    fprintf(fout, "Op[%d]:\n", strips->op.op_size);
    pddlStripsOpsPrintDebug(&strips->op, &strips->fact, fout);

    fprintf(fout, "Init State:");
    pddlFactsPrintSet(&strips->init, &strips->fact, " ", "", fout);
    fprintf(fout, "\n");

    fprintf(fout, "Goal:");
    pddlFactsPrintSet(&strips->goal, &strips->fact, " ", "", fout);
    fprintf(fout, "\n");
    if (strips->goal_is_unreachable)
        fprintf(fout, "Goal is unreachable\n");
    if (strips->has_cond_eff)
        fprintf(fout, "Has conditional effects\n");
}
