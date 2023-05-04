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

#include <limits.h>
#include <boruvka/alloc.h>
#include <boruvka/lp.h>
#include "pddl/famgroup.h"

struct fam {
    pddl_famgroup_config_t cfg;
    const pddl_strips_t *strips;
    pddl_mgroups_t *mgroups;
    bor_err_t *err;

    bor_lp_t *lp;
    int lp_var_size;
    int row; /*!< ID of the next row */
};
typedef struct fam fam_t;

static int varId(const pddl_strips_t *strips, int fact_id)
{
    return fact_id;
}

static void getPredel(bor_iset_t *predel, const pddl_strips_op_t *op)
{
    borISetEmpty(predel);
    borISetUnion(predel, &op->pre);
    borISetIntersect(predel, &op->del_eff);
}

static int nextRow(fam_t *fam, double rhs, char sense)
{
    if (fam->row >= borLPNumRows(fam->lp)){
        borLPAddRows(fam->lp, 1, &rhs, &sense);
    }else{
        borLPSetRHS(fam->lp, fam->row, rhs, sense);
    }
    return fam->row++;
}

static void initStateConstr(fam_t *fam)
{
    int fact;
    BOR_ISET_FOR_EACH(&fam->strips->init, fact)
        borLPSetCoef(fam->lp, fam->row, varId(fam->strips, fact), 1.);
    borLPSetRHS(fam->lp, fam->row, 1., 'L');
    ++fam->row;
}

static void opConstrs(fam_t *fam)
{
    const pddl_strips_op_t *op;
    int fact;
    BOR_ISET(predel);

    PDDL_STRIPS_OPS_FOR_EACH(&fam->strips->op, op){
        BOR_ISET_FOR_EACH(&op->add_eff, fact)
            borLPSetCoef(fam->lp, fam->row, varId(fam->strips, fact), 1.);

        getPredel(&predel, op);
        BOR_ISET_FOR_EACH(&predel, fact)
            borLPSetCoef(fam->lp, fam->row, varId(fam->strips, fact), -1.);
        borLPSetRHS(fam->lp, fam->row, 0., 'L');
        ++fam->row;
    }
    borISetFree(&predel);
}

static void goalConstr(fam_t *fam)
{
    int row = nextRow(fam, 1, 'G');
    int fact;
    BOR_ISET_FOR_EACH(&fam->strips->goal, fact)
        borLPSetCoef(fam->lp, row, varId(fam->strips, fact), 1.);

}

static void skipMGroupAndSubsetsConstr(fam_t *fam, const bor_iset_t *facts)
{
    int row = nextRow(fam, 1., 'G');

    const int size = borISetSize(facts);
    int fi = 0;
    for (int fact_id = 0; fact_id < fam->strips->fact.fact_size; ++fact_id){
        if (fi < size && borISetGet(facts, fi) == fact_id){
            ++fi;
        }else{
            borLPSetCoef(fam->lp, row, varId(fam->strips, fact_id), 1.);
        }
    }
}

static void skipMGroupExactlyConstr(fam_t *fam, const bor_iset_t *facts)
{
    int row = nextRow(fam, borISetSize(facts) - 1, 'L');
    int fact;

    BOR_ISET_FOR_EACH(facts, fact)
        borLPSetCoef(fam->lp, row, varId(fam->strips, fact), 1.);
}

static void skipMGroup(fam_t *fam, const bor_iset_t *facts)
{
    if (fam->cfg.maximal){
        skipMGroupAndSubsetsConstr(fam, facts);
    }else{
        skipMGroupExactlyConstr(fam, facts);
    }
}


static void objToFAMGroup(const double *obj,
                          const pddl_strips_t *strips,
                          bor_iset_t *fam_group)
{
    borISetEmpty(fam_group);

    for (int fact_id = 0; fact_id < strips->fact.fact_size; ++fact_id){
        int var_id = varId(strips, fact_id);
        if (obj[var_id] > 0.5)
            borISetAdd(fam_group, fact_id);
    }
}

static pddl_mgroup_t *addFAMGroup(pddl_mgroups_t *mgs,
                                  const bor_iset_t *fset,
                                  const pddl_strips_t *strips)
{
    pddl_mgroup_t *mg;

    mg = pddlMGroupsAdd(mgs, fset);
    mg->is_fam_group = 1;
    mg->is_exactly_one = 0;

    return mg;
}

static void genSymmetricFAMGroups(fam_t *fam, const bor_iset_t *mgfacts)
{
    bor_hashset_t set_of_mgroups;

    borHashSetInitISet(&set_of_mgroups);
    borHashSetAdd(&set_of_mgroups, mgfacts);
    pddlStripsSymAllFactSetSymmetries(fam->cfg.sym, &set_of_mgroups);
    for (int i = 1; i < set_of_mgroups.size; ++i){
        const bor_iset_t *fset = borHashSetGet(&set_of_mgroups, i);
        if (!fam->cfg.keep_only_asymetric)
            addFAMGroup(fam->mgroups, fset, fam->strips);
        skipMGroup(fam, fset);
    }
    borHashSetFree(&set_of_mgroups);
}

static void prioritizeUncovered(fam_t *fam)
{
    BOR_ISET(covered);
    for (int mi = 0; mi < fam->mgroups->mgroup_size; ++mi)
        borISetUnion(&covered, &fam->mgroups->mgroup[mi].mgroup);

    for (int fact_id = 0; fact_id < fam->strips->fact.fact_size; ++fact_id){
        int var_id = varId(fam->strips, fact_id);
        if (borISetIn(fact_id, &covered)){
            borLPSetObj(fam->lp, var_id, 1.);
        }else{
            borLPSetObj(fam->lp, var_id, borISetSize(&covered));
        }
        borLPSetVarBinary(fam->lp, var_id);
    }
    borISetFree(&covered);
}

static void famInit(fam_t *fam,
                    pddl_mgroups_t *mgroups,
                    const pddl_strips_t *strips,
                    const pddl_famgroup_config_t *cfg,
                    bor_err_t *err)
{
    unsigned lp_flags;
    int rows;

    bzero(fam, sizeof(*fam));
    fam->cfg = *cfg;
    fam->strips = strips;
    fam->mgroups = mgroups;
    fam->err = err;
    fam->row = 0;

    if (fam->cfg.limit <= 0)
        fam->cfg.limit = INT_MAX;

    if (!borLPSolverAvailable(BOR_LP_DEFAULT)){
        fprintf(stderr, "Missing LP solver! Exiting...\n");
        exit(-1);
    }

    lp_flags  = BOR_LP_DEFAULT;
    lp_flags |= BOR_LP_NUM_THREADS(1); // TODO: Parametrize
    lp_flags |= BOR_LP_MAX;
    fam->lp_var_size = strips->fact.fact_size;
    rows = strips->op.op_size + 1;
    fam->lp = borLPNew(rows, fam->lp_var_size, lp_flags);

    // Set up coeficients in the objective function and set up binary
    // variables
    for (int fact_id = 0; fact_id < strips->fact.fact_size; ++fact_id){
        int var_id = varId(strips, fact_id);
        borLPSetObj(fam->lp, var_id, 1.);
        borLPSetVarBinary(fam->lp, var_id);
    }

    // Initial state constraint
    initStateConstr(fam);
    // Operator constraints
    opConstrs(fam);

    if (fam->cfg.goal)
        goalConstr(fam);

    // Skip mutex groups already stored in mgroups
    for (int i = 0; i < mgroups->mgroup_size; ++i)
        skipMGroup(fam, &mgroups->mgroup[i].mgroup);
}

static void famFree(fam_t *fam)
{
    borLPDel(fam->lp);
}

static void famInfer(fam_t *fam)
{
    BOR_ISET(famgroup);
    double val, *obj;
    pddl_mgroup_t *mg;
    bor_timer_t timer;
    int last_info = 0;

    borTimerStart(&timer);

    obj = BOR_ALLOC_ARR(double, borLPNumCols(fam->lp));
    for (int i = 0;
            borLPSolve(fam->lp, &val, obj) == 0
                && val > 0.5 && i < fam->cfg.limit;
            ++i){
        objToFAMGroup(obj, fam->strips, &famgroup);
        mg = addFAMGroup(fam->mgroups, &famgroup, fam->strips);
        skipMGroup(fam, &mg->mgroup);
        if (fam->cfg.sym != NULL)
            genSymmetricFAMGroups(fam, &mg->mgroup);

        if (fam->cfg.prioritize_uncovered)
            prioritizeUncovered(fam);

        borTimerStop(&timer);
        float elapsed = borTimerElapsedInSF(&timer);
        if ((int)elapsed > last_info){
            BOR_INFO(fam->err, "  Inference of fam-groups: fam-groups: %d",
                     i + 1);
            last_info = elapsed;
        }

        if (fam->cfg.time_limit > 0. && elapsed > fam->cfg.time_limit)
            break;
    }
    BOR_FREE(obj);
    borISetFree(&famgroup);
}

int pddlFAMGroupsInfer(pddl_mgroups_t *mgs,
                       const pddl_strips_t *strips,
                       const pddl_famgroup_config_t *cfg,
                       bor_err_t *err)
{
    if (strips->has_cond_eff)
        BOR_FATAL2("fam-groups does not support conditional effects");

    fam_t fam;
    int start_num = mgs->mgroup_size;
    BOR_INFO(err, "Inference of fam-groups ["
                  "maximal: %d, goal: %d, sym: %d, keep-only-asymetric: %d,"
                  " prioritize-uncovered: %d,"
                  " limit: %d, time-limit: %.2fs] ...",
                  cfg->maximal,
                  cfg->goal,
                  (cfg->sym == NULL ? 0 : 1),
                  cfg->keep_only_asymetric,
                  cfg->prioritize_uncovered,
                  cfg->limit,
                  cfg->time_limit);

    famInit(&fam, mgs, strips, cfg, err);
    famInfer(&fam);
    famFree(&fam);

    BOR_INFO(err, "Inference of fam-groups DONE: %d fam-groups found.",
             mgs->mgroup_size - start_num);
    return 0;
}



static int isDeadEndOp(const bor_iset_t *mgroup,
                       const pddl_strips_op_t *op,
                       bor_iset_t *madd,
                       bor_iset_t *mpredel)
{
    if (borISetSize(&op->pre) < borISetSize(&op->del_eff)){
        borISetIntersect2(mpredel, mgroup, &op->pre);
        borISetIntersect(mpredel, &op->del_eff);
    }else{
        borISetIntersect2(mpredel, mgroup, &op->del_eff);
        borISetIntersect(mpredel, &op->pre);
    }
    borISetIntersect2(madd, mgroup, &op->add_eff);
    return borISetSize(mpredel) > borISetSize(madd);
}

static void deadEndOps(const bor_iset_t *mgroup,
                       const pddl_strips_t *strips,
                       bor_iset_t *madd,
                       bor_iset_t *mpredel,
                       bor_iset_t *dead_end)
{
    for (int op_id = 0; op_id < strips->op.op_size; ++op_id){
        const pddl_strips_op_t *op = strips->op.op[op_id];
        // Skip operators with conditional effects
        if (op->cond_eff_size > 0)
            continue;

        if (isDeadEndOp(mgroup, op, madd, mpredel))
            borISetAdd(dead_end, op->id);
    }
}

void pddlFAMGroupsDeadEndOps(const pddl_mgroups_t *mgs,
                             const pddl_strips_t *strips,
                             bor_iset_t *dead_end_ops)
{
    BOR_ISET(madd);
    BOR_ISET(mpredel);

    for (int mi = 0; mi < mgs->mgroup_size; ++mi){
        const pddl_mgroup_t *mg = mgs->mgroup + mi;
        if (mg->is_fam_group
                && !borISetIsDisjunct(&strips->goal, &mg->mgroup)){
            deadEndOps(&mg->mgroup, strips, &madd, &mpredel, dead_end_ops);
        }
    }

    borISetFree(&madd);
    borISetFree(&mpredel);
}
