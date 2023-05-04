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

#include "pddl/strips_fact_cross_ref.h"

void pddlStripsFactCrossRefInit(pddl_strips_fact_cross_ref_t *cref,
                                const pddl_strips_t *strips,
                                int init,
                                int goal,
                                int op_pre,
                                int op_add,
                                int op_del)
{
    if (strips->has_cond_eff){
        BOR_FATAL2("pddlStripsFactCrossRefInit() does not support"
                   " conditional effects!");
    }

    int fact;

    bzero(cref, sizeof(*cref));

    cref->fact_size = strips->fact.fact_size;
    cref->fact = BOR_CALLOC_ARR(pddl_strips_fact_cross_ref_fact_t,
                                cref->fact_size);

    if (init){
        BOR_ISET_FOR_EACH(&strips->init, fact)
            cref->fact[fact].is_init = 1;
    }

    if (goal){
        BOR_ISET_FOR_EACH(&strips->goal, fact)
            cref->fact[fact].is_goal = 1;
    }

    if (op_pre || op_add || op_del){
        for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
            const pddl_strips_op_t *op = strips->op.op[op_id];

            if (op_pre){
                BOR_ISET_FOR_EACH(&op->pre, fact)
                    borISetAdd(&cref->fact[fact].op_pre, op_id);
            }
            if (op_add){
                BOR_ISET_FOR_EACH(&op->add_eff, fact)
                    borISetAdd(&cref->fact[fact].op_add, op_id);
            }
            if (op_del){
                BOR_ISET_FOR_EACH(&op->del_eff, fact)
                    borISetAdd(&cref->fact[fact].op_del, op_id);
            }
        }
    }
}

void pddlStripsFactCrossRefFree(pddl_strips_fact_cross_ref_t *cref)
{
    for (int i = 0; i < cref->fact_size; ++i){
        borISetFree(&cref->fact[i].op_pre);
        borISetFree(&cref->fact[i].op_add);
        borISetFree(&cref->fact[i].op_del);
    }
    if (cref->fact != NULL)
        BOR_FREE(cref->fact);
}
