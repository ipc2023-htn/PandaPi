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

#include "pddl/irrelevance.h"
#include "pddl/strips_fact_cross_ref.h"
#include "assert.h"

static void backwardIrrelevanceEnqueue(const pddl_strips_t *s,
                                       int op_id,
                                       int *fact_irr,
                                       int *op_irr,
                                       int *queue,
                                       int *queue_size)
{
    op_irr[op_id] = -1;

    const pddl_strips_op_t *op = s->op.op[op_id];
    int next;
    BOR_ISET_FOR_EACH(&op->pre, next){
        if (fact_irr[next] == 0){
            fact_irr[next] = -1;
            queue[(*queue_size)++] = next;
        }
    }
}

static void backwardIrrelevance(const pddl_strips_t *s,
                                const pddl_strips_fact_cross_ref_t *cref,
                                int *fact_irr,
                                int *op_irr)
{
    int queue_size, *queue;

    queue = BOR_CALLOC_ARR(int, s->fact.fact_size);

    // Initialize queue with the goal
    queue_size = 0;
    int fact_id;
    BOR_ISET_FOR_EACH(&s->goal, fact_id){
        if (fact_irr[fact_id] == 0){
            queue[queue_size++] = fact_id;
            fact_irr[fact_id] = -1;
        }
    }

    while (queue_size > 0){
        fact_id = queue[--queue_size];
        const pddl_strips_fact_cross_ref_fact_t *f = cref->fact + fact_id;
        int op_id;
        BOR_ISET_FOR_EACH(&f->op_add, op_id){
            if (op_irr[op_id] == 0){
                backwardIrrelevanceEnqueue(s, op_id, fact_irr, op_irr,
                                           queue, &queue_size);
            }
        }

        // TODO: Allow for enable/disable
        //       This may or may not be desired, because the operator
        //       without add effects
        //       cannot be part of the strictly optimal plan, but it may
        //       reduce branching for the satisficing planning.
        BOR_ISET_FOR_EACH(&f->op_del, op_id){
            if (op_irr[op_id] == 0){
                backwardIrrelevanceEnqueue(s, op_id, fact_irr, op_irr,
                                           queue, &queue_size);
            }
        }
    }

    BOR_FREE(queue);
}

int pddlIrrelevanceAnalysis(const pddl_strips_t *strips,
                            bor_iset_t *irrelevant_facts,
                            bor_iset_t *irrelevant_ops,
                            bor_iset_t *static_facts,
                            bor_err_t *err)
{
    pddl_strips_fact_cross_ref_t cref;
    int *fact_irr, *op_irr;

    if (strips->has_cond_eff){
        BOR_ERR_RET2(err, -1, "Irrelevance analysis does not support"
                              " conditional effects.");
    }

    BOR_INFO(err, "Irrelevance analysis. facts: %d, ops: %d",
             strips->fact.fact_size, strips->op.op_size);

    pddlStripsFactCrossRefInit(&cref, strips, 1, 1, 0, 1, 1);

    fact_irr = BOR_CALLOC_ARR(int, strips->fact.fact_size);
    op_irr = BOR_CALLOC_ARR(int, strips->op.op_size);

    if (irrelevant_ops != NULL && borISetSize(irrelevant_ops) > 0){
        int op_id;
        BOR_ISET_FOR_EACH(irrelevant_ops, op_id)
            op_irr[op_id] = 1;
    }

    if (irrelevant_facts != NULL && borISetSize(irrelevant_facts) > 0){
        int fact_id;
        BOR_ISET_FOR_EACH(irrelevant_facts, fact_id)
            fact_irr[fact_id] = 1;
    }

    /* Detect static facts:
     * Although during grounding the facts created from the static
     * predicates were already removed, there still can be facts that are
     * static individually, i.e., they appear only in the preconditions of
     * operators -- they are not manipulated in any way --, but their
     * predicates are not static, because they appear in effects.
     * If such a static fact appears in the initial state, then it is true
     * troughout the whole state space, therefore it can be safely removed
     * from all operators, init and goal.
     */
    for (int fact_id = 0; fact_id < strips->fact.fact_size; ++fact_id){
        if (borISetSize(&cref.fact[fact_id].op_add) == 0
                && borISetSize(&cref.fact[fact_id].op_del) == 0
                && cref.fact[fact_id].is_init){
            fact_irr[fact_id] = 1;
            if (static_facts != NULL)
                borISetAdd(static_facts, fact_id);
        }
    }

    backwardIrrelevance(strips, &cref, fact_irr, op_irr);

    if (irrelevant_facts != NULL){
        for (int fact_id = 0; fact_id < strips->fact.fact_size; ++fact_id){
            if (fact_irr[fact_id] >= 0)
                borISetAdd(irrelevant_facts, fact_id);
        }
    }

    if (irrelevant_ops != NULL){
        for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
            if (op_irr[op_id] >= 0)
                borISetAdd(irrelevant_ops, op_id);
        }
    }

    if (fact_irr != NULL)
        BOR_FREE(fact_irr);
    if (op_irr != NULL)
        BOR_FREE(op_irr);
    pddlStripsFactCrossRefFree(&cref);

    BOR_INFO(err, "Irrelevance analysis DONE: irrelevant facts: %d,"
                  " irrelevant ops: %d, static facts: %d",
             (irrelevant_facts != NULL ? borISetSize(irrelevant_facts) : -1),
             (irrelevant_ops != NULL ? borISetSize(irrelevant_ops) : -1),
             (static_facts != NULL ? borISetSize(static_facts) : -1));

    return 0;
}
