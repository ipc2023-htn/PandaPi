/***
 * cpddl
 * -------
 * Copyright (c)2019 Daniel Fiser <danfis@danfis.cz>,
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

#include <boruvka/sort.h>
#include "pddl/fdr.h"
#include "assert.h"

static void stripsToFDRState(const pddl_fdr_vars_t *fdr_var,
                             const bor_iset_t *state,
                             int *fdr_state)
{
    for (int vi = 0; vi < fdr_var->var_size; ++vi)
        fdr_state[vi] = -1;

    int fact_id;
    BOR_ISET_FOR_EACH(state, fact_id){
        int val_id;
        BOR_ISET_FOR_EACH(&fdr_var->strips_id_to_val[fact_id], val_id){
            const pddl_fdr_val_t *v = fdr_var->global_id_to_val[val_id];
            fdr_state[v->var_id] = v->val_id;
        }
    }

    for (int vi = 0; vi < fdr_var->var_size; ++vi){
        if (fdr_state[vi] == -1){
            ASSERT(fdr_var->var[vi].val_none_of_those >= 0);
            fdr_state[vi] = fdr_var->var[vi].val_none_of_those;
        }
    }
}


static int setPre(const pddl_fdr_vars_t *fdr_var,
                  const bor_iset_t *strips_pre,
                  int *pre)
{
    for (int vi = 0; vi < fdr_var->var_size; ++vi)
        pre[vi] = -1;

    int fact_id;
    BOR_ISET_FOR_EACH(strips_pre, fact_id){
        int val_id;
        BOR_ISET_FOR_EACH(&fdr_var->strips_id_to_val[fact_id], val_id){
            const pddl_fdr_val_t *v = fdr_var->global_id_to_val[val_id];
            ASSERT(v != NULL);
            if (pre[v->var_id] != -1)
                return -1;
            pre[v->var_id] = v->val_id;
        }
    }

    return 0;
}

static void setDelEff(const pddl_mutex_pairs_t *mutex,
                      const pddl_fdr_vars_t *fdr_var,
                      const bor_iset_t *pre,
                      const bor_iset_t *ce_pre,
                      int *eff,
                      int fact_id,
                      const pddl_fdr_val_t *v)
{
    const pddl_fdr_var_t *var = fdr_var->var + v->var_id;
    if (!pddlMutexPairsIsMutexFactSet(mutex, fact_id, pre)
            && (ce_pre == NULL
                    || !pddlMutexPairsIsMutexFactSet(mutex, fact_id, ce_pre))
            && var->val_none_of_those >= 0){
        eff[var->var_id] = var->val_none_of_those;
    }
}

static void setEff(const pddl_mutex_pairs_t *mutex,
                   const pddl_fdr_vars_t *fdr_var,
                   const bor_iset_t *pre,
                   const bor_iset_t *ce_pre,
                   const bor_iset_t *add_eff,
                   const bor_iset_t *del_eff,
                   int *eff)
{
    int fact_id;

    for (int vi = 0; vi < fdr_var->var_size; ++vi)
        eff[vi] = -1;

    BOR_ISET_FOR_EACH(del_eff, fact_id){
        int val_id;
        BOR_ISET_FOR_EACH(&fdr_var->strips_id_to_val[fact_id], val_id){
            const pddl_fdr_val_t *v = fdr_var->global_id_to_val[val_id];
            setDelEff(mutex, fdr_var, pre, ce_pre, eff, fact_id, v);
        }
    }

    BOR_ISET_FOR_EACH(add_eff, fact_id){
        int val_id;
        BOR_ISET_FOR_EACH(&fdr_var->strips_id_to_val[fact_id], val_id){
            const pddl_fdr_val_t *v = fdr_var->global_id_to_val[val_id];
            eff[v->var_id] = v->val_id;
        }
    }
}

struct eff {
    bor_iset_t ce_pre;
    int var;
    int pre;
    int eff;
};
typedef struct eff eff_t;

struct effs {
    eff_t *eff;
    int eff_size;
    int eff_alloc;
};
typedef struct effs effs_t;

static void initEffs(effs_t *effs)
{
    bzero(effs, sizeof(*effs));
}

static void freeEffs(effs_t *effs)
{
    for (int i = 0; i < effs->eff_size; ++i){
        eff_t *e = effs->eff + i;
        borISetFree(&e->ce_pre);
    }
    if (effs->eff != NULL)
        BOR_FREE(effs->eff);
}

static void addEff(effs_t *effs, int var, int pre, int eff,
                   const bor_iset_t *ce_pre)
{
    if (effs->eff_size == effs->eff_alloc){
        if (effs->eff_alloc == 0)
            effs->eff_alloc = 1;
        effs->eff_alloc *= 2;
        effs->eff = BOR_REALLOC_ARR(effs->eff, eff_t, effs->eff_alloc);
    }

    eff_t *e = effs->eff + effs->eff_size++;
    bzero(e, sizeof(*e));
    e->var = var;
    e->pre = e->eff = -1;

    if (pre >= 0)
        e->pre = pre;
    e->eff = eff;

    if (ce_pre != NULL)
        borISetUnion(&e->ce_pre, ce_pre);
}

static int effsCmp(const void *a, const void *b, void *_)
{
    const eff_t *e1 = a;
    const eff_t *e2 = b;
    int cmp = borISetSize(&e1->ce_pre) - borISetSize(&e2->ce_pre);
    if (cmp == 0)
        cmp = borISetCmp(&e1->ce_pre, &e2->ce_pre);
    if (cmp == 0)
        cmp = e1->var - e2->var;
    if (cmp == 0)
        cmp = e1->eff - e2->eff;
    return cmp;
}

static void sortEffs(effs_t *effs)
{
    borSort(effs->eff, effs->eff_size, sizeof(eff_t),
            effsCmp, NULL);
}

static int numOps(const pddl_fdr_vars_t *fdr_var,
                  const pddl_strips_ops_t *ops)
{
    int num_ops = 0;
    int *pre = BOR_ALLOC_ARR(int, fdr_var->var_size);

    for (int op_id = 0; op_id < ops->op_size; ++op_id){
        const pddl_strips_op_t *op = ops->op[op_id];
        if (setPre(fdr_var, &op->pre, pre) == 0)
            ++num_ops;
    }

    if (pre != NULL)
        BOR_FREE(pre);

    return num_ops;
}

static void opToFDR(const pddl_strips_t *strips,
                    const pddl_mutex_pairs_t *mutex,
                    const pddl_fdr_vars_t *fdr_var,
                    const pddl_strips_op_t *op,
                    FILE *fout)
{
    int *pre = BOR_ALLOC_ARR(int, fdr_var->var_size);
    int *pre_test = BOR_ALLOC_ARR(int, fdr_var->var_size);
    int *pre_var_eff = BOR_CALLOC_ARR(int, fdr_var->var_size);
    int *eff = BOR_ALLOC_ARR(int, fdr_var->var_size);
    effs_t effs;
    initEffs(&effs);

    if (setPre(fdr_var, &op->pre, pre) != 0)
        return;

    setEff(mutex, fdr_var, &op->pre, NULL, &op->add_eff, &op->del_eff, eff);
    for (int vi = 0; vi < fdr_var->var_size; ++vi){
        if (eff[vi] < 0)
            continue;
        addEff(&effs, vi, pre[vi], eff[vi], NULL);
        if (pre[vi] >= 0)
            pre_var_eff[vi] = 1;
    }

    for (int cei = 0; cei < op->cond_eff_size; ++cei){
        const pddl_strips_op_cond_eff_t *ce = op->cond_eff + cei;
        setEff(mutex, fdr_var, &op->pre,
               &ce->pre, &ce->add_eff, &ce->del_eff, eff);
        for (int vi = 0; vi < fdr_var->var_size; ++vi){
            if (eff[vi] < 0)
                continue;
            if (setPre(fdr_var, &ce->pre, pre_test) != 0)
                continue;
            addEff(&effs, vi, pre[vi], eff[vi], &ce->pre);
            if (pre[vi] >= 0)
                pre_var_eff[vi] = 1;
        }
    }

    sortEffs(&effs);

    fprintf(fout, "begin_operator\n");
    fprintf(fout, "%s\n", op->name);

    int num_prevails = 0;
    for (int vi = 0; vi < fdr_var->var_size; ++vi){
        if (pre[vi] >= 0 && !pre_var_eff[vi])
            ++num_prevails;
    }
    fprintf(fout, "%d\n", num_prevails);
    for (int vi = 0; vi < fdr_var->var_size; ++vi){
        if (pre[vi] >= 0 && !pre_var_eff[vi])
            fprintf(fout, "%d %d\n", vi, pre[vi]);
    }

    fprintf(fout, "%d\n", effs.eff_size);
    for (int effi = 0; effi < effs.eff_size; ++effi){
        eff_t *e = effs.eff + effi;
        int ce_size = 0;
        int fact_id;
        BOR_ISET_FOR_EACH(&e->ce_pre, fact_id)
            ce_size += borISetSize(&fdr_var->strips_id_to_val[fact_id]);
        fprintf(fout, "%d", ce_size);
        BOR_ISET_FOR_EACH(&e->ce_pre, fact_id){
            int val_id;
            BOR_ISET_FOR_EACH(&fdr_var->strips_id_to_val[fact_id], val_id){
                const pddl_fdr_val_t *v = fdr_var->global_id_to_val[val_id];
                fprintf(fout, " %d %d", v->var_id, v->val_id);
            }
        }
        fprintf(fout, " %d %d %d\n", e->var, e->pre, e->eff);
    }

    fprintf(fout, "%d\n", op->cost);
    fprintf(fout, "end_operator\n");

    freeEffs(&effs);
    if (pre != NULL)
        BOR_FREE(pre);
    if (pre_test != NULL)
        BOR_FREE(pre_test);
    if (pre_var_eff != NULL)
        BOR_FREE(pre_var_eff);
    if (eff != NULL)
        BOR_FREE(eff);
}


void pddlFDRPrintAsFD(const pddl_strips_t *strips,
                      const pddl_mgroups_t *mg,
                      const pddl_mutex_pairs_t *mutex,
                      unsigned fdr_var_flags,
                      FILE *fout,
                      bor_err_t *err)
{
    pddl_fdr_vars_t fdr_var;

    if (pddlFDRVarsInitFromStrips(&fdr_var, strips, mg, mutex,
                                  fdr_var_flags) != 0){
        return;
    }

    BOR_INFO(err, "Created %d variables.", fdr_var.var_size);
    int num_none_of_those = 0;
    for (int vi = 0; vi < fdr_var.var_size; ++vi){
        if (fdr_var.var[vi].val_none_of_those != -1)
            ++num_none_of_those;
    }
    BOR_INFO(err, "Created %d none-of-those values.", num_none_of_those);

    fprintf(fout, "begin_version\n3\nend_version\n");
    fprintf(fout, "begin_metric\n1\nend_metric\n");

    // variables
    fprintf(fout, "%d\n", fdr_var.var_size);
    for (int vi = 0; vi < fdr_var.var_size; ++vi){
        const pddl_fdr_var_t *var = fdr_var.var + vi;
        fprintf(fout, "begin_variable\n");
        fprintf(fout, "var%d\n", vi);
        fprintf(fout, "-1\n");
        fprintf(fout, "%d\n", var->val_size);
        for (int vali = 0; vali < var->val_size; ++vali)
            fprintf(fout, "%s\n", var->val[vali].name);
        fprintf(fout, "end_variable\n");
    }

    // mutex groups
    fprintf(fout, "%d\n", mg->mgroup_size);
    for (int mi = 0; mi < mg->mgroup_size; ++mi){
        const pddl_mgroup_t *m = mg->mgroup + mi;
        fprintf(fout, "begin_mutex_group\n");
        fprintf(fout, "%d\n", borISetSize(&m->mgroup));
        int fact_id;
        BOR_ISET_FOR_EACH(&m->mgroup, fact_id){
            // TODO
            int val_id = borISetGet(&fdr_var.strips_id_to_val[fact_id], 0);
            const pddl_fdr_val_t *v = fdr_var.global_id_to_val[val_id];
            fprintf(fout, "%d %d\n", v->var_id, v->val_id);
        }
        fprintf(fout, "end_mutex_group\n");
    }

    // initial state
    fprintf(fout, "begin_state\n");
    int *init = BOR_ALLOC_ARR(int, fdr_var.var_size);
    stripsToFDRState(&fdr_var, &strips->init, init);
    for (int vi = 0; vi < fdr_var.var_size; ++vi)
        fprintf(fout, "%d\n", init[vi]);
    if (init != NULL)
        BOR_FREE(init);
    fprintf(fout, "end_state\n");

    // goal
    fprintf(fout, "begin_goal\n");
    int goal_size = 0;
    int fact_id;
    BOR_ISET_FOR_EACH(&strips->goal, fact_id)
        goal_size += borISetSize(&fdr_var.strips_id_to_val[fact_id]);
    fprintf(fout, "%d\n", goal_size);
    BOR_ISET_FOR_EACH(&strips->goal, fact_id){
        int val_id;
        BOR_ISET_FOR_EACH(&fdr_var.strips_id_to_val[fact_id], val_id){
            const pddl_fdr_val_t *v = fdr_var.global_id_to_val[val_id];
            fprintf(fout, "%d %d\n", v->var_id, v->val_id);
        }
    }
    fprintf(fout, "end_goal\n");

    // operators
    fprintf(fout, "%d\n", numOps(&fdr_var, &strips->op));
    for (int op_id = 0; op_id < strips->op.op_size; ++op_id)
        opToFDR(strips, mutex, &fdr_var, strips->op.op[op_id], fout);
        //fdOp(strips, mutex, &fdr_var, strips->op.op[op_id], fout);

    // axioms
    fprintf(fout, "0\n");

    pddlFDRVarsFree(&fdr_var);
}
