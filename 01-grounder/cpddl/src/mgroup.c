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

#include <boruvka/alloc.h>
#include <boruvka/sort.h>
#include <boruvka/lp.h>
#include "pddl/pddl_struct.h"
#include "pddl/strips.h"
#include "pddl/mgroup.h"
#include "assert.h"

typedef struct pred_tnode pred_tnode_t;
struct pred_tnode {
    int depth;
    int leaf;
    pddl_obj_id_t obj;
    pred_tnode_t *child;
    int child_size;
    int child_alloc;
    bor_iset_t fact;
};

struct pred_tree {
    int arg_size;
    int *arg;
    int *param;
    pred_tnode_t root;
};
typedef struct pred_tree pred_tree_t;


static int checkFact(const pddl_types_t *types,
                     const pddl_ground_atom_t *ga,
                     const pddl_lifted_mgroup_t *mg,
                     const pddl_cond_atom_t *mg_atom)
{
    if (ga->pred != mg_atom->pred)
        return 0;
    ASSERT(ga->arg_size == mg_atom->arg_size);

    for (int ai = 0; ai < ga->arg_size; ++ai){
        if (mg_atom->arg[ai].obj >= 0){
            if (ga->arg[ai] != mg_atom->arg[ai].obj)
                return 0;
        }else{
            int type = mg->param.param[mg_atom->arg[ai].param].type;
            if (!pddlTypesObjHasType(types, type, ga->arg[ai]))
                return 0;
        }
    }

    return 1;
}

static void predTNodeFree(pred_tnode_t *tnode)
{
    for (int i = 0; i < tnode->child_size; ++i)
        predTNodeFree(tnode->child + i);
    if (tnode->child != NULL)
        BOR_FREE(tnode->child);
    borISetFree(&tnode->fact);
}

static void predTreeInitNode(pred_tree_t *tree,
                             pred_tnode_t *tnode,
                             int next,
                             pddl_obj_id_t obj)
{
    bzero(tnode, sizeof(*tnode));
    tnode->depth = next;
    tnode->obj = obj;
    if (next >= tree->arg_size){
        tnode->leaf = 1;
    }else{
        tnode->leaf = 0;
    }
}

struct arg_order {
    const pddl_cond_atom_t *atom;
    const pddl_params_t *mg_params;
};

static int cmpArgOrder(const void *a, const void *b, void *_ao)
{
    int ai1 = *(int *)a;
    int ai2 = *(int *)b;
    const struct arg_order *ao = _ao;
    const pddl_cond_atom_t *atom = ao->atom;
    const pddl_params_t *params = ao->mg_params;

    int a1obj = atom->arg[ai1].obj;
    int a1param = atom->arg[ai1].param;
    int a2obj = atom->arg[ai2].obj;
    int a2param = atom->arg[ai2].param;
    if (a1obj >= 0 && a2obj >= 0){
        return a1obj - a2obj;
    }else if (a1obj >= 0){
        return 1;
    }else if (a2obj >= 0){
        return -1;
    }else{
        if (params->param[a1param].is_counted_var
                == params->param[a2param].is_counted_var){
            return a1param - a2param;
        }else if (params->param[a1param].is_counted_var){
            return 1;
        }else{
            return -1;
        }
    }

}

static void predTreeInit(pred_tree_t *tree,
                         const pddl_lifted_mgroup_t *mg,
                         const pddl_cond_atom_t *atom)
{
    bzero(tree, sizeof(*tree));
    tree->arg_size = atom->arg_size;
    tree->arg = BOR_ALLOC_ARR(int, atom->arg_size);

    for (int i = 0; i < tree->arg_size; ++i)
        tree->arg[i] = i;
    struct arg_order ao = { atom, &mg->param };
    borSort(tree->arg, tree->arg_size, sizeof(int), cmpArgOrder, &ao);

    int arg_size = 0;
    for (; arg_size < tree->arg_size; ++arg_size){
        int argi = tree->arg[arg_size];
        if (atom->arg[argi].obj >= 0
                || mg->param.param[atom->arg[argi].param].is_counted_var){
            break;
        }
    }
    tree->arg_size = arg_size;

    tree->param = BOR_ALLOC_ARR(int, tree->arg_size);
    for (int i = 0; i < tree->arg_size; ++i){
        tree->param[i] = atom->arg[tree->arg[i]].param;
    }

    predTreeInitNode(tree, &tree->root, 0, -1);
}

static void predTreeFree(pred_tree_t *tree)
{
    if (tree->arg != NULL)
        BOR_FREE(tree->arg);
    if (tree->param != NULL)
        BOR_FREE(tree->param);
    predTNodeFree(&tree->root);
}

static void _predTreeAdd(pred_tree_t *tree,
                         pred_tnode_t *tnode,
                         const pddl_fact_t *fact)
{
    if (tnode->leaf){
        borISetAdd(&tnode->fact, fact->id);
    }else{
        int argi = tree->arg[tnode->depth];
        pddl_obj_id_t fact_obj = fact->ground_atom->arg[argi];

        for (int i = 0; i < tnode->child_size; ++i){
            if (tnode->child[i].obj == fact_obj){
                _predTreeAdd(tree, tnode->child + i, fact);
                return;
            }
        }

        if (tnode->child_alloc == tnode->child_size){
            if (tnode->child_alloc == 0)
                tnode->child_alloc = 2;
            tnode->child_alloc *= 2;
            tnode->child = BOR_REALLOC_ARR(tnode->child, pred_tnode_t,
                                           tnode->child_alloc);
        }

        pred_tnode_t *next = tnode->child + tnode->child_size++;
        predTreeInitNode(tree, next, tnode->depth + 1, fact_obj);
        _predTreeAdd(tree, next, fact);
    }
}

static void predTreeAdd(pred_tree_t *tree, const pddl_fact_t *fact)
{
    _predTreeAdd(tree, &tree->root, fact);
}


static void buildPredTrees(pred_tree_t *tree,
                           const pddl_t *pddl,
                           const pddl_strips_t *strips,
                           const pddl_lifted_mgroup_t *mg)
{
    bzero(tree, sizeof(*tree) * mg->cond.size);
    for (int ci = 0; ci < mg->cond.size; ++ci){
        const pddl_cond_atom_t *a = PDDL_COND_CAST(mg->cond.cond[ci], atom);
        predTreeInit(tree + ci, mg, a);
    }

    const pddl_fact_t *fact;
    PDDL_FACTS_FOR_EACH(&strips->fact, fact){
        if (fact->ground_atom == NULL)
            continue;
        const pddl_ground_atom_t *ga = fact->ground_atom;
        for (int ci = 0; ci < mg->cond.size; ++ci){
            const pddl_cond_atom_t *a = PDDL_COND_CAST(mg->cond.cond[ci], atom);
            if (a->pred == ga->pred && checkFact(&pddl->type, ga, mg, a))
                predTreeAdd(&tree[ci], fact);
        }
    }
}

static void _gen(pred_tree_t *tree,
                 pred_tnode_t **tnode,
                 int tree_size,
                 int param_i,
                 const bor_iset_t *params,
                 int lifted_mgroup_id,
                 pddl_mgroups_t *mg)
{
    if (param_i == borISetSize(params)){
        // We have fixed all variables -- create the mutex group from all
        // leaf nodes
        BOR_ISET(mgroup);
        for (int i = 0; i < tree_size; ++i){
            if (tnode[i]->leaf)
                borISetUnion(&mgroup, &tnode[i]->fact);
        }
        if (borISetSize(&mgroup) > 0){
            pddl_mgroup_t *m = pddlMGroupsAdd(mg, &mgroup);
            m->lifted_mgroup_id = lifted_mgroup_id;
        }
        borISetFree(&mgroup);
        return;
    }

    // Find all nodes corresponding to the current parameter and collect
    // all possible objects that can be bound to this parameter
    int param = borISetGet(params, param_i);
    BOR_ISET(relevant_tree);
    BOR_ISET(param_obj);
    for (int i = 0; i < tree_size; ++i){
        if (tnode[i]->depth < tree[i].arg_size
                && tree[i].param[tnode[i]->depth] == param){
            borISetAdd(&relevant_tree, i);
            for (int ci = 0; ci < tnode[i]->child_size; ++ci)
                borISetAdd(&param_obj, tnode[i]->child[ci].obj);
        }
    }

    // Prepare an array of tree nodes for recursive descent
    pred_tnode_t *next_tnode[tree_size];
    int obj;
    BOR_ISET_FOR_EACH(&param_obj, obj){
        // For each object find nodes that match and descent in those nodes
        memcpy(next_tnode, tnode, sizeof(pred_tnode_t *) * tree_size);
        int tree_i;
        BOR_ISET_FOR_EACH(&relevant_tree, tree_i){
            for (int ci = 0; ci < tnode[tree_i]->child_size; ++ci){
                pred_tnode_t *ch = tnode[tree_i]->child + ci;
                if (ch->obj == obj)
                    next_tnode[tree_i] = ch;
            }
        }

        // All trees are matched, we can descent for the current
        // combination of objects
        // Note that at least one next_tnode[] must be changed because we
        // collected objects into param_obj in that way
        _gen(tree, next_tnode, tree_size, param_i + 1, params,
             lifted_mgroup_id, mg);
    }

    borISetFree(&relevant_tree);
    borISetFree(&param_obj);
}

static void gen(pred_tree_t *tree, int tree_size,
                int lifted_mgroup_id, pddl_mgroups_t *mg)
{
    BOR_ISET(param);
    for (int i = 0; i < tree_size; ++i){
        for (int j = 0; j < tree[i].arg_size; ++j){
            borISetAdd(&param, tree[i].param[j]);
        }
    }

    pred_tnode_t *tnode[tree_size];
    for (int i = 0; i < tree_size; ++i)
        tnode[i] = &tree[i].root;

    _gen(tree, tnode, tree_size, 0, &param, lifted_mgroup_id, mg);

    borISetFree(&param);
}


/** DEBUG
static void predTNodePrint(pred_tnode_t *tn,
                           const pddl_strips_t *strips,
                           FILE *fout)
{
    for (int i = 0; i < tn->depth; ++i)
        fprintf(fout, "  ");
    fprintf(fout, "%d:%d:%d ::", tn->obj, tn->depth, tn->leaf);
    int fact;
    BOR_ISET_FOR_EACH(&tn->fact, fact){
        fprintf(fout, " (%s)", strips->fact.fact[fact]->name);
    }
    fprintf(fout, "\n");

    for (int i = 0; i < tn->child_size; ++i){
        predTNodePrint(tn->child + i, strips, fout);
    }
}

static void predTreePrint(pred_tree_t *tree,
                          const pddl_strips_t *strips,
                          FILE *fout)
{
    fprintf(fout, "arg:");
    for (int i = 0; i < tree->arg_size; ++i)
        fprintf(fout, " %d", tree->arg[i]);
    fprintf(fout, ", param:");
    for (int i = 0; i < tree->arg_size; ++i)
        fprintf(fout, " %d", tree->param[i]);
    fprintf(fout, "\n");
    predTNodePrint(&tree->root, strips, fout);
}
*/


static void groundMGroup(pddl_mgroups_t *mg,
                         const pddl_t *pddl,
                         const pddl_strips_t *strips,
                         const pddl_lifted_mgroup_t *lifted_mg,
                         int lifted_mg_id)
{
    if (lifted_mg->cond.size == 0)
        return;

    pred_tree_t tree[lifted_mg->cond.size];
    buildPredTrees(tree, pddl, strips, lifted_mg);

    /** DEBUG
    for (int i = 0; i < lifted_mg->cond.size; ++i){
        fprintf(stderr, "[[%d]]: ", i);
        pddlLiftedMGroupPrint(pddl, lifted_mg, stderr);
        predTreePrint(tree + i, strips, stderr);
    }
    fprintf(stderr, "\n");
    */

    gen(tree, lifted_mg->cond.size, lifted_mg_id, mg);

    for (int i = 0; i < lifted_mg->cond.size; ++i)
        predTreeFree(tree + i);
}

void pddlMGroupFree(pddl_mgroup_t *m)
{
    borISetFree(&m->mgroup);
}

void pddlMGroupsInitEmpty(pddl_mgroups_t *mg)
{
    bzero(mg, sizeof(*mg));
}

void pddlMGroupsInitCopy(pddl_mgroups_t *dst, const pddl_mgroups_t *src)
{
    pddlMGroupsInitEmpty(dst);
    pddlLiftedMGroupsInitCopy(&dst->lifted_mgroup, &src->lifted_mgroup);
    for (int mi = 0; mi < src->mgroup_size; ++mi){
        const pddl_mgroup_t *mg = src->mgroup + mi;
        pddl_mgroup_t *add = pddlMGroupsAdd(dst, &mg->mgroup);
        add->lifted_mgroup_id = mg->lifted_mgroup_id;
        add->is_exactly_one = mg->is_exactly_one;
        add->is_fam_group = mg->is_fam_group;
    }
}

void pddlMGroupsGround(pddl_mgroups_t *mg,
                       const pddl_t *pddl,
                       const pddl_lifted_mgroups_t *lifted_mg,
                       const pddl_strips_t *strips)
{
    pddlMGroupsInitEmpty(mg);
    pddlLiftedMGroupsInitCopy(&mg->lifted_mgroup, lifted_mg);

    for (int mgi = 0; mgi < mg->lifted_mgroup.mgroup_size; ++mgi)
        groundMGroup(mg, pddl, strips, mg->lifted_mgroup.mgroup + mgi, mgi);
    pddlMGroupsSortUniq(mg);

    for (int mgi = 0; mgi < mg->mgroup_size; ++mgi){
        pddl_mgroup_t *m = mg->mgroup + mgi;
        if (m->lifted_mgroup_id >= 0){
            const pddl_lifted_mgroup_t *lm;
            lm = mg->lifted_mgroup.mgroup + m->lifted_mgroup_id;
            if (lm->is_exactly_one)
                m->is_exactly_one = 1;
            m->is_fam_group = 1;
        }
    }
}

void pddlMGroupsFree(pddl_mgroups_t *mg)
{
    pddlLiftedMGroupsFree(&mg->lifted_mgroup);
    for (int i = 0; i < mg->mgroup_size; ++i){
        pddl_mgroup_t *m = mg->mgroup + i;
        pddlMGroupFree(m);
    }
    if (mg->mgroup != NULL)
        BOR_FREE(mg->mgroup);
}


pddl_mgroup_t *pddlMGroupsAdd(pddl_mgroups_t *mg, const bor_iset_t *fact)
{
    if (mg->mgroup_alloc == mg->mgroup_size){
        if (mg->mgroup_alloc == 0)
            mg->mgroup_alloc = 2;
        mg->mgroup_alloc *= 2;
        mg->mgroup = BOR_REALLOC_ARR(mg->mgroup, pddl_mgroup_t,
                                     mg->mgroup_alloc);
    }

    pddl_mgroup_t *m = mg->mgroup + mg->mgroup_size++;
    bzero(m, sizeof(*m));
    m->lifted_mgroup_id = -1;
    borISetUnion(&m->mgroup, fact);
    return m;
}

static int cmpMGroup(const void *a, const void *b, void *_)
{
    const pddl_mgroup_t *m1 = a;
    const pddl_mgroup_t *m2 = b;
    int cmp = borISetSize(&m1->mgroup) - borISetSize(&m2->mgroup);
    if (cmp == 0)
        cmp = borISetCmp(&m1->mgroup, &m2->mgroup);
    if (cmp == 0)
        cmp = m1->lifted_mgroup_id - m2->lifted_mgroup_id;
    return cmp;
}

void pddlMGroupsSortUniq(pddl_mgroups_t *mg)
{
    if (mg->mgroup_size == 0)
        return;

    borSort(mg->mgroup, mg->mgroup_size, sizeof(pddl_mgroup_t),
            cmpMGroup, NULL);

    int ins = 1;
    const pddl_mgroup_t *b = mg->mgroup + 0;
    for (int i = 1; i < mg->mgroup_size; ++i){
        pddl_mgroup_t *m = mg->mgroup + i;
        if (borISetCmp(&b->mgroup, &m->mgroup) == 0){
            pddlMGroupFree(m);
        }else{
            if (ins != i)
                mg->mgroup[ins] = mg->mgroup[i];
            b = mg->mgroup + ins;
            ++ins;
        }
    }
    mg->mgroup_size = ins;
}

static int cmpMGroupSizeDesc(const void *a, const void *b, void *_)
{
    const pddl_mgroup_t *m1 = a;
    const pddl_mgroup_t *m2 = b;
    int cmp = borISetSize(&m2->mgroup) - borISetSize(&m1->mgroup);
    if (cmp == 0)
        cmp = borISetCmp(&m1->mgroup, &m2->mgroup);
    if (cmp == 0)
        cmp = m1->lifted_mgroup_id - m2->lifted_mgroup_id;
    return cmp;
}

void pddlMGroupsSortBySizeDesc(pddl_mgroups_t *mg)
{
    if (mg->mgroup_size == 0)
        return;

    borSort(mg->mgroup, mg->mgroup_size, sizeof(pddl_mgroup_t),
            cmpMGroupSizeDesc, NULL);
}

int pddlMGroupsSetExactlyOne(pddl_mgroups_t *mgs, const pddl_strips_t *strips)
{
    int num = 0;

    for (int mi = 0; mi < mgs->mgroup_size; ++mi){
        pddl_mgroup_t *mg = mgs->mgroup + mi;
        if (borISetIsDisjunct(&mg->mgroup, &strips->init)){
            mg->is_exactly_one = 0;
            continue;
        }

        mg->is_exactly_one = 1;

        for (int op_id = 0;
                op_id < strips->op.op_size && mg->is_exactly_one; ++op_id){
            const pddl_strips_op_t *op = strips->op.op[op_id];
            if (!borISetIsDisjunct(&op->del_eff, &mg->mgroup)
                    && borISetIsDisjunct(&op->add_eff, &mg->mgroup)){
                mg->is_exactly_one = 0;
                break;
            }

            for (int ce_id = 0; ce_id < op->cond_eff_size; ++ce_id){
                const pddl_strips_op_cond_eff_t *ce = op->cond_eff + ce_id;
                if (!borISetIsDisjunct(&ce->del_eff, &mg->mgroup)
                        && borISetIsDisjunct(&ce->add_eff, &mg->mgroup)
                        && borISetIsDisjunct(&op->add_eff, &mg->mgroup)){
                    mg->is_exactly_one = 0;
                    break;
                }
            }
        }

        if (mg->is_exactly_one)
            ++num;
    }

    return num;
}

int pddlMGroupsSetGoal(pddl_mgroups_t *mgs, const pddl_strips_t *strips)
{
    int num = 0;
    for (int mi = 0; mi < mgs->mgroup_size; ++mi){
        pddl_mgroup_t *mg = mgs->mgroup + mi;
        if (!borISetIsDisjunct(&mg->mgroup, &strips->goal)){
            ++num;
            mg->is_goal = 1;
        }
    }
    return num;
}

void pddlMGroupsGatherExactlyOneFacts(const pddl_mgroups_t *mgs,
                                      bor_iset_t *set)
{
    for (int mi = 0; mi < mgs->mgroup_size; ++mi){
        if (mgs->mgroup[mi].is_exactly_one)
            borISetUnion(set, &mgs->mgroup[mi].mgroup);
    }
}

void pddlMGroupsReduce(pddl_mgroups_t *mgs, const bor_iset_t *rm_facts)
{
    int max_fact_id = 0;

    if (borISetSize(rm_facts) == 0)
        return;

    max_fact_id = borISetGet(rm_facts, borISetSize(rm_facts) - 1);
    for (int mi = 0; mi < mgs->mgroup_size; ++mi){
        pddl_mgroup_t *mg = mgs->mgroup + mi;
        borISetMinus(&mg->mgroup, rm_facts);
        if (borISetSize(&mg->mgroup) > 0){
            int fid = borISetGet(&mg->mgroup, borISetSize(&mg->mgroup) - 1);
            max_fact_id = BOR_MAX(max_fact_id, fid);
        }
    }

    int *remap = BOR_CALLOC_ARR(int, max_fact_id + 1);
    pddlFactsDelFactsGenRemap(max_fact_id + 1, rm_facts, remap);

    int ins = 0;
    for (int mi = 0; mi < mgs->mgroup_size; ++mi){
        pddl_mgroup_t *mg = mgs->mgroup + mi;
        if (borISetSize(&mg->mgroup) == 0){
            pddlMGroupFree(mg);
        }else{
            borISetRemap(&mg->mgroup, remap);
            mgs->mgroup[ins++] = mgs->mgroup[mi];
        }
    }
    mgs->mgroup_size = ins;
    pddlMGroupsSortUniq(mgs);

    if (remap != NULL)
        BOR_FREE(remap);
}

void pddlMGroupsRemoveSmall(pddl_mgroups_t *mgs, int size)
{
    int ins = 0;
    for (int mi = 0; mi < mgs->mgroup_size; ++mi){
        pddl_mgroup_t *mg = mgs->mgroup + mi;
        if (borISetSize(&mg->mgroup) <= size){
            pddlMGroupFree(mg);
        }else{
            mgs->mgroup[ins++] = mgs->mgroup[mi];
        }
    }
    mgs->mgroup_size = ins;
}

void pddlMGroupsRemoveEmpty(pddl_mgroups_t *mgs)
{
    pddlMGroupsRemoveSmall(mgs, 0);
}

int pddlMGroupsCoverNumber(const pddl_mgroups_t *mgs, int fact_size)
{
    if (mgs->mgroup_size == 0)
        return fact_size;
    if (!borLPSolverAvailable(BOR_LP_DEFAULT)){
        BOR_FATAL2("Can't computed mutex group cover number:"
                   " Missing LP solver!");
    }

    unsigned lp_flags;
    bor_lp_t *lp;
    int cover_number = 0;

    int cols = fact_size + mgs->mgroup_size;
    int rows = fact_size + 1;

    lp_flags  = BOR_LP_DEFAULT;
    lp_flags |= BOR_LP_NUM_THREADS(1);
    lp_flags |= BOR_LP_MIN;
    lp = borLPNew(rows, cols, lp_flags);

    for (int i = 0; i < cols; ++i){
        borLPSetVarBinary(lp, i);
        if (i < fact_size){
            borLPSetObj(lp, i, 0.);
        }else{
            borLPSetObj(lp, i, 1.);
        }
    }

    for (int fact_id = 0; fact_id < fact_size; ++fact_id){
        char sense = 'L';
        double rhs = 0.;
        borLPSetRHS(lp, fact_id, rhs, sense);
        borLPSetCoef(lp, fact_id, fact_id, 1.);
    }

    BOR_ISET(covered_facts);
    for (int mi = 0; mi < mgs->mgroup_size; ++mi){
        const pddl_mgroup_t *mg = mgs->mgroup + mi;
        int fact_id;
        BOR_ISET_FOR_EACH(&mg->mgroup, fact_id)
            borLPSetCoef(lp, fact_id, fact_size + mi, -1.);
        borISetUnion(&covered_facts, &mg->mgroup);
    }

    int fact_id;
    BOR_ISET_FOR_EACH(&covered_facts, fact_id)
        borLPSetCoef(lp, fact_size, fact_id, 1.);
    char sense = 'E';
    double rhs = borISetSize(&covered_facts);
    borLPSetRHS(lp, fact_size, rhs, sense);

    cover_number = fact_size - borISetSize(&covered_facts);
    borISetFree(&covered_facts);

    double val, *obj;
    obj = BOR_ALLOC_ARR(double, cols);
    if (borLPSolve(lp, &val, obj) == 0){
        for (int i = fact_size; i < cols; ++i){
            if (obj[i] > 0.5)
                ++cover_number;
        }

    }else{
        return -1;
    }

    BOR_FREE(obj);
    borLPDel(lp);

    return cover_number;
}

void pddlMGroupsPrint(const pddl_t *pddl,
                      const pddl_strips_t *strips,
                      const pddl_mgroups_t *mg,
                      FILE *fout)
{
    BOR_ISET(lmgs);
    for (int i = 0; i < mg->mgroup_size; ++i){
        const pddl_mgroup_t *m = mg->mgroup + i;
        borISetAdd(&lmgs, m->lifted_mgroup_id);
    }

    int lmgid;
    BOR_ISET_FOR_EACH(&lmgs, lmgid){
        if (lmgid >= 0){
            pddlLiftedMGroupPrint(pddl, mg->lifted_mgroup.mgroup + lmgid, fout);
        }

        for (int i = 0; i < mg->mgroup_size; ++i){
            const pddl_mgroup_t *m = mg->mgroup + i;
            if (m->lifted_mgroup_id != lmgid)
                continue;
            int fact;
            BOR_ISET_FOR_EACH(&m->mgroup, fact){
                fprintf(fout, " (%s)", strips->fact.fact[fact]->name);
            }
            if (m->is_exactly_one)
                fprintf(fout, ":=1");
            if (m->is_fam_group)
                fprintf(fout, ":fam");
            if (m->is_goal)
                fprintf(fout, ":G");
            fprintf(fout, "\n");
        }
        fprintf(fout, "\n");
    }

    borISetFree(&lmgs);
}

void pddlMGroupPrint(const pddl_t *pddl,
                     const pddl_strips_t *strips,
                     const pddl_mgroup_t *mg,
                     FILE *fout)
{
    int init = 0;
    int fact;
    BOR_ISET_FOR_EACH(&mg->mgroup, fact){
        if (init)
            fprintf(fout, " ");
        fprintf(fout, "(%s)", strips->fact.fact[fact]->name);
        init = 1;
    }
    fprintf(fout, "\n");
}
