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
#include "pddl/pddl_struct.h"
#include "err.h"
#include "assert.h"

static int checkDerivedPredicates(const pddl_t *pddl, bor_err_t *err)
{
    const pddl_lisp_node_t *root = &pddl->domain_lisp->root;
    for (int i = 0; i < root->child_size; ++i){
        const pddl_lisp_node_t *n = root->child + i;
        if (pddlLispNodeHeadKw(n) == PDDL_KW_DERIVED){
            BOR_ERR_RET(err, -1, "Derived predicates are not supported"
                                 " (line %d).", n->lineno);
        }
    }
    return 0;
}

static int checkConfig(const pddl_config_t *cfg)
{
    return 1;
}

static const char *parseName(pddl_lisp_t *lisp, int kw,
                             const char *err_name, bor_err_t *err)
{
    const pddl_lisp_node_t *n;

    n = pddlLispFindNode(&lisp->root, kw);
    if (n == NULL){
        // TODO: Configure warn/err
        BOR_ERR_RET(err, NULL, "Could not find %s name definition in %s.",
                    err_name, lisp->filename);
    }

    if (n->child_size != 2 || n->child[1].value == NULL){
        BOR_ERR_RET(err, NULL, "Invalid %s name definition in %s.",
                    err_name, lisp->filename);
    }

    return n->child[1].value;
}

static char *parseDomainName(pddl_lisp_t *lisp, bor_err_t *err)
{
    const char *name = parseName(lisp, PDDL_KW_DOMAIN, "domain", err);
    if (name != NULL)
        return BOR_STRDUP(name);
    return NULL;
}

static char *parseProblemName(pddl_lisp_t *lisp, bor_err_t *err)
{
    const char *name = parseName(lisp, PDDL_KW_PROBLEM, "problem", err);
    if (name != NULL)
        return BOR_STRDUP(name);
    return NULL;
}

static int checkDomainName(pddl_t *pddl, bor_err_t *err)
{
    const char *problem_domain_name;

    // TODO: Configure err/warn/nothing
    problem_domain_name = parseName(pddl->problem_lisp,
                                    PDDL_KW_DOMAIN2, ":domain", err);
    if (problem_domain_name == NULL)
        BOR_TRACE_RET(err, 0);

    if (strcmp(problem_domain_name, pddl->domain_name) != 0){
        BOR_WARN(err, "Domain names does not match: `%s' x `%s'",
                 pddl->domain_name, problem_domain_name);
        return 0;
    }
    return 0;
}

static int parseMetric(pddl_t *pddl, const pddl_lisp_t *lisp, bor_err_t *err)
{
    const pddl_lisp_node_t *n;

    n = pddlLispFindNode(&lisp->root, PDDL_KW_METRIC);
    if (n == NULL)
        return 0;

    if (n->child_size != 3
            || n->child[1].value == NULL
            || n->child[1].kw != PDDL_KW_MINIMIZE
            || n->child[2].value != NULL
            || n->child[2].child_size != 1
            || strcmp(n->child[2].child[0].value, "total-cost") != 0){
        BOR_ERR_RET(err, -1, "Only (:metric minimize (total-cost)) is supported"
                    " (line %d in %s).", n->lineno, lisp->filename);
    }

    pddl->metric = 1;
    return 0;
}

static int parseInit(pddl_t *pddl, bor_err_t *err)
{
    const pddl_lisp_node_t *ninit;

    ninit = pddlLispFindNode(&pddl->problem_lisp->root, PDDL_KW_INIT);
    if (ninit == NULL){
        BOR_ERR_RET(err, -1, "Missing :init in %s.",
                    pddl->problem_lisp->filename);
    }

    pddl->init = pddlCondParseInit(ninit, pddl, err);
    if (pddl->init == NULL){
        BOR_TRACE_PREPEND_RET(err, -1, "While parsing :init specification"
                              " in %s: ", pddl->problem_lisp->filename);
    }

    pddl_cond_const_it_atom_t it;
    const pddl_cond_atom_t *atom;
    PDDL_COND_FOR_EACH_ATOM(&pddl->init->cls, &it, atom)
        pddl->pred.pred[atom->pred].in_init = 1;

    return 0;
}

static int parseGoal(pddl_t *pddl, bor_err_t *err)
{
    const pddl_lisp_node_t *ngoal;

    ngoal = pddlLispFindNode(&pddl->problem_lisp->root, PDDL_KW_GOAL);
    if (ngoal == NULL)
        BOR_ERR_RET(err, -1, "Missing :goal in %s.", pddl->problem_lisp->filename);

    if (ngoal->child_size != 2 || ngoal->child[1].value != NULL){
        BOR_ERR_RET(err, -1, "Invalid definition of :goal in %s (line %d).",
                    pddl->problem_lisp->filename, ngoal->lineno);
    }

    pddl->goal = pddlCondParse(ngoal->child + 1, pddl, NULL, "", err);
    if (pddl->goal == NULL){
        BOR_TRACE_PREPEND_RET(err, -1, "While parsing :goal specification"
                              " in %s: ", pddl->problem_lisp->filename);
    }
    return 0;
}

int pddlInit(pddl_t *pddl, const char *domain_fn, const char *problem_fn,
             const pddl_config_t *cfg, bor_err_t *err)
{
    bzero(pddl, sizeof(*pddl));
    pddl->cfg = *cfg;

    BOR_INFO(err, "Translation of %s and %s.", domain_fn, problem_fn);

    if (!checkConfig(cfg))
        BOR_TRACE_RET(err, -1);

    BOR_INFO2(err, "Parsing domain lisp file...");
    pddl->domain_lisp = pddlLispParse(domain_fn, err);
    if (pddl->domain_lisp == NULL)
        BOR_TRACE_RET(err, -1);

    BOR_INFO2(err, "Parsing problem lisp file...");
    pddl->problem_lisp = pddlLispParse(problem_fn, err);
    if (pddl->problem_lisp == NULL){
        if (pddl->domain_lisp)
            pddlLispDel(pddl->domain_lisp);
        BOR_TRACE_RET(err, -1);
    }

    BOR_INFO2(err, "Parsing entire contents of domain/problem PDDL...");
    pddl->domain_name = parseDomainName(pddl->domain_lisp, err);
    if (pddl->domain_name == NULL)
        goto pddl_fail;

    pddl->problem_name = parseProblemName(pddl->problem_lisp, err);
    if (pddl->domain_name == NULL)
        goto pddl_fail;

    if (checkDerivedPredicates(pddl, err) != 0
            || checkDomainName(pddl, err) != 0
            || pddlRequireParse(pddl, err) != 0
            || pddlTypesParse(pddl, err) != 0
            || pddlObjsParse(pddl, err) != 0
            || pddlPredsParse(pddl, err) != 0
            || pddlFuncsParse(pddl, err) != 0
            || parseInit(pddl, err) != 0
            || parseGoal(pddl, err) != 0
            || pddlActionsParse(pddl, err) != 0
            || parseMetric(pddl, pddl->problem_lisp, err) != 0){
        goto pddl_fail;
    }
    pddlTypesBuildObjTypeMap(&pddl->type, pddl->obj.obj_size);
    BOR_INFO2(err, "PDDL content parsed.");

    return 0;

pddl_fail:
    if (pddl != NULL)
        pddlFree(pddl);
    BOR_TRACE_RET(err, -1);
}

void pddlInitCopy(pddl_t *dst, const pddl_t *src)
{
    bzero(dst, sizeof(*dst));
    dst->cfg = src->cfg;
    dst->domain_lisp = pddlLispClone(src->domain_lisp);
    dst->problem_lisp = pddlLispClone(src->problem_lisp);
    if (src->domain_name != NULL)
        dst->domain_name = BOR_STRDUP(src->domain_name);
    if (src->problem_name != NULL)
        dst->problem_name = BOR_STRDUP(src->problem_name);
    dst->require = src->require;
    pddlTypesInitCopy(&dst->type, &src->type);
    pddlObjsInitCopy(&dst->obj, &src->obj);
    pddlPredsInitCopy(&dst->pred, &src->pred);
    pddlPredsInitCopy(&dst->func, &src->func);
    if (src->init != NULL)
        dst->init = PDDL_COND_CAST(pddlCondClone(&src->init->cls), part);
    if (src->goal != NULL)
        dst->goal = pddlCondClone(src->goal);
    pddlActionsInitCopy(&dst->action, &src->action);
    dst->metric = src->metric;
    dst->normalized = src->normalized;
}

pddl_t *pddlNew(const char *domain_fn, const char *problem_fn,
                const pddl_config_t *cfg, bor_err_t *err)
{
    pddl_t *pddl = BOR_ALLOC(pddl_t);

    if (pddlInit(pddl, domain_fn, problem_fn, cfg, err) != 0){
        BOR_FREE(pddl);
        return NULL;
    }

    return pddl;
}

void pddlDel(pddl_t *pddl)
{
    pddlFree(pddl);
    BOR_FREE(pddl);
}

void pddlFree(pddl_t *pddl)
{
    if (pddl->domain_lisp)
        pddlLispDel(pddl->domain_lisp);
    if (pddl->problem_lisp)
        pddlLispDel(pddl->problem_lisp);
    if (pddl->domain_name != NULL)
        BOR_FREE(pddl->domain_name);
    if (pddl->problem_name != NULL)
        BOR_FREE(pddl->problem_name);
    pddlTypesFree(&pddl->type);
    pddlObjsFree(&pddl->obj);
    pddlPredsFree(&pddl->pred);
    pddlPredsFree(&pddl->func);
    if (pddl->init)
        pddlCondDel(&pddl->init->cls);
    if (pddl->goal)
        pddlCondDel(pddl->goal);
    pddlActionsFree(&pddl->action);
}

static int markNegPre(pddl_cond_t *c, void *_m)
{
    pddl_cond_atom_t *atom;
    int *m = _m;

    if (c->type == PDDL_COND_ATOM){
        atom = PDDL_COND_CAST(c, atom);
        if (atom->neg)
            m[atom->pred] = 1;
    }

    return 0;
}

static int markNegPreWhen(pddl_cond_t *c, void *_m)
{
    pddl_cond_when_t *when;

    if (c->type == PDDL_COND_WHEN){
        when = PDDL_COND_CAST(c, when);
        pddlCondTraverse(when->pre, markNegPre, NULL, _m);
    }

    return 0;
}

/** Sets to 1 indexes in {np} of those predicates that are not static and
 *  appear as negative preconditions */
static void findNonStaticPredInNegPre(pddl_t *pddl, int *np)
{
    int i;

    bzero(np, sizeof(int) * pddl->pred.pred_size);
    for (i = 0; i < pddl->action.action_size; ++i){
        pddlCondTraverse(pddl->action.action[i].pre, markNegPre, NULL, np);
        pddlCondTraverse(pddl->action.action[i].eff, markNegPreWhen, NULL, np);
    }
    // Also, check the goal
    if (pddl->goal)
        pddlCondTraverse(pddl->goal, markNegPre, NULL, np);

    for (i = 0; i < pddl->pred.pred_size; ++i){
        if (pddlPredIsStatic(pddl->pred.pred + i))
            np[i] = 0;
    }
}

/** Create a new NOT-... predicate and returns its ID */
static int createNewNotPred(pddl_t *pddl, int pred_id)
{
    pddl_pred_t *pos = pddl->pred.pred + pred_id;
    pddl_pred_t *neg;
    int name_size;
    char *name;

    name_size = strlen(pos->name) + 4;
    name = BOR_ALLOC_ARR(char, name_size + 1);
    strcpy(name, "NOT-");
    strcpy(name + 4, pos->name);

    neg = pddlPredsAddCopy(&pddl->pred, pred_id);
    if (neg->name != NULL)
        BOR_FREE(neg->name);
    neg->name = name;
    neg->neg_of = pred_id;
    pddl->pred.pred[pred_id].neg_of = neg->id;

    return neg->id;
}

static int replaceNegPre(pddl_cond_t **c, void *_ids)
{
    int *ids = _ids;
    int pos = ids[0];
    int neg = ids[1];
    pddl_cond_atom_t *atom;

    if ((*c)->type == PDDL_COND_ATOM){
        atom = PDDL_COND_CAST(*c, atom);
        if (atom->pred == pos && atom->neg){
            atom->pred = neg;
            atom->neg = 0;
        }
    }

    return 0;
}

static int replaceNegEff(pddl_cond_t **c, void *_ids)
{
    int *ids = _ids;
    int pos = ids[0];
    int neg = ids[1];
    pddl_cond_t *c2;
    pddl_cond_atom_t *atom, *not_atom;
    pddl_cond_when_t *when;
    pddl_cond_part_t *and;

    if ((*c)->type == PDDL_COND_WHEN){
        when = PDDL_COND_CAST(*c, when);
        pddlCondRebuild(&when->pre, NULL, replaceNegPre, _ids);
        pddlCondRebuild(&when->eff, replaceNegEff, NULL, _ids);
        return -1;

    }else if ((*c)->type == PDDL_COND_ATOM){
        atom = PDDL_COND_CAST(*c, atom);
        if (atom->pred == pos){
            // Create new NOT atom and flip negation
            c2 = pddlCondClone(*c);
            not_atom = PDDL_COND_CAST(c2, atom);
            not_atom->pred = neg;
            not_atom->neg = !atom->neg;

            // Transorm atom to (and atom)
            *c = pddlCondAtomToAnd(*c);
            and = PDDL_COND_CAST(*c, part);
            pddlCondPartAdd(and, c2);

            // Prevent recursion
            return -1;
        }
    }

    return 0;
}

static void compileOutNegPreInAction(pddl_t *pddl, int pos, int neg,
                                     pddl_action_t *a)
{
    int ids[2] = { pos, neg };
    pddlCondRebuild(&a->pre, NULL, replaceNegPre, ids);
    pddlCondRebuild(&a->eff, replaceNegEff, NULL, ids);
    pddlActionNormalize(a, pddl);
}

static void compileOutNegPre(pddl_t *pddl, int pos, int neg)
{
    int i;

    for (i = 0; i < pddl->action.action_size; ++i)
        compileOutNegPreInAction(pddl, pos, neg, pddl->action.action + i);

    if (pddl->goal){
        int ids[2] = { pos, neg };
        pddlCondRebuild(&pddl->goal, NULL, replaceNegPre, ids);
    }
}

static int initHasFact(const pddl_t *pddl, int pred,
                       int arg_size, const pddl_obj_id_t *arg)
{
    bor_list_t *item;
    const pddl_cond_t *c;
    const pddl_cond_atom_t *a;
    int i;

    BOR_LIST_FOR_EACH(&pddl->init->part, item){
        c = BOR_LIST_ENTRY(item, const pddl_cond_t, conn);
        if (c->type != PDDL_COND_ATOM)
            continue;
        a = PDDL_COND_CAST(c, atom);
        if (a->pred != pred || a->arg_size != arg_size)
            continue;
        for (i = 0; i < arg_size; ++i){
            if (a->arg[i].obj != arg[i])
                break;
        }
        if (i == arg_size)
            return 1;
    }

    return 0;
}

static void addNotPredsToInitRec(pddl_t *pddl, int pos, int neg,
                                 int arg_size, pddl_obj_id_t *arg,
                                 const pddl_pred_t *pred, int argi)
{
    pddl_cond_atom_t *a;
    const pddl_obj_id_t *obj;
    int obj_size;

    if (argi == arg_size){
        if (!initHasFact(pddl, pos, arg_size, arg)){
            a = pddlCondCreateFactAtom(neg, arg_size, arg);
            pddlCondPartAdd(pddl->init, &a->cls);
        }

        return;
    }

    obj = pddlTypesObjsByType(&pddl->type, pred->param[argi], &obj_size);
    for (int i = 0; i < obj_size; ++i){
        arg[argi] = obj[i];
        addNotPredsToInitRec(pddl, pos, neg, arg_size, arg, pred, argi + 1);
    }
}

static void addNotPredsToInit(pddl_t *pddl, int pos, int neg)
{
    const pddl_pred_t *pos_pred = pddl->pred.pred + pos;
    pddl_obj_id_t arg[pos_pred->param_size];

    // Recursivelly try all possible objects for each argument
    addNotPredsToInitRec(pddl, pos, neg,
                         pos_pred->param_size, arg, pos_pred, 0);
}

/** Compile out negative preconditions if they are not static */
static void compileOutNonStaticNegPre(pddl_t *pddl)
{
    int size, *negpred;

    size = pddl->pred.pred_size;
    negpred = BOR_ALLOC_ARR(int, size);
    findNonStaticPredInNegPre(pddl, negpred);

    for (int i = 0; i < size; ++i){
        if (negpred[i]){
            int not = createNewNotPred(pddl, i);
            compileOutNegPre(pddl, i, not);
            addNotPredsToInit(pddl, i, not);
        }
    }
    BOR_FREE(negpred);
}

static int isFalsePre(const pddl_cond_t *c)
{
    if (c->type == PDDL_COND_BOOL){
        const pddl_cond_bool_t *b = PDDL_COND_CAST(c, bool);
        return !b->val;
    }
    return 0;
}

static void removeIrrelevantActions(pddl_t *pddl)
{
    for (int ai = 0; ai < pddl->action.action_size;){
        pddl_action_t *a = pddl->action.action + ai;
        a->pre = pddlCondDeconflictPre(a->pre, pddl, &a->param);
        a->eff = pddlCondDeconflictEff(a->eff, pddl, &a->param);

        if (isFalsePre(a->pre) || !pddlCondHasAtom(a->eff)){
            pddlActionFree(a);
            if (ai != pddl->action.action_size - 1)
                *a = pddl->action.action[pddl->action.action_size - 1];
            --pddl->action.action_size;
        }else{
            ++ai;
        }
    }
}

static int removeActionsWithUnsatisfiableArgs(pddl_t *pddl)
{
    int ret = 0;
    for (int ai = 0; ai < pddl->action.action_size;){
        pddl_action_t *a = pddl->action.action + ai;
        int remove = 0;
        for (int pi = 0; pi < a->param.param_size; ++pi){
            if (pddlTypeNumObjs(&pddl->type, a->param.param[pi].type) == 0){
                remove = 1;
                break;
            }
        }

        if (remove){
            pddlActionFree(a);
            if (ai != pddl->action.action_size - 1)
                *a = pddl->action.action[pddl->action.action_size - 1];
            --pddl->action.action_size;
            ret = 1;
        }else{
            ++ai;
        }
    }

    return ret;
}

static int isStaticPreUnreachable(const pddl_t *pddl, const pddl_cond_t *c)
{
    pddl_cond_const_it_atom_t it;
    const pddl_cond_atom_t *atom;
    PDDL_COND_FOR_EACH_ATOM(c, &it, atom){
        const pddl_pred_t *pred = pddl->pred.pred + atom->pred;
        if (pred->id != pddl->pred.eq_pred
                && pddlPredIsStatic(pred)
                && !pred->in_init){
            return 1;
        }
    }
    return 0;
}

static int isInequalityUnsatisfiable(const pddl_t *pddl,
                                     const pddl_action_t *action)
{
    pddl_cond_const_it_atom_t it;
    const pddl_cond_atom_t *atom;
    PDDL_COND_FOR_EACH_ATOM(action->pre, &it, atom){
        if (atom->neg && atom->pred == pddl->pred.eq_pred){
            int param1 = atom->arg[0].param;
            int obj1 = atom->arg[0].obj;
            int param2 = atom->arg[1].param;
            int obj2 = atom->arg[1].obj;
            if (param1 >= 0){
                int type = action->param.param[param1].type;
                if (pddlTypeNumObjs(&pddl->type, type) == 1)
                    obj1 = pddlTypeGetObj(&pddl->type, type, 0);
            }
            if (param2 >= 0){
                int type = action->param.param[param2].type;
                if (pddlTypeNumObjs(&pddl->type, type) == 1)
                    obj2 = pddlTypeGetObj(&pddl->type, type, 0);
            }

            if (obj1 >= 0 && obj2 >= 0 && obj1 == obj2)
                return 1;
        }
    }
    return 0;
}

static int removeUnreachableActions(pddl_t *pddl)
{
    int ret = 0;
    for (int ai = 0; ai < pddl->action.action_size;){
        pddl_action_t *a = pddl->action.action + ai;
        a->pre = pddlCondDeconflictPre(a->pre, pddl, &a->param);
        a->eff = pddlCondDeconflictEff(a->eff, pddl, &a->param);

        if (isStaticPreUnreachable(pddl, a->pre)
                || isInequalityUnsatisfiable(pddl, a)){
            pddlActionFree(a);
            if (ai != pddl->action.action_size - 1)
                *a = pddl->action.action[pddl->action.action_size - 1];
            --pddl->action.action_size;
            ret = 1;
        }else{
            ++ai;
        }
    }

    return ret;
}

static void pddlResetPredReadWrite(pddl_t *pddl)
{
    for (int i = 0; i < pddl->pred.pred_size; ++i)
        pddl->pred.pred[i].read = pddl->pred.pred[i].write = 0;
    for (int i = 0; i < pddl->action.action_size; ++i){
        const pddl_action_t *a = pddl->action.action + i;
        pddlCondSetPredRead(a->pre, &pddl->pred);
        pddlCondSetPredReadWriteEff(a->eff, &pddl->pred);
    }
}

void pddlNormalize(pddl_t *pddl)
{
    removeActionsWithUnsatisfiableArgs(pddl);

    for (int i = 0; i < pddl->action.action_size; ++i)
        pddlActionNormalize(pddl->action.action + i, pddl);

    for (int i = 0; i < pddl->action.action_size; ++i)
        pddlActionSplit(pddl->action.action + i, pddl);

    removeIrrelevantActions(pddl);

#ifdef PDDL_DEBUG
    for (int i = 0; i < pddl->action.action_size; ++i){
        pddlActionAssertPreConjuction(pddl->action.action + i);
    }
#endif

    if (pddl->goal)
        pddl->goal = pddlCondNormalize(pddl->goal, pddl, NULL);

    compileOutNonStaticNegPre(pddl);
    removeIrrelevantActions(pddl);
    do {
        pddlResetPredReadWrite(pddl);
    } while (removeUnreachableActions(pddl));
    pddl->normalized = 1;
}

static void compileAwayCondEff(pddl_t *pddl, int only_non_static)
{
    pddl_action_t *a, *new_a;
    pddl_cond_when_t *w;
    pddl_cond_t *neg_pre;
    int asize;
    int change;

    do {
        change = 0;
        pddlNormalize(pddl);

        asize = pddl->action.action_size;
        for (int ai = 0; ai < asize; ++ai){
            a = pddl->action.action + ai;
            if (only_non_static){
                w = pddlCondRemoveFirstNonStaticWhen(a->eff, pddl);
            }else{
                w = pddlCondRemoveFirstWhen(a->eff, pddl);
            }
            if (w != NULL){
                // Create a new action
                new_a = pddlActionsAddCopy(&pddl->action, ai);

                // Get the original action again, because pddlActionsAdd()
                // could realloc the array.
                a = pddl->action.action + ai;

                // The original takes additional precondition which is the
                // negation of w->pre
                if ((neg_pre = pddlCondNegate(w->pre, pddl)) == NULL){
                    // This shoud never fail, because we force
                    // normalization before this.
                    BOR_FATAL2("Fatal Error: Encountered problem in"
                               " the normalization.");
                }
                a->pre = pddlCondNewAnd2(a->pre, neg_pre);

                // The new action extends both pre and eff by w->pre and
                // w->eff.
                new_a->pre = pddlCondNewAnd2(new_a->pre, pddlCondClone(w->pre));
                new_a->eff = pddlCondNewAnd2(new_a->eff, pddlCondClone(w->eff));

                pddlCondDel(&w->cls);
                change = 1;
            }
        }
    } while (change);
    pddlResetPredReadWrite(pddl);
}

void pddlCompileAwayCondEff(pddl_t *pddl)
{
    compileAwayCondEff(pddl, 0);
}

void pddlCompileAwayNonStaticCondEff(pddl_t *pddl)
{
    compileAwayCondEff(pddl, 1);
}

int pddlPredFuncMaxParamSize(const pddl_t *pddl)
{
    int max = 0;

    for (int i = 0; i < pddl->pred.pred_size; ++i)
        max = BOR_MAX(max, pddl->pred.pred[i].param_size);
    for (int i = 0; i < pddl->func.pred_size; ++i)
        max = BOR_MAX(max, pddl->func.pred[i].param_size);

    return max;
}

void pddlCheckSizeTypes(const pddl_t *pddl)
{
    unsigned long max_size;

    max_size = (1ul << (sizeof(pddl_obj_size_t) * 8)) - 1;
    if (pddl->obj.obj_size > max_size){
        BOR_FATAL("The problem has %d objects, but pddl_obj_size_t can"
                  " hold only %lu.",
                  pddl->obj.obj_size,
                  sizeof(pddl_obj_size_t) * 8 - 1);
    }

    max_size = (1ul << (sizeof(pddl_action_param_size_t) * 8)) - 1;
    for (int ai = 0; ai < pddl->action.action_size; ++ai){
        int param_size = pddl->action.action[ai].param.param_size;
        if (param_size > max_size){
            BOR_FATAL("The action %s has %d parameters, but"
                      "pddl_action_param_size_t can hold only %lu.",
                      pddl->action.action[ai].name,
                      param_size,
                      sizeof(pddl_action_param_size_t) * 8 - 1);
        }
    }
}

void pddlAddObjectTypes(pddl_t *pddl)
{
    for (pddl_obj_id_t obj_id = 0; obj_id < pddl->obj.obj_size; ++obj_id){
        pddl_obj_t *obj = pddl->obj.obj + obj_id;
        ASSERT(obj->type >= 0);
        if (pddlTypeNumObjs(&pddl->type, obj->type) <= 1)
            continue;

        char *name = BOR_ALLOC_ARR(char, strlen(obj->name) + 8 + 1);
        sprintf(name, "%s-OBJTYPE", obj->name);
        int type_id = pddlTypesAdd(&pddl->type, name, obj->type);
        ASSERT(type_id == pddl->type.type_size - 1);
        pddlTypesAddObj(&pddl->type, obj_id, type_id);
        obj->type = type_id;
        BOR_FREE(name);
    }
    pddlTypesBuildObjTypeMap(&pddl->type, pddl->obj.obj_size);
}

void pddlPrintPDDLDomain(const pddl_t *pddl, FILE *fout)
{
    fprintf(fout, "(define (domain %s)\n", pddl->domain_name);
    pddlRequirePrintPDDL(pddl->require, fout);
    pddlTypesPrintPDDL(&pddl->type, fout);
    pddlObjsPrintPDDLConstants(&pddl->obj, &pddl->type, fout);
    pddlPredsPrintPDDL(&pddl->pred, &pddl->type, fout);
    pddlFuncsPrintPDDL(&pddl->func, &pddl->type, fout);
    pddlActionsPrintPDDL(&pddl->action, pddl, fout);
    fprintf(fout, ")\n");
}

void pddlPrintPDDLProblem(const pddl_t *pddl, FILE *fout)
{
    bor_list_t *item;
    pddl_cond_t *c;
    pddl_params_t params;

    fprintf(fout, "(define (problem %s) (:domain %s)\n",
            pddl->problem_name, pddl->domain_name);

    pddlParamsInit(&params);
    fprintf(fout, "(:init\n");
    BOR_LIST_FOR_EACH(&pddl->init->part, item){
        c = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
        fprintf(fout, " ");
        pddlCondPrintPDDL(c, pddl, &params, fout);
    }
    fprintf(fout, ")\n");
    pddlParamsFree(&params);

    fprintf(fout, "(:goal ");
    pddlCondPrintPDDL(pddl->goal, pddl, NULL, fout);
    fprintf(fout, ")\n");

    if (pddl->metric)
        fprintf(fout, "(:metric minimize (total-cost))\n");

    fprintf(fout, ")\n");
}

static int initCondSize(const pddl_t *pddl, int type)
{
    bor_list_t *item;
    const pddl_cond_t *c;
    int size = 0;

    BOR_LIST_FOR_EACH(&pddl->init->part, item){
        c = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
        if (c->type == type)
            ++size;
    }
    return size;
}

// TODO: Rename to pddlPrintDebug
void pddlPrintDebug(const pddl_t *pddl, FILE *fout)
{
    bor_list_t *item;
    pddl_cond_t *c;
    pddl_cond_atom_t *a;
    pddl_params_t params;

    fprintf(fout, "Domain: %s\n", pddl->domain_name);
    fprintf(fout, "Problem: %s\n", pddl->problem_name);
    fprintf(fout, "Require: %x\n", pddl->require);
    pddlTypesPrint(&pddl->type, fout);
    pddlObjsPrint(&pddl->obj, fout);
    pddlPredsPrint(&pddl->pred, "Predicate", fout);
    pddlPredsPrint(&pddl->func, "Function", fout);
    pddlActionsPrint(pddl, &pddl->action, fout);

    pddlParamsInit(&params);
    fprintf(fout, "Init[%d]:\n", initCondSize(pddl, PDDL_COND_ATOM));
    BOR_LIST_FOR_EACH(&pddl->init->part, item){
        c = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
        if (c->type != PDDL_COND_ATOM)
            continue;
        a = PDDL_COND_CAST(c, atom);
        fprintf(fout, "  ");
        if (pddlPredIsStatic(&pddl->pred.pred[a->pred]))
            fprintf(fout, "S:");
        pddlCondPrintPDDL(c, pddl, &params, fout);
        fprintf(fout, "\n");
    }

    fprintf(fout, "Init[%d]:\n", initCondSize(pddl, PDDL_COND_ASSIGN));
    BOR_LIST_FOR_EACH(&pddl->init->part, item){
        c = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
        if (c->type != PDDL_COND_ASSIGN)
            continue;
        fprintf(fout, "  ");
        pddlCondPrintPDDL(c, pddl, &params, fout);
        fprintf(fout, "\n");
    }
    pddlParamsFree(&params);

    fprintf(fout, "Goal: ");
    pddlCondPrint(pddl, pddl->goal, NULL, fout);
    fprintf(fout, "\n");

    fprintf(fout, "Metric: %d\n", pddl->metric);
}
