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
#include "pddl/config.h"
#include "pddl/pddl.h"
#include "pddl/action.h"
#include "lisp_err.h"
#include "assert.h"


#define PDDL_ACTIONS_ALLOC_INIT 4

#define ERR_PREFIX_MAXSIZE 128


static int parseAction(pddl_t *pddl, const pddl_lisp_node_t *root,
                       bor_err_t *err)
{
    char err_prefix[ERR_PREFIX_MAXSIZE];
    const pddl_lisp_node_t *n;
    pddl_action_t *a;
    int i, ret;

    if (root->child_size < 4
            || root->child_size / 2 == 1
            || root->child[1].value == NULL){
        BOR_ERR_RET2(err, -1, "Invalid definition.");
    }

    a = pddlActionsAddEmpty(&pddl->action);
    a->name = BOR_STRDUP(root->child[1].value);
    for (i = 2; i < root->child_size; i += 2){
        n = root->child + i + 1;
        if (root->child[i].kw == PDDL_KW_AGENT){
            if (!(pddl->require & PDDL_REQUIRE_MULTI_AGENT)){
                // TODO: err/warn
                ERR_LISP_RET2(err, -1, root->child + i,
                              ":agent is allowed only with :multi-agent"
                              " requirement");
            }

            ret = pddlParamsParseAgent(&a->param, root, i, &pddl->type, err);
            if (ret < 0)
                BOR_TRACE_RET(err, -1);
            i = ret - 2;

        }else if (root->child[i].kw == PDDL_KW_PARAMETERS){
            if (pddlParamsParse(&a->param, n, &pddl->type, err) != 0)
                BOR_TRACE_RET(err, -1);

        }else if (root->child[i].kw == PDDL_KW_PRE){
            // Skip empty preconditions, i.e., () or (and)
            //      -- it will be set to the empty conjunction later anyway.
            if (pddlLispNodeIsEmptyAnd(n))
                continue;

            snprintf(err_prefix, ERR_PREFIX_MAXSIZE,
                     "Precondition of the action `%s': ", a->name);
            a->pre = pddlCondParse(n, pddl, &a->param, err_prefix, err);
            if (a->pre == NULL)
                BOR_TRACE_RET(err, -1);
            if (pddlCondCheckPre(a->pre, pddl->require, err) != 0)
                BOR_TRACE_RET(err, -1);
            pddlCondSetPredRead(a->pre, &pddl->pred);

        }else if (root->child[i].kw == PDDL_KW_EFF){
            if (pddlLispNodeIsEmptyAnd(n))
                continue;

            snprintf(err_prefix, ERR_PREFIX_MAXSIZE,
                     "Effect of the action `%s': ", a->name);
            a->eff = pddlCondParse(n, pddl, &a->param, err_prefix, err);
            if (a->eff == NULL)
                BOR_TRACE_RET(err, -1);
            if (pddlCondCheckEff(a->eff, pddl->require, err) != 0)
                BOR_TRACE_RET(err, -1);
            pddlCondSetPredReadWriteEff(a->eff, &pddl->pred);

        }else{
            ERR_LISP_RET(err, -1, root->child + i, "Unexpected token: %s",
                         root->child[i].value);
        }
    }

    // Empty precondition is allowed meaning the action can be applied in
    // any state
    if (a->pre == NULL)
        a->pre = pddlCondNewEmptyAnd();

    // Empty effect is also allowed because of some domains that contain
    // these actions. This action can be later removed by pddlNormalize().
    if (a->eff == NULL)
        a->eff = pddlCondNewEmptyAnd();

    // TODO: Check compatibility of types of parameters and types of
    //       arguments of all predicates.
    //       --> Restrict types instead of disallowing such an action?

    return 0;
}

int pddlActionsParse(pddl_t *pddl, bor_err_t *err)
{
    const pddl_lisp_node_t *root = &pddl->domain_lisp->root;
    const pddl_lisp_node_t *n;

    for (int i = 0; i < root->child_size; ++i){
        n = root->child + i;
        if (pddlLispNodeHeadKw(n) == PDDL_KW_ACTION){
            if (parseAction(pddl, n, err) != 0){
                BOR_TRACE_PREPEND_RET(err, -1, "While parsing :action in %s"
                                      " on line %d: ",
                                      pddl->domain_lisp->filename, n->lineno);
            }
        }
    }
    return 0;
}

void pddlActionsInitCopy(pddl_actions_t *dst, const pddl_actions_t *src)
{
    bzero(dst, sizeof(*dst));
    dst->action_size = dst->action_alloc = src->action_size;
    dst->action = BOR_CALLOC_ARR(pddl_action_t, src->action_size);
    for (int i = 0; i < src->action_size; ++i)
        pddlActionInitCopy(dst->action + i, src->action + i);
}

void pddlActionInit(pddl_action_t *a)
{
    bzero(a, sizeof(*a));
    pddlParamsInit(&a->param);
}

void pddlActionFree(pddl_action_t *a)
{
    if (a->name != NULL)
        BOR_FREE(a->name);
    pddlParamsFree(&a->param);
    if (a->pre != NULL)
        pddlCondDel(a->pre);
    if (a->eff != NULL)
        pddlCondDel(a->eff);
}

void pddlActionInitCopy(pddl_action_t *dst, const pddl_action_t *src)
{
    pddlActionInit(dst);
    if (src->name != NULL)
        dst->name = BOR_STRDUP(src->name);
    pddlParamsInitCopy(&dst->param, &src->param);
    if (src->pre != NULL)
        dst->pre = pddlCondClone(src->pre);
    if (src->eff != NULL)
        dst->eff = pddlCondClone(src->eff);
}

struct propagate_eq {
    pddl_action_t *a;
    int eq_pred;

    const pddl_cond_atom_t *eq_atom;
    int param;
    pddl_obj_id_t obj;
};

static int setParamToObj(pddl_cond_t *cond, void *ud)
{
    struct propagate_eq *ctx = ud;

    if (cond->type == PDDL_COND_ATOM){
        pddl_cond_atom_t *atom = PDDL_COND_CAST(cond, atom);
        if (atom == ctx->eq_atom)
            return 0;

        for (int i = 0; i < atom->arg_size; ++i){
            if (atom->arg[i].param == ctx->param){
                atom->arg[i].param = -1;
                atom->arg[i].obj = ctx->obj;
            }
        }
    }

    return 0;
}

static int _propagateEquality(pddl_cond_t *c, void *ud)
{
    struct propagate_eq *ctx = ud;

    if (c->type == PDDL_COND_ATOM){
        const pddl_cond_atom_t *atom = PDDL_COND_CAST(c, atom);
        if (atom->pred == ctx->eq_pred && !atom->neg){
            if (atom->arg[0].param >= 0 && atom->arg[1].obj >= 0){
                ctx->eq_atom = atom;
                ctx->param = atom->arg[0].param;
                ctx->obj = atom->arg[1].obj;
                pddlCondTraverse(ctx->a->pre, NULL, setParamToObj, ctx);
                pddlCondTraverse(ctx->a->eff, NULL, setParamToObj, ctx);
            }else if (atom->arg[1].param >= 0 && atom->arg[0].obj >= 0){
                ctx->eq_atom = atom;
                ctx->param = atom->arg[1].param;
                ctx->obj = atom->arg[0].obj;
                pddlCondTraverse(ctx->a->pre, NULL, setParamToObj, ctx);
                pddlCondTraverse(ctx->a->eff, NULL, setParamToObj, ctx);
            }
        }
    }
    return 0;
}

static void propagateEquality(pddl_action_t *a, const pddl_t *pddl)
{
    if (a->pre == NULL || pddl->pred.eq_pred < 0)
        return;

    struct propagate_eq ctx = { a, pddl->pred.eq_pred, NULL, -1, -1 };
    if (a->pre->type != PDDL_COND_AND
            && a->pre->type != PDDL_COND_ATOM)
        return;
    pddlCondTraverse(a->pre, _propagateEquality, NULL, &ctx);
}

void pddlActionNormalize(pddl_action_t *a, const pddl_t *pddl)
{
    a->pre = pddlCondNormalize(a->pre, pddl, &a->param);
    a->eff = pddlCondNormalize(a->eff, pddl, &a->param);

    if (a->pre->type == PDDL_COND_BOOL && PDDL_COND_CAST(a->pre, bool)->val){
        pddlCondDel(a->pre);
        a->pre = pddlCondNewEmptyAnd();
    }
    if (a->pre->type == PDDL_COND_ATOM)
        a->pre = pddlCondAtomToAnd(a->pre);
    if (a->eff->type == PDDL_COND_ATOM
            || a->eff->type == PDDL_COND_ASSIGN
            || a->eff->type == PDDL_COND_INCREASE
            || a->eff->type == PDDL_COND_WHEN){
        a->eff = pddlCondAtomToAnd(a->eff);
    }

    propagateEquality(a, pddl);
}

pddl_action_t *pddlActionsAddEmpty(pddl_actions_t *as)
{
    return pddlActionsAddCopy(as, -1);
}

pddl_action_t *pddlActionsAddCopy(pddl_actions_t *as, int copy_id)
{
    pddl_action_t *a;

    if (as->action_size == as->action_alloc){
        if (as->action_alloc == 0)
            as->action_alloc = PDDL_ACTIONS_ALLOC_INIT;
        as->action_alloc *= 2;
        as->action = BOR_REALLOC_ARR(as->action, pddl_action_t,
                                     as->action_alloc);
    }

    a = as->action + as->action_size;
    ++as->action_size;
    if (copy_id >= 0){
        pddlActionInitCopy(a, as->action + copy_id);
    }else{
        pddlActionInit(a);
    }
    return a;
}

void pddlActionsFree(pddl_actions_t *actions)
{
    pddl_action_t *a;
    int i;

    for (i = 0; i < actions->action_size; ++i){
        a = actions->action + i;
        pddlActionFree(a);
    }
    if (actions->action != NULL)
        BOR_FREE(actions->action);
}

void pddlActionSplit(pddl_action_t *a, pddl_t *pddl)
{
    pddl_actions_t *as = &pddl->action;
    pddl_action_t *newa;
    pddl_cond_part_t *pre;
    pddl_cond_t *first_cond, *cond;
    bor_list_t *item;
    int aidx;

    if (a->pre->type != PDDL_COND_OR)
        return;

    pre = bor_container_of(a->pre, pddl_cond_part_t, cls);
    if (borListEmpty(&pre->part))
        return;

    item = borListNext(&pre->part);
    borListDel(item);
    first_cond = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
    a->pre = NULL;
    aidx = a - as->action;
    while (!borListEmpty(&pre->part)){
        item = borListNext(&pre->part);
        borListDel(item);
        cond = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
        newa = pddlActionsAddCopy(as, aidx);
        newa->pre = cond;
        pddlActionNormalize(newa, pddl);
    }
    as->action[aidx].pre = first_cond;
    pddlActionNormalize(as->action + aidx, pddl);

    pddlCondDel(&pre->cls);
}

void pddlActionAssertPreConjuction(pddl_action_t *a)
{
    bor_list_t *item;
    pddl_cond_part_t *pre;
    pddl_cond_t *c;

    if (a->pre->type != PDDL_COND_AND){
        fprintf(stderr, "Fatal Error: Precondition of the action `%s' is"
                        " not a conjuction.\n", a->name);
        exit(-1);
    }

    pre = bor_container_of(a->pre, pddl_cond_part_t, cls);
    BOR_LIST_FOR_EACH(&pre->part, item){
        c = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
        if (c->type != PDDL_COND_ATOM){
            fprintf(stderr, "Fatal Error: Precondition of the action `%s' is"
                            " not a flatten conjuction (conjuction contains"
                            " something else besides atoms).\n", a->name);
            exit(-1);
        }
    }
}

void pddlActionPrint(const pddl_t *pddl, const pddl_action_t *a, FILE *fout)
{
    fprintf(fout, "    %s: ", a->name);
    pddlParamsPrint(&a->param, fout);
    fprintf(fout, "\n");

    fprintf(fout, "        pre: ");
    pddlCondPrint(pddl, a->pre, &a->param, fout);
    fprintf(fout, "\n");

    fprintf(fout, "        eff: ");
    pddlCondPrint(pddl, a->eff, &a->param, fout);
    fprintf(fout, "\n");
}

void pddlActionsPrint(const pddl_t *pddl,
                      const pddl_actions_t *actions,
                      FILE *fout)
{
    int i;

    fprintf(fout, "Action[%d]:\n", actions->action_size);
    for (i = 0; i < actions->action_size; ++i)
        pddlActionPrint(pddl, actions->action + i, fout);
}

static void pddlActionPrintPDDL(const pddl_action_t *a,
                                const pddl_t *pddl,
                                FILE *fout)
{
    fprintf(fout, "(:action %s\n", a->name);
    if (a->param.param_size > 0){
        fprintf(fout, "    :parameters (");
        pddlParamsPrintPDDL(&a->param, &pddl->type, fout);
        fprintf(fout, ")\n");
    }

    if (a->pre != NULL){
        fprintf(fout, "    :precondition ");
        pddlCondPrintPDDL(a->pre, pddl, &a->param, fout);
        fprintf(fout, "\n");
    }

    if (a->eff != NULL){
        fprintf(fout, "    :effect ");
        pddlCondPrintPDDL(a->eff, pddl, &a->param, fout);
        fprintf(fout, "\n");
    }

    fprintf(fout, ")\n");

}

void pddlActionsPrintPDDL(const pddl_actions_t *actions,
                          const pddl_t *pddl,
                          FILE *fout)
{
    for (int i = 0; i < actions->action_size; ++i){
        pddlActionPrintPDDL(&actions->action[i], pddl, fout);
        fprintf(fout, "\n");
    }
}
