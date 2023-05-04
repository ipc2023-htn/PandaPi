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
#include <boruvka/sort.h>

#include "pddl/pddl.h"
#include "pddl/cond.h"
#include "lisp_err.h"
#include "assert.h"

static char *type_names[PDDL_COND_NUM_TYPES] = {
    "and",      /* PDDL_COND_AND */
    "or",       /* PDDL_COND_OR */
    "forall",   /* PDDL_COND_FORALL */
    "exist",    /* PDDL_COND_EXIST */
    "when",     /* PDDL_COND_WHEN */
    "atom",     /* PDDL_COND_ATOM */
    "assign",   /* PDDL_COND_ASSIGN */
    "increase", /* PDDL_COND_INCREASE */
    "bool",     /* PDDL_COND_BOOL */
    "imply",    /* PDDL_COND_IMPLY */
};

const char *pddlCondTypeName(int type)
{
    if (type >= 0 && type < PDDL_COND_NUM_TYPES)
        return type_names[type];
    return "unknown";
}

typedef void (*pddl_cond_method_del_fn)(pddl_cond_t *);
typedef pddl_cond_t *(*pddl_cond_method_clone_fn)(const pddl_cond_t *);
typedef pddl_cond_t *(*pddl_cond_method_negate_fn)(const pddl_cond_t *,
                                                   const pddl_t *);
typedef int (*pddl_cond_method_eq_fn)(const pddl_cond_t *,
                                      const pddl_cond_t *);
typedef int (*pddl_cond_method_traverse_fn)(pddl_cond_t *,
                            int (*pre)(pddl_cond_t *, void *),
                            int (*post)(pddl_cond_t *, void *),
                            void *userdata);
typedef int (*pddl_cond_method_rebuild_fn)(
                            pddl_cond_t **c,
                            int (*pre)(pddl_cond_t **, void *),
                            int (*post)(pddl_cond_t **, void *),
                            void *userdata);
typedef void (*pddl_cond_method_print_pddl_fn)(
                            const pddl_cond_t *c,
                            const pddl_t *pddl,
                            const pddl_params_t *params,
                            FILE *fout);

static int condEq(const pddl_cond_t *, const pddl_cond_t *);
static int condTraverse(pddl_cond_t *c,
                        int (*pre)(pddl_cond_t *, void *),
                        int (*post)(pddl_cond_t *, void *),
                        void *u);
static int condRebuild(pddl_cond_t **c,
                       int (*pre)(pddl_cond_t **, void *),
                       int (*post)(pddl_cond_t **, void *),
                       void *u);

struct pddl_cond_cls {
    pddl_cond_method_del_fn del;
    pddl_cond_method_clone_fn clone;
    pddl_cond_method_negate_fn negate;
    pddl_cond_method_eq_fn eq;
    pddl_cond_method_traverse_fn traverse;
    pddl_cond_method_rebuild_fn rebuild;
    pddl_cond_method_print_pddl_fn print_pddl;
};
typedef struct pddl_cond_cls pddl_cond_cls_t;

#define METHOD(X, NAME) ((pddl_cond_method_##NAME##_fn)(X))
#define MCLS(NAME) \
    { .del = METHOD(cond##NAME##Del, del), \
      .clone = METHOD(cond##NAME##Clone, clone), \
      .negate = METHOD(cond##NAME##Negate, negate), \
      .eq = METHOD(cond##NAME##Eq, eq), \
      .traverse = METHOD(cond##NAME##Traverse, traverse), \
      .rebuild = METHOD(cond##NAME##Rebuild, rebuild), \
      .print_pddl = METHOD(cond##NAME##PrintPDDL, print_pddl), \
    }


struct parse_ctx {
    pddl_types_t *types;
    const pddl_objs_t *objs;
    const pddl_preds_t *preds;
    const pddl_preds_t *funcs;
    const pddl_params_t *params;
    const char *err_prefix;
    bor_err_t *err;
};
typedef struct parse_ctx parse_ctx_t;

#define OBJ(C, T) PDDL_COND_CAST(C, T)

static void condPartDel(pddl_cond_part_t *);
static pddl_cond_part_t *condPartClone(const pddl_cond_part_t *p);
static pddl_cond_part_t *condPartNegate(const pddl_cond_part_t *p,
                                        const pddl_t *pddl);
static int condPartEq(const pddl_cond_part_t *c1, const pddl_cond_part_t *c2);
static void condPartAdd(pddl_cond_part_t *p, pddl_cond_t *add);
static int condPartTraverse(pddl_cond_part_t *,
                            int (*pre)(pddl_cond_t *, void *),
                            int (*post)(pddl_cond_t *, void *),
                            void *userdata);
static int condPartRebuild(pddl_cond_part_t **p,
                           int (*pre)(pddl_cond_t **, void *),
                           int (*post)(pddl_cond_t **, void *),
                           void *userdata);
static void condPartPrintPDDL(const pddl_cond_part_t *p,
                              const pddl_t *pddl,
                              const pddl_params_t *params,
                              FILE *fout);

static void condQuantDel(pddl_cond_quant_t *);
static pddl_cond_quant_t *condQuantClone(const pddl_cond_quant_t *q);
static pddl_cond_quant_t *condQuantNegate(const pddl_cond_quant_t *q,
                                          const pddl_t *pddl);
static int condQuantEq(const pddl_cond_quant_t *c1,
                       const pddl_cond_quant_t *c2);
static int condQuantTraverse(pddl_cond_quant_t *,
                             int (*pre)(pddl_cond_t *, void *),
                             int (*post)(pddl_cond_t *, void *),
                             void *userdata);
static int condQuantRebuild(pddl_cond_quant_t **q,
                            int (*pre)(pddl_cond_t **, void *),
                            int (*post)(pddl_cond_t **, void *),
                            void *userdata);
static void condQuantPrintPDDL(const pddl_cond_quant_t *q,
                               const pddl_t *pddl,
                               const pddl_params_t *params,
                               FILE *fout);

static void condWhenDel(pddl_cond_when_t *);
static pddl_cond_when_t *condWhenClone(const pddl_cond_when_t *w);
static pddl_cond_when_t *condWhenNegate(const pddl_cond_when_t *w,
                                        const pddl_t *pddl);
static int condWhenEq(const pddl_cond_when_t *c1, const pddl_cond_when_t *c2);
static int condWhenTraverse(pddl_cond_when_t *w,
                            int (*pre)(pddl_cond_t *, void *),
                            int (*post)(pddl_cond_t *, void *),
                            void *userdata);
static int condWhenRebuild(pddl_cond_when_t **w,
                            int (*pre)(pddl_cond_t **, void *),
                            int (*post)(pddl_cond_t **, void *),
                            void *userdata);
static void condWhenPrintPDDL(const pddl_cond_when_t *w,
                              const pddl_t *pddl,
                              const pddl_params_t *params,
                              FILE *fout);

static void condAtomDel(pddl_cond_atom_t *);
static pddl_cond_atom_t *condAtomClone(const pddl_cond_atom_t *a);
static pddl_cond_atom_t *condAtomNegate(const pddl_cond_atom_t *a,
                                        const pddl_t *pddl);
static int condAtomEq(const pddl_cond_atom_t *c1, const pddl_cond_atom_t *c2);
static int condAtomTraverse(pddl_cond_atom_t *,
                            int (*pre)(pddl_cond_t *, void *),
                            int (*post)(pddl_cond_t *, void *),
                            void *userdata);
static int condAtomRebuild(pddl_cond_atom_t **a,
                            int (*pre)(pddl_cond_t **, void *),
                            int (*post)(pddl_cond_t **, void *),
                            void *userdata);
static void condAtomPrintPDDL(const pddl_cond_atom_t *a,
                              const pddl_t *pddl,
                              const pddl_params_t *params,
                              FILE *fout);

static void condFuncOpDel(pddl_cond_func_op_t *);
static pddl_cond_func_op_t *condFuncOpClone(const pddl_cond_func_op_t *);
static pddl_cond_func_op_t *condFuncOpNegate(const pddl_cond_func_op_t *,
                                             const pddl_t *pddl);
static int condFuncOpEq(const pddl_cond_func_op_t *c1,
                        const pddl_cond_func_op_t *c2);
static int condFuncOpTraverse(pddl_cond_func_op_t *,
                              int (*pre)(pddl_cond_t *, void *),
                              int (*post)(pddl_cond_t *, void *),
                              void *userdata);
static int condFuncOpRebuild(pddl_cond_func_op_t **,
                            int (*pre)(pddl_cond_t **, void *),
                            int (*post)(pddl_cond_t **, void *),
                            void *userdata);
static void condFuncOpPrintPDDL(const pddl_cond_func_op_t *,
                                const pddl_t *pddl,
                                const pddl_params_t *params,
                                FILE *fout);

static void condBoolDel(pddl_cond_bool_t *);
static pddl_cond_bool_t *condBoolClone(const pddl_cond_bool_t *a);
static pddl_cond_bool_t *condBoolNegate(const pddl_cond_bool_t *a,
                                        const pddl_t *pddl);
static int condBoolEq(const pddl_cond_bool_t *c1, const pddl_cond_bool_t *c2);
static int condBoolTraverse(pddl_cond_bool_t *,
                            int (*pre)(pddl_cond_t *, void *),
                            int (*post)(pddl_cond_t *, void *),
                            void *userdata);
static int condBoolRebuild(pddl_cond_bool_t **a,
                            int (*pre)(pddl_cond_t **, void *),
                            int (*post)(pddl_cond_t **, void *),
                            void *userdata);
static void condBoolPrintPDDL(const pddl_cond_bool_t *b,
                              const pddl_t *pddl,
                              const pddl_params_t *params,
                              FILE *fout);

static void condImplyDel(pddl_cond_imply_t *);
static pddl_cond_imply_t *condImplyClone(const pddl_cond_imply_t *a);
static pddl_cond_t *condImplyNegate(const pddl_cond_imply_t *a,
                                    const pddl_t *pddl);
static int condImplyEq(const pddl_cond_imply_t *c1,
                       const pddl_cond_imply_t *c2);
static int condImplyTraverse(pddl_cond_imply_t *,
                             int (*pre)(pddl_cond_t *, void *),
                             int (*post)(pddl_cond_t *, void *),
                             void *userdata);
static int condImplyRebuild(pddl_cond_imply_t **a,
                            int (*pre)(pddl_cond_t **, void *),
                            int (*post)(pddl_cond_t **, void *),
                            void *userdata);
static void condImplyPrintPDDL(const pddl_cond_imply_t *b,
                               const pddl_t *pddl,
                               const pddl_params_t *params,
                               FILE *fout);


static pddl_cond_cls_t cond_cls[PDDL_COND_NUM_TYPES] = {
    MCLS(Part),   // PDDL_COND_AND
    MCLS(Part),   // PDDL_COND_OR
    MCLS(Quant),  // PDDL_COND_FORALL
    MCLS(Quant),  // PDDL_COND_EXIST
    MCLS(When),   // PDDL_COND_WHEN
    MCLS(Atom),   // PDDL_COND_ATOM
    MCLS(FuncOp), // PDDL_COND_ASSIGN
    MCLS(FuncOp), // PDDL_COND_INCREASE
    MCLS(Bool),   // PDDL_COND_BOOL
    MCLS(Imply),  // PDDL_COND_IMPLY
};

static pddl_cond_t *parse(const pddl_lisp_node_t *root,
                          const parse_ctx_t *ctx,
                          int negated);

#define condNew(CTYPE, TYPE) \
    (CTYPE *)_condNew(sizeof(CTYPE), TYPE)

static pddl_cond_t *_condNew(int size, unsigned type)
{
    pddl_cond_t *c;

    c = BOR_MALLOC(size);
    bzero(c, size);
    c->type = type;
    borListInit(&c->conn);
    return c;
}


/*** PART ***/
static pddl_cond_part_t *condPartNew(int type)
{
    pddl_cond_part_t *p;
    p = condNew(pddl_cond_part_t, type);
    borListInit(&p->part);
    return p;
}

static void condPartDel(pddl_cond_part_t *p)
{
    bor_list_t *item, *tmp;
    pddl_cond_t *cond;

    BOR_LIST_FOR_EACH_SAFE(&p->part, item, tmp){
        cond = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
        pddlCondDel(cond);
    }

    BOR_FREE(p);
}

static pddl_cond_part_t *condPartClone(const pddl_cond_part_t *p)
{
    pddl_cond_part_t *n;
    pddl_cond_t *c, *nc;
    bor_list_t *item;

    n = condPartNew(p->cls.type);
    BOR_LIST_FOR_EACH(&p->part, item){
        c = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
        nc = pddlCondClone(c);
        borListAppend(&n->part, &nc->conn);
    }
    return n;
}

static pddl_cond_part_t *condPartNegate(const pddl_cond_part_t *p,
                                        const pddl_t *pddl)
{
    pddl_cond_part_t *n;
    pddl_cond_t *c, *nc;
    bor_list_t *item;

    if (p->cls.type == PDDL_COND_AND){
        n = condPartNew(PDDL_COND_OR);
    }else{
        n = condPartNew(PDDL_COND_AND);
    }
    BOR_LIST_FOR_EACH(&p->part, item){
        c = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
        nc = pddlCondNegate(c, pddl);
        borListAppend(&n->part, &nc->conn);
    }
    return n;
}

static int condPartEq(const pddl_cond_part_t *p1,
                      const pddl_cond_part_t *p2)
{
    pddl_cond_t *c1, *c2;
    bor_list_t *item1, *item2;
    BOR_LIST_FOR_EACH(&p1->part, item1){
        c1 = BOR_LIST_ENTRY(item1, pddl_cond_t, conn);
        int found = 0;
        BOR_LIST_FOR_EACH(&p2->part, item2){
            c2 = BOR_LIST_ENTRY(item2, pddl_cond_t, conn);
            if(condEq(c1, c2)){
                found = 1;
                break;
            }
        }
        if (!found)
            return 0;
    }

    return 1;
}

static void condPartAdd(pddl_cond_part_t *p, pddl_cond_t *add)
{
    borListInit(&add->conn);
    borListAppend(&p->part, &add->conn);
}

static int condPartTraverse(pddl_cond_part_t *p,
                            int (*pre)(pddl_cond_t *, void *),
                            int (*post)(pddl_cond_t *, void *),
                            void *u)
{
    pddl_cond_t *c;
    bor_list_t *item, *tmp;

    BOR_LIST_FOR_EACH_SAFE(&p->part, item, tmp){
        c = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
        if (condTraverse(c, pre, post, u) != 0)
            return -1;
    }

    return 0;
}

static int condPartRebuild(pddl_cond_part_t **p,
                           int (*pre)(pddl_cond_t **, void *),
                           int (*post)(pddl_cond_t **, void *),
                           void *u)
{
    pddl_cond_t *c;
    bor_list_t *item, *last;

    if (borListEmpty(&(*p)->part))
        return 0;

    last = borListPrev(&(*p)->part);
    do {
        item = borListNext(&(*p)->part);
        borListDel(item);
        c = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
        if (condRebuild(&c, pre, post, u) != 0)
            return -1;
        borListAppend(&(*p)->part, &c->conn);
    } while (item != last);

    return 0;
}

/** Moves all parts of src to dst */
static void condPartStealPart(pddl_cond_part_t *dst,
                              pddl_cond_part_t *src)
{
    bor_list_t *item;

    while (!borListEmpty(&src->part)){
        item = borListNext(&src->part);
        borListDel(item);
        borListAppend(&dst->part, item);
    }
}

static void condPartPrintPDDL(const pddl_cond_part_t *p,
                              const pddl_t *pddl,
                              const pddl_params_t *params,
                              FILE *fout)
{
    bor_list_t *item;
    const pddl_cond_t *child;


    fprintf(fout, "(");
    if (p->cls.type == PDDL_COND_AND){
        fprintf(fout, "and");
    }else if (p->cls.type == PDDL_COND_OR){
        fprintf(fout, "or");
    }
    BOR_LIST_FOR_EACH(&p->part, item){
        child = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
        //fprintf(fout, "\n        ");
        fprintf(fout, " ");
        pddlCondPrintPDDL(child, pddl, params, fout);
    }
    fprintf(fout, ")");
}


/*** QUANT ***/
static pddl_cond_quant_t *condQuantNew(int type)
{
    return condNew(pddl_cond_quant_t, type);
}

static void condQuantDel(pddl_cond_quant_t *q)
{
    pddlParamsFree(&q->param);
    if (q->cond != NULL)
        pddlCondDel(q->cond);
    BOR_FREE(q);
}

static pddl_cond_quant_t *condQuantClone(const pddl_cond_quant_t *q)
{
    pddl_cond_quant_t *n;

    n = condQuantNew(q->cls.type);
    pddlParamsInitCopy(&n->param, &q->param);
    n->cond = pddlCondClone(q->cond);
    return n;
}

static pddl_cond_quant_t *condQuantNegate(const pddl_cond_quant_t *q,
                                          const pddl_t *pddl)
{
    pddl_cond_quant_t *n;

    if (q->cls.type == PDDL_COND_FORALL){
        n = condQuantNew(PDDL_COND_EXIST);
    }else{
        n = condQuantNew(PDDL_COND_FORALL);
    }
    pddlParamsInitCopy(&n->param, &q->param);
    n->cond = pddlCondNegate(q->cond, pddl);
    return n;
}

static int condQuantEq(const pddl_cond_quant_t *q1,
                       const pddl_cond_quant_t *q2)
{
    if (q1->param.param_size != q2->param.param_size)
        return 0;
    for (int i = 0; i < q1->param.param_size; ++i){
        if (q1->param.param[i].type != q2->param.param[i].type
                || q1->param.param[i].is_agent != q2->param.param[i].is_agent
                || q1->param.param[i].inherit != q2->param.param[i].inherit)
            return 0;
    }
    return condEq(q1->cond, q2->cond);
}

static int condQuantTraverse(pddl_cond_quant_t *q,
                             int (*pre)(pddl_cond_t *, void *),
                             int (*post)(pddl_cond_t *, void *),
                             void *u)
{
    if (q->cond)
        return condTraverse(q->cond, pre, post, u);
    return 0;
}

static int condQuantRebuild(pddl_cond_quant_t **q,
                            int (*pre)(pddl_cond_t **, void *),
                            int (*post)(pddl_cond_t **, void *),
                            void *userdata)
{
    if ((*q)->cond)
        return condRebuild(&(*q)->cond, pre, post, userdata);
    return 0;
}

static void condQuantPrintPDDL(const pddl_cond_quant_t *q,
                               const pddl_t *pddl,
                               const pddl_params_t *params,
                               FILE *fout)
{
    fprintf(fout, "(");
    if (q->cls.type == PDDL_COND_FORALL){
        fprintf(fout, "forall");
    }else if (q->cls.type == PDDL_COND_EXIST){
        fprintf(fout, "exists");
    }

    fprintf(fout, " (");
    pddlParamsPrintPDDL(&q->param, &pddl->type, fout);
    fprintf(fout, ") ");

    pddlCondPrintPDDL(q->cond, pddl, &q->param, fout);

    fprintf(fout, ")");
}




/*** WHEN ***/
static pddl_cond_when_t *condWhenNew(void)
{
    return condNew(pddl_cond_when_t, PDDL_COND_WHEN);
}

static void condWhenDel(pddl_cond_when_t *w)
{
    if (w->pre)
        pddlCondDel(w->pre);
    if (w->eff)
        pddlCondDel(w->eff);
    BOR_FREE(w);
}

static pddl_cond_when_t *condWhenClone(const pddl_cond_when_t *w)
{
    pddl_cond_when_t *n;

    n = condWhenNew();
    if (w->pre)
        n->pre = pddlCondClone(w->pre);
    if (w->eff)
        n->eff = pddlCondClone(w->eff);
    return n;
}

static pddl_cond_when_t *condWhenNegate(const pddl_cond_when_t *w,
                                        const pddl_t *pddl)
{
    fprintf(stderr, "Fatal Error: Cannot negate (when ...)\n");
    exit(-1);
}

static int condWhenEq(const pddl_cond_when_t *w1,
                      const pddl_cond_when_t *w2)
{
    return condEq(w1->pre, w2->pre) && condEq(w1->eff, w2->eff);
}

static int condWhenTraverse(pddl_cond_when_t *w,
                            int (*pre)(pddl_cond_t *, void *),
                            int (*post)(pddl_cond_t *, void *),
                            void *u)
{
    if (condTraverse(w->pre, pre, post, u) != 0)
        return -1;
    if (condTraverse(w->eff, pre, post, u) != 0)
        return -1;
    return 0;
}

static int condWhenRebuild(pddl_cond_when_t **w,
                           int (*pre)(pddl_cond_t **, void *),
                           int (*post)(pddl_cond_t **, void *),
                           void *u)
{
    if ((*w)->pre){
        if (condRebuild(&(*w)->pre, pre, post, u) != 0)
            return -1;
    }
    if ((*w)->eff){
        if (condRebuild(&(*w)->eff, pre, post, u) != 0)
            return -1;
    }
    return 0;
}

static void condWhenPrintPDDL(const pddl_cond_when_t *w,
                              const pddl_t *pddl,
                              const pddl_params_t *params,
                              FILE *fout)
{
    fprintf(fout, "(when ");
    pddlCondPrintPDDL(w->pre, pddl, params, fout);
    fprintf(fout, " ");
    pddlCondPrintPDDL(w->eff, pddl, params, fout);
    fprintf(fout, ")");
}



/*** ATOM ***/
static pddl_cond_atom_t *condAtomNew(void)
{
    return condNew(pddl_cond_atom_t, PDDL_COND_ATOM);
}

static void condAtomDel(pddl_cond_atom_t *a)
{
    if (a->arg != NULL)
        BOR_FREE(a->arg);
    BOR_FREE(a);
}

static pddl_cond_atom_t *condAtomClone(const pddl_cond_atom_t *a)
{
    pddl_cond_atom_t *n;

    n = condAtomNew();
    n->pred = a->pred;
    n->arg_size = a->arg_size;
    n->arg = BOR_ALLOC_ARR(pddl_cond_atom_arg_t, n->arg_size);
    memcpy(n->arg, a->arg, sizeof(pddl_cond_atom_arg_t) * n->arg_size);
    n->neg = a->neg;

    return n;
}

static pddl_cond_atom_t *condAtomNegate(const pddl_cond_atom_t *a,
                                        const pddl_t *pddl)
{
    pddl_cond_atom_t *n;

    n = condAtomClone(a);
    if (pddl->pred.pred[a->pred].neg_of >= 0){
        n->pred = pddl->pred.pred[a->pred].neg_of;
    }else{
        n->neg = !a->neg;
    }
    return n;
}

static int condAtomEq(const pddl_cond_atom_t *a1,
                      const pddl_cond_atom_t *a2)
{
    if (a1->pred != a2->pred
            || a1->neg != a2->neg
            || a1->arg_size != a2->arg_size)
        return 0;
    for (int i = 0; i < a1->arg_size; ++i){
        if (a1->arg[i].param != a2->arg[i].param
                || a1->arg[i].obj != a2->arg[i].obj){
            return 0;
        }
    }
    return 1;
}

static int condAtomTraverse(pddl_cond_atom_t *a,
                            int (*pre)(pddl_cond_t *, void *),
                            int (*post)(pddl_cond_t *, void *),
                            void *u)
{
    return 0;
}

static int condAtomRebuild(pddl_cond_atom_t **a,
                           int (*pre)(pddl_cond_t **, void *),
                           int (*post)(pddl_cond_t **, void *),
                           void *u)
{
    return 0;
}

static void atomPrintPDDL(const pddl_cond_atom_t *a,
                          const pddl_t *pddl,
                          const pddl_params_t *params,
                          int is_func,
                          FILE *fout)
{
    if (a->neg)
        fprintf(fout, "(not ");
    if (is_func){
        fprintf(fout, "(%s", pddl->func.pred[a->pred].name);
    }else{
        fprintf(fout, "(%s", pddl->pred.pred[a->pred].name);
    }
    for (int i = 0; i < a->arg_size; ++i){
        pddl_cond_atom_arg_t *arg = a->arg + i;
        if (arg->param >= 0){
            fprintf(fout, " %s", params->param[arg->param].name);
        }else{
            fprintf(fout, " %s", pddl->obj.obj[arg->obj].name);
        }
    }
    fprintf(fout, ")");
    if (a->neg)
        fprintf(fout, ")");
}

static void condAtomPrintPDDL(const pddl_cond_atom_t *a,
                              const pddl_t *pddl,
                              const pddl_params_t *params,
                              FILE *fout)
{
    atomPrintPDDL(a, pddl, params, 0, fout);
}



/*** FUNC_OP ***/
static pddl_cond_func_op_t *condFuncOpNew(int type)
{
    return condNew(pddl_cond_func_op_t, type);
}

static void condFuncOpDel(pddl_cond_func_op_t *op)
{
    if (op->lvalue)
        condAtomDel(op->lvalue);
    if (op->fvalue)
        condAtomDel(op->fvalue);
    BOR_FREE(op);
}

static pddl_cond_func_op_t *condFuncOpClone(const pddl_cond_func_op_t *op)
{
    pddl_cond_func_op_t *n;
    n = condFuncOpNew(op->cls.type);
    n->value = op->value;
    if (op->lvalue)
        n->lvalue = condAtomClone(op->lvalue);
    if (op->fvalue)
        n->fvalue = condAtomClone(op->fvalue);
    return n;
}

static pddl_cond_func_op_t *condFuncOpNegate(const pddl_cond_func_op_t *op,
                                             const pddl_t *pddl)
{
    fprintf(stderr, "Fatal Error: Cannot negate function!\n");
    exit(-1);
}

static int condFuncOpEq(const pddl_cond_func_op_t *f1,
                        const pddl_cond_func_op_t *f2)
{
    if (f1->fvalue == NULL && f2->fvalue != NULL)
        return 0;
    if (f1->fvalue != NULL && f2->fvalue == NULL)
        return 0;
    if (f1->fvalue != NULL){
        return condAtomEq(f1->lvalue, f2->lvalue)
                    && condAtomEq(f1->fvalue, f2->fvalue);
    }else{
        return f1->value == f2->value && condAtomEq(f1->lvalue, f2->lvalue);
    }
}

static int condFuncOpTraverse(pddl_cond_func_op_t *op,
                              int (*pre)(pddl_cond_t *, void *),
                              int (*post)(pddl_cond_t *, void *),
                              void *u)
{
    return 0;
}

static int condFuncOpRebuild(pddl_cond_func_op_t **op,
                             int (*pre)(pddl_cond_t **, void *),
                             int (*post)(pddl_cond_t **, void *),
                             void *userdata)
{
    return 0;
}

static void condFuncOpPrintPDDL(const pddl_cond_func_op_t *op,
                                const pddl_t *pddl,
                                const pddl_params_t *params,
                                FILE *fout)
{
    if (op->cls.type == PDDL_COND_ASSIGN){
        fprintf(fout, "(= ");
    }else if (op->cls.type == PDDL_COND_INCREASE){
        fprintf(fout, "(increase ");
    }

    if (op->lvalue == NULL){
        fprintf(fout, "(total-cost)");
    }else{
        atomPrintPDDL(op->lvalue, pddl, params, 1, fout);
    }
    fprintf(fout, " ");
    if (op->fvalue == NULL){
        fprintf(fout, "%d", op->value);
    }else{
        atomPrintPDDL(op->fvalue, pddl, params, 1, fout);
    }
    fprintf(fout, ")");
}


/*** BOOL ***/
static pddl_cond_bool_t *condBoolNew(int val)
{
    pddl_cond_bool_t *b;
    b = condNew(pddl_cond_bool_t, PDDL_COND_BOOL);
    b->val = val;
    return b;
}

static void condBoolDel(pddl_cond_bool_t *a)
{
    BOR_FREE(a);
}

static pddl_cond_bool_t *condBoolClone(const pddl_cond_bool_t *a)
{
    return condBoolNew(a->val);
}

static pddl_cond_bool_t *condBoolNegate(const pddl_cond_bool_t *a,
                                        const pddl_t *pddl)
{
    pddl_cond_bool_t *b = condBoolClone(a);
    b->val = !a->val;
    return b;
}

static int condBoolEq(const pddl_cond_bool_t *b1,
                      const pddl_cond_bool_t *b2)
{
    return b1->val == b2->val;
}


static int condBoolTraverse(pddl_cond_bool_t *a,
                            int (*pre)(pddl_cond_t *, void *),
                            int (*post)(pddl_cond_t *, void *),
                            void *u)
{
    return 0;
}

static int condBoolRebuild(pddl_cond_bool_t **a,
                           int (*pre)(pddl_cond_t **, void *),
                           int (*post)(pddl_cond_t **, void *),
                           void *userdata)
{
    return 0;
}

static void condBoolPrintPDDL(const pddl_cond_bool_t *b,
                              const pddl_t *pddl,
                              const pddl_params_t *params,
                              FILE *fout)
{
    if (b->val){
        fprintf(fout, "(TRUE)");
    }else{
        fprintf(fout, "(FALSE)");
    }
}


/*** IMPLY ***/
static pddl_cond_imply_t *condImplyNew(void)
{
    return condNew(pddl_cond_imply_t, PDDL_COND_IMPLY);
}

static void condImplyDel(pddl_cond_imply_t *a)
{
    if (a->left != NULL)
        pddlCondDel(a->left);
    if (a->right != NULL)
        pddlCondDel(a->right);
    BOR_FREE(a);
}

static pddl_cond_imply_t *condImplyClone(const pddl_cond_imply_t *a)
{
    pddl_cond_imply_t *n = condImplyNew();
    if (a->left != NULL)
        n->left = pddlCondClone(a->left);
    if (a->right != NULL)
        n->right = pddlCondClone(a->right);
    return n;
}

static pddl_cond_t *condImplyNegate(const pddl_cond_imply_t *a,
                                    const pddl_t *pddl)
{
    pddl_cond_part_t *or;
    pddl_cond_t *left, *right;

    or = condPartNew(PDDL_COND_AND);
    left = pddlCondClone(a->left);
    right = pddlCondNegate(a->right, pddl);
    pddlCondPartAdd(or, left);
    pddlCondPartAdd(or, right);
    return &or->cls;
}

static int condImplyEq(const pddl_cond_imply_t *i1,
                       const pddl_cond_imply_t *i2)
{
    return condEq(i1->left, i2->left) && condEq(i1->right, i2->right);
}

static int condImplyTraverse(pddl_cond_imply_t *imp,
                            int (*pre)(pddl_cond_t *, void *),
                            int (*post)(pddl_cond_t *, void *),
                            void *u)
{
    if (imp->left != NULL){
        if (condTraverse(imp->left, pre, post, u) != 0)
            return -1;
    }
    if (imp->right != NULL){
        if (condTraverse(imp->right, pre, post, u) != 0)
            return -1;
    }
    return 0;
}

static int condImplyRebuild(pddl_cond_imply_t **imp,
                           int (*pre)(pddl_cond_t **, void *),
                           int (*post)(pddl_cond_t **, void *),
                           void *u)
{
    if ((*imp)->left != NULL){
        if (condRebuild(&(*imp)->left, pre, post, u) != 0)
            return -1;
    }
    if ((*imp)->right != NULL){
        if (condRebuild(&(*imp)->right, pre, post, u) != 0)
            return -1;
    }
    return 0;
}

static void condImplyPrintPDDL(const pddl_cond_imply_t *imp,
                               const pddl_t *pddl,
                               const pddl_params_t *params,
                               FILE *fout)
{
    fprintf(fout, "(imply ");
    if (imp->left == NULL){
        fprintf(fout, "()");
    }else{
        pddlCondPrintPDDL(imp->left, pddl, params, fout);
    }
    fprintf(fout, " ");
    if (imp->right == NULL){
        fprintf(fout, "()");
    }else{
        pddlCondPrintPDDL(imp->right, pddl, params, fout);
    }
    fprintf(fout, ")");
}




void pddlCondDel(pddl_cond_t *cond)
{
    cond_cls[cond->type].del(cond);
}

pddl_cond_t *pddlCondClone(const pddl_cond_t *cond)
{
    return cond_cls[cond->type].clone(cond);
}

pddl_cond_t *pddlCondNegate(const pddl_cond_t *cond,
                            const pddl_t *pddl)
{
    return cond_cls[cond->type].negate(cond, pddl);
}

static int condEq(const pddl_cond_t *c1, const pddl_cond_t *c2)
{
    if (c1 == c2)
        return 1;
    if ((c1 == NULL && c2 != NULL)
            || (c1 != NULL && c2 == NULL))
        return 0;
    if (c1->type != c2->type)
        return 0;
    return cond_cls[c1->type].eq(c1, c2);
}

int pddlCondEq(const pddl_cond_t *c1, const pddl_cond_t *c2)
{
    return condEq(c1, c2);
}

static int condTraverse(pddl_cond_t *c,
                        int (*pre)(pddl_cond_t *, void *),
                        int (*post)(pddl_cond_t *, void *),
                        void *u)
{
    int ret;

    if (pre != NULL){
        ret = pre(c, u);
        if (ret == -1)
            return 0;
        if (ret == -2)
            return -1;
    }

    ret = cond_cls[c->type].traverse(c, pre, post, u);
    if (ret < 0)
        return ret;

    if (post != NULL)
        if (post(c, u) != 0)
            return -1;
    return 0;
}

void pddlCondTraverse(pddl_cond_t *c,
                      int (*pre)(pddl_cond_t *, void *),
                      int (*post)(pddl_cond_t *, void *),
                      void *u)
{
    condTraverse(c, pre, post, u);
}

static int condRebuild(pddl_cond_t **c,
                       int (*pre)(pddl_cond_t **, void *),
                       int (*post)(pddl_cond_t **, void *),
                       void *u)
{
    int ret;

    if (pre != NULL){
        ret = pre(c, u);
        if (ret == -1)
            return 0;
        if (ret == -2)
            return -1;
    }

    ret = cond_cls[(*c)->type].rebuild(c, pre, post, u);
    if (ret < 0)
        return ret;

    if (post != NULL)
        if (post(c, u) != 0)
            return -1;
    return 0;
}

void pddlCondRebuild(pddl_cond_t **c,
                     int (*pre)(pddl_cond_t **, void *),
                     int (*post)(pddl_cond_t **, void *),
                     void *u)
{
    condRebuild(c, pre, post, u);
}

struct test_static {
    const pddl_t *pddl;
    int ret;
};
static int atomIsStatic(pddl_cond_t *c, void *_ts)
{
    struct test_static *ts = _ts;
    if (c->type == PDDL_COND_ATOM){
        const pddl_cond_atom_t *a = OBJ(c, atom);
        if (!pddlPredIsStatic(ts->pddl->pred.pred + a->pred)){
            ts->ret = 0;
            return -2;
        }
        return 0;
    }
    return 0;
}

static int pddlCondIsStatic(pddl_cond_t *c, const pddl_t *pddl)
{
    struct test_static ts;
    ts.pddl = pddl;
    ts.ret = 1;

    pddlCondTraverse(c, atomIsStatic, NULL, &ts);
    return ts.ret;
}

pddl_cond_when_t *pddlCondRemoveFirstNonStaticWhen(pddl_cond_t *c,
                                                   const pddl_t *pddl)
{
    pddl_cond_part_t *cp;
    pddl_cond_t *cw;
    bor_list_t *item, *tmp;

    if (c->type != PDDL_COND_AND)
        return NULL;
    cp = PDDL_COND_CAST(c, part);

    BOR_LIST_FOR_EACH_SAFE(&cp->part, item, tmp){
        cw = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
        if (cw->type == PDDL_COND_WHEN){
            pddl_cond_when_t *w = PDDL_COND_CAST(cw, when);
            if (!pddlCondIsStatic(w->pre, pddl)){
                borListDel(item);
                return w;
            }
        }
    }

    return NULL;
}

pddl_cond_when_t *pddlCondRemoveFirstWhen(pddl_cond_t *c, const pddl_t *pddl)
{
    pddl_cond_part_t *cp;
    pddl_cond_t *cw;
    bor_list_t *item, *tmp;

    if (c->type != PDDL_COND_AND)
        return NULL;
    cp = PDDL_COND_CAST(c, part);

    BOR_LIST_FOR_EACH_SAFE(&cp->part, item, tmp){
        cw = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
        if (cw->type == PDDL_COND_WHEN){
            pddl_cond_when_t *w = PDDL_COND_CAST(cw, when);
            borListDel(item);
            return w;
        }
    }

    return NULL;
}

pddl_cond_t *pddlCondNewAnd2(pddl_cond_t *a, pddl_cond_t *b)
{
    pddl_cond_part_t *p = condPartNew(PDDL_COND_AND);
    condPartAdd(p, a);
    condPartAdd(p, b);
    return &p->cls;
}

pddl_cond_t *pddlCondNewEmptyAnd(void)
{
    pddl_cond_part_t *p = condPartNew(PDDL_COND_AND);
    return &p->cls;
}

pddl_cond_atom_t *pddlCondNewEmptyAtom(int num_args)
{
    pddl_cond_atom_t *atom = condAtomNew();

    if (num_args > 0){
        atom->arg_size = num_args;
        atom->arg = BOR_ALLOC_ARR(pddl_cond_atom_arg_t, atom->arg_size);
        for (int i = 0; i < atom->arg_size; ++i){
            atom->arg[i].param = -1;
            atom->arg[i].obj = PDDL_OBJ_ID_UNDEF;
        }
    }

    return atom;
}

static int hasAtom(pddl_cond_t *c, void *_ret)
{
    int *ret = _ret;

    if (c->type == PDDL_COND_ATOM){
        *ret = 1;
        return -2;
    }
    return 0;
}

int pddlCondHasAtom(const pddl_cond_t *c)
{
    int ret = 0;
    pddlCondTraverse((pddl_cond_t *)c, hasAtom, NULL, &ret);
    return ret;
}

/*** PARSE ***/
static int parseAtomArg(pddl_cond_atom_arg_t *arg,
                        const pddl_lisp_node_t *root,
                        const parse_ctx_t *ctx)
{
    if (root->value[0] == '?'){
        if (ctx->params == NULL){
            ERR_LISP_RET(ctx->err, -1, root, "%sUnexpected variable `%s'",
                         ctx->err_prefix, root->value);
        }

        int param = pddlParamsGetId(ctx->params, root->value);
        if (param < 0){
            ERR_LISP_RET(ctx->err, -1, root, "%sUnkown variable `%s'",
                         ctx->err_prefix, root->value);
        }
        arg->param = param;
        arg->obj = PDDL_OBJ_ID_UNDEF;

    }else{
        pddl_obj_id_t obj = pddlObjsGet(ctx->objs, root->value);
        if (obj < 0){
            ERR_LISP_RET(ctx->err, -1, root, "%sUnkown constant/object `%s'",
                         ctx->err_prefix, root->value);
        }
        arg->param = -1;
        arg->obj = obj;
    }

    return 0;
}

static pddl_cond_t *parseAtom(const pddl_lisp_node_t *root,
                              const parse_ctx_t *ctx,
                              int negated)
{
    pddl_cond_atom_t *atom;
    const char *name;
    int pred;

    // Get predicate name
    name = pddlLispNodeHead(root);
    if (name == NULL){
        ERR_LISP_RET(ctx->err, NULL, root,
                     "%sMissing head of the expression", ctx->err_prefix);
    }

    // And resolve it against known predicates
    pred = pddlPredsGet(ctx->preds, name);
    if (pred == -1){
        ERR_LISP_RET(ctx->err, NULL, root,
                     "%sUnkown predicate `%s'", ctx->err_prefix, name);
    }

    // Check correct number of predicates
    if (root->child_size - 1 != ctx->preds->pred[pred].param_size){
        ERR_LISP_RET(ctx->err, NULL, root,
                     "%sInvalid number of arguments of the predicate `%s'",
                     ctx->err_prefix, name);
    }

    // Check that all children are terminals
    for (int i = 1; i < root->child_size; ++i){
        if (root->child[i].value == NULL){
            ERR_LISP_RET(ctx->err, NULL, root->child + i,
                         "%sInvalid %d'th argument of the predicate `%s'",
                         ctx->err_prefix, i, name);
        }
    }

    atom = condAtomNew();
    atom->pred = pred;
    atom->arg_size = root->child_size - 1;
    atom->arg = BOR_ALLOC_ARR(pddl_cond_atom_arg_t, atom->arg_size);
    for (int i = 0; i < atom->arg_size; ++i){
        if (parseAtomArg(atom->arg + i, root->child + i + 1, ctx) != 0){
            condAtomDel(atom);
            BOR_TRACE_RET(ctx->err, NULL);
        }
    }
    atom->neg = negated;

    return &atom->cls;
}

static pddl_cond_t *parseAssign(const pddl_lisp_node_t *root,
                                const parse_ctx_t *ctx,
                                int negated)
{
    const char *head;
    const pddl_lisp_node_t *nfunc, *nval;
    pddl_cond_t *lvalue;
    pddl_cond_func_op_t *assign;
    parse_ctx_t sub_ctx;

    head = pddlLispNodeHead(root);
    if (head == NULL
            || strcmp(head, "=") != 0
            || root->child_size != 3){
        ERR_LISP_RET2(ctx->err, NULL, root, "Invalid (= ...) expression.");
    }

    nfunc = root->child + 1;
    nval = root->child + 2;

    if (nfunc->child_size < 1 || nfunc->child[0].value == NULL)
        ERR_LISP_RET2(ctx->err, NULL, root, "Invalid function in (= ...).");
    if (nval->value == NULL){
        ERR_LISP_RET2(ctx->err, NULL, root, "Only (= ... N) expressions where"
                                            " N is a number are supported.");
    }

    sub_ctx = *ctx;
    sub_ctx.preds = sub_ctx.funcs;
    lvalue = parseAtom(nfunc, &sub_ctx, negated);
    if (lvalue == NULL)
        BOR_TRACE_RET(ctx->err, NULL);

    assign = condFuncOpNew(PDDL_COND_ASSIGN);
    assign->value = atoi(nval->value);
    assign->lvalue = OBJ(lvalue, atom);
    return &assign->cls;
}

static pddl_cond_t *parseIncrease(const pddl_lisp_node_t *root,
                                  const parse_ctx_t *ctx,
                                  int negated)
{
    pddl_cond_func_op_t *inc;
    pddl_cond_t *fvalue;
    parse_ctx_t sub_ctx;

    if (root->child_size != 3
            || root->child[1].value != NULL
            || root->child[1].child_size != 1
            || root->child[1].child[0].value == NULL
            || strcmp(root->child[1].child[0].value, "total-cost") != 0){
        ERR_LISP_RET(ctx->err, NULL, root,
                     "%sOnly (increase (total-cost) int-value) is supported;",
                     ctx->err_prefix);
    }

    if (root->child[2].value != NULL){
        inc = condFuncOpNew(PDDL_COND_INCREASE);
        inc->value = atoi(root->child[2].value);
        if (inc->value < 0){
            ERR_LISP_RET(ctx->err, NULL, root,
                         "%sOnly non-negative actions costs are supported;",
                         ctx->err_prefix);
        }

    }else{
        sub_ctx = *ctx;
        sub_ctx.preds = sub_ctx.funcs;
        fvalue = parseAtom(root->child + 2, &sub_ctx, negated);
        if (fvalue == NULL)
            BOR_TRACE_RET(ctx->err, NULL);
        inc = condFuncOpNew(PDDL_COND_INCREASE);
        inc->fvalue = (pddl_cond_atom_t *)fvalue;
    }

    return &inc->cls;
}

static pddl_cond_t *parsePart(int part_type,
                              const pddl_lisp_node_t *root,
                              const parse_ctx_t *ctx,
                              int negated)
{
    pddl_cond_part_t *part;
    pddl_cond_t *cond;
    int i;

    part = condPartNew(part_type);
    for (i = 1; i < root->child_size; ++i){
        cond = parse(root->child + i, ctx, negated);
        if (cond == NULL){
            condPartDel(part);
            BOR_TRACE_RET(ctx->err, NULL);
        }
        borListAppend(&part->part, &cond->conn);
    }

    return &part->cls;
}

static pddl_cond_t *parseImply(const pddl_lisp_node_t *left,
                               const pddl_lisp_node_t *right,
                               const parse_ctx_t *ctx,
                               int negated)
{
    pddl_cond_part_t *part;
    pddl_cond_imply_t *imp;
    pddl_cond_t *cleft = NULL, *cright = NULL;

    if (negated){
        if ((cleft = parse(left, ctx, 0)) == NULL)
            BOR_TRACE_RET(ctx->err, NULL);

        if ((cright = parse(right, ctx, 1)) == NULL){
            pddlCondDel(cleft);
            BOR_TRACE_RET(ctx->err, NULL);
        }

        part = condPartNew(PDDL_COND_AND);
        borListAppend(&part->part, &cleft->conn);
        borListAppend(&part->part, &cright->conn);
        return &part->cls;

    }else{
        if ((cleft = parse(left, ctx, 0)) == NULL)
            BOR_TRACE_RET(ctx->err, NULL);

        if ((cright = parse(right, ctx, 0)) == NULL){
            pddlCondDel(cleft);
            BOR_TRACE_RET(ctx->err, NULL);
        }

        imp = condImplyNew();
        imp->left = cleft;
        imp->right = cright;
        return &imp->cls;
    }
}

static int parseQuantParams(pddl_params_t *params,
                            const pddl_lisp_node_t *root,
                            const parse_ctx_t *ctx)
{
    pddl_param_t *param;

    pddlParamsInit(params);

    // Parse all parameters of the quantifier
    if (pddlParamsParse(params, root, ctx->types, ctx->err) != 0){
        pddlParamsFree(params);
        BOR_TRACE_RET(ctx->err, -1);
    }

    // And also add all global parameters that are not shadowed
    for (int i = 0; ctx->params != NULL && i < ctx->params->param_size; ++i){
        int use = 1;
        for (int j = 0; j < params->param_size; ++j){
            if (strcmp(params->param[j].name, ctx->params->param[i].name) == 0){
                use = 0;
                break;
            }
        }

        if (use){
            param = pddlParamsAdd(params);
            pddlParamInitCopy(param, ctx->params->param + i);
            param->inherit = i;
        }
    }

    return 0;
}

static pddl_cond_t *parseQuant(int quant_type,
                               const pddl_lisp_node_t *root,
                               const parse_ctx_t *ctx,
                               int negated)
{
    pddl_cond_quant_t *q;
    pddl_params_t params;
    pddl_cond_t *cond;
    parse_ctx_t sub_ctx;

    if (root->child_size != 3
            || root->child[1].value != NULL
            || root->child[2].value != NULL){
        if (quant_type == PDDL_COND_FORALL){
            ERR_LISP(ctx->err, root,
                     "%sInvalid (forall ...) condition", ctx->err_prefix);
        }else{
            ERR_LISP(ctx->err, root,
                     "%sInvalid (exists ...) condition", ctx->err_prefix);
        }
        return NULL;
    }

    if (parseQuantParams(&params, root->child + 1, ctx) != 0)
        BOR_TRACE_RET(ctx->err, NULL);

    if (params.param_size == 0){
        pddlParamsFree(&params);
        ERR_LISP_RET(ctx->err, NULL, root,
                     "%sMissing variables in the quantifier",
                     ctx->err_prefix);
    }

    sub_ctx = *ctx;
    sub_ctx.params = &params;
    cond = parse(root->child + 2, &sub_ctx, negated);
    if (cond == NULL){
        pddlParamsFree(&params);
        BOR_TRACE_RET(ctx->err, NULL);
    }

    q = condQuantNew(quant_type);
    q->param = params;
    q->cond = cond;

    return &q->cls;
}

static pddl_cond_t *parseWhen(const pddl_lisp_node_t *root,
                              const parse_ctx_t *ctx)
{
    pddl_cond_when_t *w;
    pddl_cond_t *pre, *eff;

    if (root->child_size != 3
            || root->child[1].value != NULL
            || root->child[2].value != NULL){
        ERR_LISP_RET(ctx->err, NULL, root,
                     "%sInvalid (when ...)", ctx->err_prefix);
    }

    if ((pre = parse(root->child + 1, ctx, 0)) == NULL)
        BOR_TRACE_RET(ctx->err, NULL);

    if ((eff = parse(root->child + 2, ctx, 0)) == NULL){
        pddlCondDel(pre);
        BOR_TRACE_RET(ctx->err, NULL);
    }

    w = condWhenNew();
    w->pre = pre;
    w->eff = eff;
    return &w->cls;
}

static pddl_cond_t *parse(const pddl_lisp_node_t *root,
                          const parse_ctx_t *ctx,
                          int negated)
{
    int kw;

    kw = pddlLispNodeHeadKw(root);

    if (kw == PDDL_KW_NOT){
        if (root->child_size != 2)
            ERR_LISP_RET(ctx->err, NULL, root,
                         "%sInvalid (not ...)", ctx->err_prefix);

        return parse(root->child + 1, ctx, !negated);

    }else if (kw == PDDL_KW_AND){
        if (root->child_size <= 1)
            ERR_LISP_RET(ctx->err, NULL, root,
                         "%sEmpty (and) expression", ctx->err_prefix);

        if (negated){
            return parsePart(PDDL_COND_OR, root, ctx, negated);
        }else{
            return parsePart(PDDL_COND_AND, root, ctx, negated);
        }

    }else if (kw == PDDL_KW_OR){
        if (root->child_size <= 1)
            ERR_LISP_RET(ctx->err, NULL, root,
                         "%sEmpty (or) expression", ctx->err_prefix);

        if (negated){
            return parsePart(PDDL_COND_AND, root, ctx, negated);
        }else{
            return parsePart(PDDL_COND_OR, root, ctx, negated);
        }

    }else if (kw == PDDL_KW_IMPLY){
        if (root->child_size != 3)
            ERR_LISP_RET(ctx->err, NULL, root,
                         "%s(imply ...) requires two arguments",
                         ctx->err_prefix);

        return parseImply(root->child + 1, root->child + 2, ctx, negated);

    }else if (kw == PDDL_KW_FORALL){
        // TODO: :conditional-effects || :universal-preconditions
        if (negated){
            return parseQuant(PDDL_COND_EXIST, root, ctx, negated);
        }else{
            return parseQuant(PDDL_COND_FORALL, root, ctx, negated);
        }

    }else if (kw == PDDL_KW_EXISTS){
        // TODO: :existential-preconditions
        if (negated){
            return parseQuant(PDDL_COND_FORALL, root, ctx, negated);
        }else{
            return parseQuant(PDDL_COND_EXIST, root, ctx, negated);
        }

    }else if (kw == PDDL_KW_WHEN){
        // Conditional effect cannot be negated
        return parseWhen(root, ctx);

    }else if (kw == PDDL_KW_INCREASE){
        return parseIncrease(root, ctx, negated);

    }else if (kw == -1){
        return parseAtom(root, ctx, negated);
    }

    if (root->child_size >= 1 && root->child[0].value != NULL){
        ERR_LISP_RET(ctx->err, NULL, root, "%sUnexpected token `%s'",
                     ctx->err_prefix, root->child[0].value);
    }else{
        ERR_LISP_RET(ctx->err, NULL, root,
                     "%sUnexpected token", ctx->err_prefix);
    }
}

pddl_cond_t *pddlCondParse(const pddl_lisp_node_t *root,
                           pddl_t *pddl,
                           const pddl_params_t *params,
                           const char *err_prefix,
                           bor_err_t *err)
{
    parse_ctx_t ctx;
    pddl_cond_t *c;

    ctx.types = &pddl->type;
    ctx.objs = &pddl->obj;
    ctx.preds = &pddl->pred;
    ctx.funcs = &pddl->func;
    ctx.params = params;
    ctx.err_prefix = err_prefix;
    ctx.err = err;

    c = parse(root, &ctx, 0);
    if (c == NULL)
        BOR_TRACE_RET(err, NULL);
    return c;
}

static pddl_cond_t *parseInitFunc(const pddl_lisp_node_t *n, pddl_t *pddl,
                                  bor_err_t *err)
{
    parse_ctx_t ctx;
    pddl_params_t params;
    pddl_cond_t *c;

    pddlParamsInit(&params);
    ctx.types = &pddl->type;
    ctx.objs = &pddl->obj;
    ctx.preds = &pddl->pred;
    ctx.funcs = &pddl->func;
    ctx.params = &params;
    ctx.err_prefix = "";
    ctx.err = err;

    c = parseAssign(n, &ctx, 0);
    pddlParamsFree(&params);

    if (c == NULL)
        BOR_TRACE_RET(err, NULL);
    return c;
}

static pddl_cond_t *parseInitFact(const pddl_lisp_node_t *n, pddl_t *pddl,
                                  bor_err_t *err)
{
    parse_ctx_t ctx;
    pddl_params_t params;
    pddl_cond_t *c;

    pddlParamsInit(&params);
    ctx.types = &pddl->type;
    ctx.objs = &pddl->obj;
    ctx.preds = &pddl->pred;
    ctx.funcs = &pddl->func;
    ctx.params = &params;
    ctx.err_prefix = "";
    ctx.err = err;

    c = parseAtom(n, &ctx, 0);
    pddlParamsFree(&params);

    if (c == NULL)
        BOR_TRACE_RET(err, NULL);
    return c;
}

static pddl_cond_t *parseInitFactFunc(const pddl_lisp_node_t *n, pddl_t *pddl,
                                      bor_err_t *err)
{
    const char *head;

    if (n->child_size < 1)
        ERR_LISP_RET2(err, NULL, n, "Invalid expression in :init.");

    head = pddlLispNodeHead(n);
    if (head == NULL)
        ERR_LISP_RET2(err, NULL, n, "Invalid expression in :init.");
    if (strcmp(head, "=") == 0
            && n->child_size == 3
            && n->child[1].value == NULL){
        return parseInitFunc(n, pddl, err);
    }else{
        return parseInitFact(n, pddl, err);
    }
}

pddl_cond_part_t *pddlCondParseInit(const pddl_lisp_node_t *root, pddl_t *pddl,
                                    bor_err_t *err)
{
    const pddl_lisp_node_t *n;
    pddl_cond_part_t *and;
    pddl_cond_t *c;

    and = condPartNew(PDDL_COND_AND);

    for (int i = 1; i < root->child_size; ++i){
        n = root->child + i;
        if ((c = parseInitFactFunc(n, pddl, err)) == NULL){
            condPartDel(and);
            BOR_TRACE_PREPEND_RET(err, NULL, "While parsing :init in %s: ",
                                  pddl->problem_lisp->filename);
        }
        condPartAdd(and, c);
    }

    return and;
}

pddl_cond_t *pddlCondAtomToAnd(pddl_cond_t *atom)
{
    pddl_cond_part_t *and;

    and = condPartNew(PDDL_COND_AND);
    condPartAdd(and, atom);
    return &and->cls;
}

pddl_cond_atom_t *pddlCondCreateFactAtom(int pred, int arg_size, 
                                         const pddl_obj_id_t *arg)
{
    pddl_cond_atom_t *a;

    a = condAtomNew();
    a->pred = pred;
    a->arg_size = arg_size;
    a->arg = BOR_ALLOC_ARR(pddl_cond_atom_arg_t, arg_size);
    for (int i = 0; i < arg_size; ++i){
        a->arg[i].param = -1;
        a->arg[i].obj = arg[i];
    }
    return a;
}

void pddlCondPartAdd(pddl_cond_part_t *part, pddl_cond_t *c)
{
    condPartAdd(part, c);
}

void pddlCondPartRm(pddl_cond_part_t *part, pddl_cond_t *c)
{
    borListDel(&c->conn);
}


/*** CHECK ***/
int pddlCondCheckPre(const pddl_cond_t *cond, int require, bor_err_t *err)
{
    pddl_cond_part_t *p;
    pddl_cond_quant_t *q;
    pddl_cond_atom_t *atom;
    pddl_cond_imply_t *imp;
    pddl_cond_t *c;
    bor_list_t *item;

    if (cond->type == PDDL_COND_AND
            || cond->type == PDDL_COND_OR){
        if (cond->type == PDDL_COND_OR
                && !(require & PDDL_REQUIRE_DISJUNCTIVE_PRE)){
            BOR_ERR2(err, "(or ...) can be used only with"
                     " :disjunctive-preconditions");
            return -1;
        }

        p = OBJ(cond, part);
        BOR_LIST_FOR_EACH(&p->part, item){
            c = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
            if (pddlCondCheckPre(c, require, err) != 0)
                BOR_TRACE_RET(err, -1);
        }

        return 0;

    }else if (cond->type == PDDL_COND_FORALL){
        if (!(require & PDDL_REQUIRE_UNIVERSAL_PRE)){
            BOR_ERR2(err, "(forall ...) can be used only with"
                     " :universal-preconditions");
            return -1;
        }

        q = OBJ(cond, quant);
        return pddlCondCheckPre(q->cond, require, err);

    }else if (cond->type == PDDL_COND_EXIST){
        if (!(require & PDDL_REQUIRE_EXISTENTIAL_PRE)){
            BOR_ERR2(err, "(exists ...) can be used only with"
                     " :existential-preconditions");
            return -1;
        }

        q = OBJ(cond, quant);
        return pddlCondCheckPre(q->cond, require, err);

    }else if (cond->type == PDDL_COND_WHEN){
        BOR_ERR2(err, "(when ...) cannot be part of preconditions");
        return -1;

    }else if (cond->type == PDDL_COND_ATOM){
        atom = OBJ(cond, atom);
        if (atom->neg && !(require & PDDL_REQUIRE_NEGATIVE_PRE)){
            BOR_ERR2(err, "For negative preconditions add"
                          " :negative-preconditions");
            return -1;
        }

        return 0;

    }else if (cond->type == PDDL_COND_IMPLY){
        imp = OBJ(cond, imply);
        if (!(require & PDDL_REQUIRE_DISJUNCTIVE_PRE)){
            BOR_ERR2(err, "(imply ...) can be used only with"
                     " :disjunctive-preconditions");
            return -1;
        }

        if (pddlCondCheckPre(imp->left, require, err) != 0)
            return -1;
        if (pddlCondCheckPre(imp->right, require, err) != 0)
            return -1;
        return 0;

    }else if (cond->type == PDDL_COND_ASSIGN
                || cond->type == PDDL_COND_INCREASE){
        return 0;
    }

    return -1;
}


static int checkCEffect(const pddl_cond_t *cond, int require, bor_err_t *err);
static int checkPEffect(const pddl_cond_t *cond, int require, bor_err_t *err);
static int checkCondEffect(const pddl_cond_t *cond, int require,
                           bor_err_t *err);

static int checkCEffect(const pddl_cond_t *cond, int require, bor_err_t *err)
{
    pddl_cond_quant_t *forall;
    pddl_cond_when_t *when;

    if (cond->type == PDDL_COND_FORALL){
        if (!(require & PDDL_REQUIRE_CONDITIONAL_EFF)){
            BOR_ERR2(err, "(forall ...) is allowed in effects only if"
                     " :conditional-effects is specified as requirement");
            return -1;
        }

        forall = OBJ(cond, quant);
        return pddlCondCheckEff(forall->cond, require, err);

    }else if (cond->type == PDDL_COND_WHEN){
        if (!(require & PDDL_REQUIRE_CONDITIONAL_EFF)){
            BOR_ERR2(err, "(when ...) is allowed in effects only if"
                     " :conditional-effects is specified as requirement");
            return -1;
        }

        when = OBJ(cond, when);
        if (pddlCondCheckPre(when->pre, require, err) != 0)
            return -1;
        return checkCondEffect(when->eff, require, err);

    }else{
        if (checkPEffect(cond, require, err) != 0){
            BOR_ERR2(err, "A single effect has to be either literal or"
                     " conditional effect (+ universal quantifier).");
            return -1;
        }
        return 0;
    }
}

static int checkPEffect(const pddl_cond_t *cond, int require, bor_err_t *err)
{
    if (cond->type == PDDL_COND_ATOM
            || cond->type == PDDL_COND_ASSIGN
            || cond->type == PDDL_COND_INCREASE){
        return 0;
    }
    return -1;
}

static int checkCondEffect(const pddl_cond_t *cond, int require,
                           bor_err_t *err)
{
    const pddl_cond_part_t *part;
    const pddl_cond_t *sub;
    bor_list_t *item;

    if (checkPEffect(cond, require, err) == 0)
        return 0;

    if (cond->type == PDDL_COND_AND){
        part = OBJ(cond, part);
        BOR_LIST_FOR_EACH(&part->part, item){
            sub = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
            if (checkPEffect(sub, require, err) != 0){
                BOR_ERR2(err, "Conditional effect can contain only literals"
                         " and conjuction of literals.");
                return -1;
            }
        }

        return 0;
    }

    return -1;
}

int pddlCondCheckEff(const pddl_cond_t *cond, int require, bor_err_t *err)
{
    const pddl_cond_part_t *and;
    const pddl_cond_t *sub;
    bor_list_t *item;

    if (cond->type == PDDL_COND_AND){
        and = OBJ(cond, part);
        BOR_LIST_FOR_EACH(&and->part, item){
            sub = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
            if (checkCEffect(sub, require, err) != 0)
                return -1;
        }

        return 0;

    }else{
        return checkCEffect(cond, require, err);
    }
}


static int setPredRead(pddl_cond_t *cond, void *data)
{
    pddl_cond_atom_t *atom;
    pddl_preds_t *preds = data;

    if (cond->type == PDDL_COND_ATOM){
        atom = OBJ(cond, atom);
        preds->pred[atom->pred].read = 1;
    }
    return 0;
}

void pddlCondSetPredRead(const pddl_cond_t *cond, pddl_preds_t *preds)
{
    pddlCondTraverse((pddl_cond_t *)cond, setPredRead, NULL, preds);
}


static int setPredReadWrite(pddl_cond_t *cond, void *data)
{
    pddl_cond_atom_t *atom;
    pddl_cond_when_t *when;
    pddl_preds_t *preds = data;

    if (cond->type == PDDL_COND_WHEN){
        when = OBJ(cond, when);
        pddlCondTraverse((pddl_cond_t *)when->pre, setPredRead, NULL, data);
        pddlCondTraverse((pddl_cond_t *)when->eff,
                         setPredReadWrite, NULL, data);
        return -1;

    }else if (cond->type == PDDL_COND_ATOM){
        atom = OBJ(cond, atom);
        preds->pred[atom->pred].write = 1;
    }
    return 0;
}

void pddlCondSetPredReadWriteEff(const pddl_cond_t *cond, pddl_preds_t *preds)
{
    pddlCondTraverse((pddl_cond_t *)cond, setPredReadWrite, NULL, preds);
}

/*** INSTANTIATE QUANTIFIERS ***/
struct instantiate_cond {
    int param_id;
    pddl_obj_id_t obj_id;
};
typedef struct instantiate_cond instantiate_cond_t;

static int instantiateParentParam(pddl_cond_t *c, void *data)
{
    if (c->type == PDDL_COND_ATOM){
        const pddl_params_t *params = data;
        pddl_cond_atom_t *a = OBJ(c, atom);
        for (int i = 0; i < params->param_size; ++i){
            if (params->param[i].inherit < 0)
                continue;

            for (int j = 0; j < a->arg_size; ++j){
                if (a->arg[j].param == i)
                    a->arg[j].param = params->param[i].inherit;
            }
        }

    }else if (c->type == PDDL_COND_ASSIGN
                || c->type == PDDL_COND_INCREASE){
        if (OBJ(c, func_op)->lvalue)
            return instantiateParentParam(&OBJ(c, func_op)->lvalue->cls, data);
        if (OBJ(c, func_op)->fvalue)
            return instantiateParentParam(&OBJ(c, func_op)->fvalue->cls, data);
    }

    return 0;
}

static int instantiateCond(pddl_cond_t *c, void *data)
{
    const instantiate_cond_t *d = data;

    if (c->type == PDDL_COND_ATOM){
        pddl_cond_atom_t *a = OBJ(c, atom);
        for (int i = 0; i < a->arg_size; ++i){
            if (a->arg[i].param == d->param_id){
                a->arg[i].param = -1;
                a->arg[i].obj = d->obj_id;
            }
        }

    }else if (c->type == PDDL_COND_ASSIGN
                || c->type == PDDL_COND_INCREASE){
        if (OBJ(c, func_op)->lvalue)
            return instantiateParentParam(&OBJ(c, func_op)->lvalue->cls, data);
        if (OBJ(c, func_op)->fvalue)
            return instantiateCond(&OBJ(c, func_op)->fvalue->cls, data);
    }

    return 0;
}

static pddl_cond_part_t *instantiatePart(pddl_cond_part_t *p,
                                         int param_id,
                                         const pddl_obj_id_t *objs,
                                         int objs_size)
{
    pddl_cond_part_t *out;
    pddl_cond_t *c, *newc;
    bor_list_t *item;
    instantiate_cond_t set;

    out = condPartNew(p->cls.type);

    for (int i = 0; i < objs_size; ++i){
        BOR_LIST_FOR_EACH(&p->part, item){
            c = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
            newc = pddlCondClone(c);
            set.param_id = param_id;
            set.obj_id = objs[i];
            pddlCondTraverse(newc, NULL, instantiateCond, &set);
            condPartAdd(out, newc);
        }
    }

    pddlCondDel(&p->cls);
    return out;
}

static pddl_cond_t *instantiateQuant(pddl_cond_quant_t *q,
                                     const pddl_types_t *types)
{
    pddl_cond_part_t *top;
    const pddl_param_t *param;
    const pddl_obj_id_t *obj;
    int obj_size, bval;

    // The instantiation of universal/existential quantifier is a
    // conjuction/disjunction of all instances.
    if (q->cls.type == PDDL_COND_FORALL){
        top = condPartNew(PDDL_COND_AND);
    }else{
        top = condPartNew(PDDL_COND_OR);
    }
    condPartAdd(top, q->cond);
    q->cond = NULL;

    // Apply object to each (non-inherited) parameter according to its type
    for (int i = 0; i < q->param.param_size; ++i){
        param = q->param.param + i;
        if (param->inherit >= 0)
            continue;

        obj = pddlTypesObjsByType(types, param->type, &obj_size);
        if (obj_size == 0){
            bval = q->cls.type == PDDL_COND_FORALL;
            pddlCondDel(&top->cls);
            pddlCondDel(&q->cls);
            return &condBoolNew(bval)->cls;

        }else{
            top = instantiatePart(top, i, obj, obj_size);
        }
    }

    // Replace all parameters inherited from the parent with IDs of the
    // parent parameters.
    pddlCondTraverse(&top->cls, NULL, instantiateParentParam, &q->param);

    pddlCondDel(&q->cls);
    return &top->cls;
}

static int instantiateForall(pddl_cond_t **c, void *data)
{
    const pddl_types_t *types = data;

    if ((*c)->type != PDDL_COND_FORALL)
        return 0;

    *c = instantiateQuant(OBJ(*c, quant), types);
    return 0;
}

static int instantiateExist(pddl_cond_t **c, void *data)
{
    const pddl_types_t *types = data;

    if ((*c)->type != PDDL_COND_EXIST)
        return 0;

    *c = instantiateQuant(OBJ(*c, quant), types);
    return 0;
}

static void pddlCondInstantiateQuant(pddl_cond_t **cond,
                                     const pddl_types_t *types)
{
    pddlCondRebuild(cond, NULL, instantiateForall, (void *)types);
    pddlCondRebuild(cond, NULL, instantiateExist, (void *)types);
}



/*** SIMPLIFY ***/
static pddl_cond_t *removeBoolPart(pddl_cond_part_t *part)
{
    bor_list_t *item, *tmp;
    pddl_cond_t *c;
    int bval;

    BOR_LIST_FOR_EACH_SAFE(&part->part, item, tmp){
        c = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
        if (c->type != PDDL_COND_BOOL)
            continue;

        bval = OBJ(c, bool)->val;
        if (part->cls.type == PDDL_COND_AND){
            if (!bval){
                pddlCondDel(&part->cls);
                return &condBoolNew(0)->cls;
            }else{
                borListDel(item);
                pddlCondDel(c);
            }

        }else{ // PDDL_COND_OR
            if (bval){
                pddlCondDel(&part->cls);
                return &condBoolNew(1)->cls;
            }else{
                borListDel(item);
                pddlCondDel(c);
            }
        }
    }

    if (borListEmpty(&part->part)){
        if (part->cls.type == PDDL_COND_AND){
            pddlCondDel(&part->cls);
            return &condBoolNew(1)->cls;

        }else{ // PDDL_COND_OR
            pddlCondDel(&part->cls);
            return &condBoolNew(0)->cls;
        }
    }

    return &part->cls;
}

static pddl_cond_t *removeBoolWhen(pddl_cond_when_t *when)
{
    pddl_cond_t *c;
    int bval;

    if (when->pre->type != PDDL_COND_BOOL)
        return &when->cls;

    bval = OBJ(when->pre, bool)->val;
    if (bval){
        c = when->eff;
        when->eff = NULL;
        pddlCondDel(&when->cls);
        return c;

    }else{ // !bval
        pddlCondDel(&when->cls);
        return &condBoolNew(1)->cls;
    }
}

static int atomIsInInit(const pddl_t *pddl, const pddl_cond_atom_t *atom)
{
    bor_list_t *item;
    const pddl_cond_t *c;

    BOR_LIST_FOR_EACH(&pddl->init->part, item){
        c = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
        if (c->type == PDDL_COND_ATOM
                && pddlCondAtomCmpNoNeg(atom, OBJ(c, atom)) == 0)
            return 1;
    }
    return 0;
}

/** Returns true if atom at least partially matches grounded atom ground_atom
 *  (disregarding negative flag), i.e., true is returned if the objects (in
 *  place of arguments) match. */
static int atomPartialMatchNoNeg(const pddl_cond_atom_t *atom,
                                 const pddl_cond_atom_t *ground_atom)
{
    int cmp;

    cmp = atom->pred - ground_atom->pred;
    if (cmp == 0){
        if (atom->arg_size != ground_atom->arg_size)
            return 0;

        for (int i = 0; i < atom->arg_size && cmp == 0; ++i){
            if (atom->arg[i].param >= 0)
                continue;
            cmp = atom->arg[i].obj - ground_atom->arg[i].obj;
        }
    }

    return cmp == 0;
}

static int atomIsPartiallyInInit(const pddl_t *pddl,
                                 const pddl_cond_atom_t *atom)
{
    bor_list_t *item;
    const pddl_cond_t *c;

    BOR_LIST_FOR_EACH(&pddl->init->part, item){
        c = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
        if (c->type == PDDL_COND_ATOM
                && atomPartialMatchNoNeg(atom, OBJ(c, atom))){
            return 1;
        }
    }
    return 0;
}

static pddl_cond_t *removeBoolAtom(pddl_cond_atom_t *atom, const pddl_t *pddl)
{
    int bval;

    if (pddlPredIsStatic(&pddl->pred.pred[atom->pred])){
        if (atom->pred == pddl->pred.eq_pred){
            ASSERT(atom->arg_size == 2);
            if (atom->arg[0].obj >= 0 && atom->arg[1].obj >= 0){
                // Evaluate fully grounded (= ...) atom
                if (atom->arg[0].obj == atom->arg[1].obj){
                    bval = !atom->neg;
                }else{
                    bval = atom->neg;
                }
                pddlCondDel(&atom->cls);
                return &condBoolNew(bval)->cls;
            }

        }else if (pddlCondAtomIsGrounded(atom)){
            // If the atom is static and fully grounded we can evaluate it
            // right now by comparing it to the inital state
            if (atomIsInInit(pddl, atom)){
                bval = !atom->neg;
            }else{
                bval = atom->neg;
            }
            pddlCondDel(&atom->cls);
            return &condBoolNew(bval)->cls;

        }else if (atom->neg && !atomIsPartiallyInInit(pddl, atom)){
            // If the atom is static but not fully grounded we can evaluate
            // it if there is no atom matching the grounded parts
            bval = atom->neg;
            pddlCondDel(&atom->cls);
            return &condBoolNew(bval)->cls;
        }
    }

    return &atom->cls;
}

static pddl_cond_t *removeBoolImply(pddl_cond_imply_t *imp)
{
    if (imp->left->type == PDDL_COND_BOOL){
        pddl_cond_bool_t *b = OBJ(imp->left, bool);
        if (b->val){
            pddl_cond_t *ret = imp->right;
            imp->right = NULL;
            pddlCondDel(&imp->cls);
            return ret;

        }else{
            pddlCondDel(&imp->cls);
            return &condBoolNew(1)->cls;
        }
    }

    return &imp->cls;
}

static int removeBool(pddl_cond_t **c, void *data)
{
    const pddl_t *pddl = data;

    if ((*c)->type == PDDL_COND_ATOM){
        *c = removeBoolAtom(OBJ(*c, atom), pddl);

    }else if ((*c)->type == PDDL_COND_AND
            || (*c)->type == PDDL_COND_OR){
        *c = removeBoolPart(OBJ(*c, part));

    }else if ((*c)->type == PDDL_COND_WHEN){
        *c = removeBoolWhen(OBJ(*c, when));

    }else if ((*c)->type == PDDL_COND_IMPLY){
        *c = removeBoolImply(OBJ(*c, imply));
    }

    return 0;
}

static pddl_cond_t *flattenPart(pddl_cond_part_t *part)
{
    bor_list_t *item, *tmp;
    pddl_cond_t *c;
    pddl_cond_part_t *p;

    if (borListEmpty(&part->part))
        return &part->cls;

    BOR_LIST_FOR_EACH_SAFE(&part->part, item, tmp){
        c = BOR_LIST_ENTRY(item, pddl_cond_t, conn);

        if (c->type == part->cls.type){
            // Flatten con/disjunctions
            p = OBJ(c, part);
            condPartStealPart(part, p);

            borListDel(item);
            pddlCondDel(c);

        }else if ((c->type == PDDL_COND_AND || c->type == PDDL_COND_OR)
                    && borListEmpty(&OBJ(c, part)->part)){
            borListDel(item);
            pddlCondDel(c);
        }

    }

    // If the con/disjunction contains only one atom, remove the
    // con/disjunction and return the atom directly
    if (borListPrev(&part->part) == borListNext(&part->part)){
        item = borListNext(&part->part);
        c = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
        borListDel(item);
        pddlCondDel(&part->cls);
        return c;
    }

    return &part->cls;
}

/** Splits (when ...) if its precondition is disjunction */
static pddl_cond_t *flattenWhen(pddl_cond_when_t *when)
{
    bor_list_t *item;
    pddl_cond_t *c;
    pddl_cond_part_t *pre;
    pddl_cond_part_t *and;
    pddl_cond_when_t *add;

    if (!when->pre || when->pre->type != PDDL_COND_OR)
        return &when->cls;

    and = condPartNew(PDDL_COND_AND);
    pre = OBJ(when->pre, part);
    when->pre = NULL;

    while (!borListEmpty(&pre->part)){
        item = borListNext(&pre->part);
        borListDel(item);
        c = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
        add = condWhenClone(when);
        add->pre = c;
        condPartAdd(and, &add->cls);
    }

    pddlCondDel(&pre->cls);
    pddlCondDel(&when->cls);

    return &and->cls;
}

static int flatten(pddl_cond_t **c, void *data)
{
    if ((*c)->type == PDDL_COND_AND
            || (*c)->type == PDDL_COND_OR){
        *c = flattenPart(OBJ(*c, part));

    }else if ((*c)->type == PDDL_COND_WHEN){
        *c = flattenWhen(OBJ(*c, when));
    }

    return 0;
}

static pddl_cond_part_t *moveDisjunctionsCreate1(pddl_cond_part_t *top,
                                                 pddl_cond_part_t *or)
{
    pddl_cond_part_t *ret;
    bor_list_t *item1, *item2;
    pddl_cond_t *c1, *c2;
    pddl_cond_part_t *add;

    ret = condPartNew(PDDL_COND_OR);
    BOR_LIST_FOR_EACH(&top->part, item1){
        c1 = BOR_LIST_ENTRY(item1, pddl_cond_t, conn);
        BOR_LIST_FOR_EACH(&or->part, item2){
            c2 = BOR_LIST_ENTRY(item2, pddl_cond_t, conn);
            add = OBJ(c1, part);
            add = condPartClone(add);
            condPartAdd(add, pddlCondClone(c2));
            condPartAdd(ret, &add->cls);
        }
    }

    pddlCondDel(&top->cls);
    return ret;
}

static pddl_cond_t *moveDisjunctionsCreate(pddl_cond_part_t *and,
                                           bor_list_t *or_list)
{
    bor_list_t *or_item;
    pddl_cond_part_t *or;
    pddl_cond_part_t *ret;

    ret = condPartNew(PDDL_COND_OR);
    condPartAdd(ret, &and->cls);
    while (!borListEmpty(or_list)){
        or_item = borListNext(or_list);
        borListDel(or_item);
        or = OBJ(BOR_LIST_ENTRY(or_item, pddl_cond_t, conn), part);
        ret = moveDisjunctionsCreate1(ret, or);
        pddlCondDel(&or->cls);
    }

    return &ret->cls;
}

static pddl_cond_t *moveDisjunctionsUpAnd(pddl_cond_part_t *and)
{
    bor_list_t *item, *tmp;
    bor_list_t or_list;
    pddl_cond_t *c;

    borListInit(&or_list);
    BOR_LIST_FOR_EACH_SAFE(&and->part, item, tmp){
        c = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
        if (c->type != PDDL_COND_OR)
            continue;

        borListDel(item);
        borListAppend(&or_list, item);
    }

    if (borListEmpty(&or_list)){
        return &and->cls;
    }

    return moveDisjunctionsCreate(and, &or_list);
}

static int moveDisjunctionsUp(pddl_cond_t **c, void *data)
{
    if ((*c)->type == PDDL_COND_AND)
        *c = moveDisjunctionsUpAnd(OBJ(*c, part));

    if ((*c)->type == PDDL_COND_OR)
        *c = flattenPart(OBJ(*c, part));
    return 0;
}

/** (imply ...) is considered static if it has a simple flattened left and
 *  right side and the left side consists solely of static predicates. */
static int isStaticImply(const pddl_cond_imply_t *imp, const pddl_t *pddl)
{
    ASSERT(imp->left != NULL && imp->right != NULL);
    pddl_cond_part_t *and;
    pddl_cond_atom_t *atom;
    pddl_cond_t *c;
    bor_list_t *item;

    if (imp->left->type == PDDL_COND_ATOM){
        atom = OBJ(imp->left, atom);
        if (atom->pred < 0)
            return 0;
        if (!pddlPredIsStatic(&pddl->pred.pred[atom->pred]))
            return 0;

    }else if (imp->left->type == PDDL_COND_AND){
        and = OBJ(imp->left, part);
        BOR_LIST_FOR_EACH(&and->part, item){
            c = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
            if (c->type != PDDL_COND_ATOM)
                return 0;

            atom = OBJ(c, atom);
            if (atom->pred < 0)
                return 0;
            if (!pddlPredIsStatic(&pddl->pred.pred[atom->pred]))
                return 0;
        }

    }else{
        return 0;
    }

    if (imp->right->type == PDDL_COND_ATOM){
        return 1;

    }else if (imp->right->type == PDDL_COND_AND){
        and = OBJ(imp->right, part);
        BOR_LIST_FOR_EACH(&and->part, item){
            c = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
            if (c->type != PDDL_COND_ATOM)
                return 0;
        }

    }else{
        return 0;
    }

    return 1;
}

static int removeNonStaticImply(pddl_cond_t **c, void *data)
{
    pddl_cond_imply_t *imp;
    const pddl_t *pddl = data;

    if ((*c)->type != PDDL_COND_IMPLY)
        return 0;
    imp = OBJ(*c, imply);
    if (!isStaticImply(imp, pddl)){
        pddl_cond_part_t *or;
        or = condPartNew(PDDL_COND_OR);
        pddlCondPartAdd(or, pddlCondNegate(imp->left, pddl));
        pddlCondPartAdd(or, imp->right);
        *c = &or->cls;

        imp->right = NULL;
        pddlCondDel(&imp->cls);
    }

    return 0;
}


static void implyAtomParams(const pddl_cond_atom_t *atom, bor_iset_t *params)
{
    for (int i = 0; i < atom->arg_size; ++i){
        if (atom->arg[i].param >= 0)
            borISetAdd(params, atom->arg[i].param);
    }
}

static int implyParams(pddl_cond_t *c, void *data)
{
    bor_iset_t *params = data;
    pddl_cond_imply_t *imp;
    bor_list_t *item;
    pddl_cond_part_t *and;
    pddl_cond_t *p;

    if (c->type == PDDL_COND_IMPLY){
        imp = OBJ(c, imply);
        if (imp->left->type == PDDL_COND_ATOM){
            implyAtomParams(OBJ(imp->left, atom), params);

        }else if (imp->left->type == PDDL_COND_AND){
            and = OBJ(imp->left, part);
            BOR_LIST_FOR_EACH(&and->part, item){
                p = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
                if (p->type == PDDL_COND_ATOM)
                    implyAtomParams(OBJ(p, atom), params);
            }
        }
    }

    return 0;
}

struct instantiate_ctx {
    const bor_iset_t *params;
    const pddl_obj_id_t *arg;
};
typedef struct instantiate_ctx instantiate_ctx_t;

static int instantiateTraverse(pddl_cond_t *cond, void *ud)
{
    const instantiate_ctx_t *ctx = ud;
    if (cond->type == PDDL_COND_ATOM){
        pddl_cond_atom_t *atom = OBJ(cond, atom);
        for (int i = 0; i < atom->arg_size; ++i){
            for (int j = 0; j < borISetSize(ctx->params); ++j){
                if (atom->arg[i].param == borISetGet(ctx->params, j)){
                    atom->arg[i].param = -1;
                    atom->arg[i].obj = ctx->arg[j];
                    break;
                }
            }
        }
    }

    return 0;
}

static pddl_cond_t *instantiate(pddl_cond_t *cond,
                                const bor_iset_t *params,
                                const pddl_obj_id_t *arg,
                                int eq_pred)
{
    pddl_cond_part_t *and;
    pddl_cond_atom_t *eq;
    pddl_cond_t *c = pddlCondClone(cond);
    instantiate_ctx_t ctx;

    and = condPartNew(PDDL_COND_AND);
    for (int i = 0; i < borISetSize(params); ++i){
        int param = borISetGet(params, i);
        eq = condAtomNew();
        eq->pred = eq_pred;
        eq->arg_size = 2;
        eq->arg = BOR_ALLOC_ARR(pddl_cond_atom_arg_t, 2);
        eq->arg[0].param = param;
        eq->arg[0].obj = PDDL_OBJ_ID_UNDEF;
        eq->arg[1].param = -1;
        eq->arg[1].obj = arg[i];
        pddlCondPartAdd(and, &eq->cls);
    }

    ctx.params = params;
    ctx.arg = arg;
    pddlCondTraverse(c, NULL, instantiateTraverse, &ctx);

    pddlCondPartAdd(and, c);
    return &and->cls;
}

static void removeStaticImplyRec(pddl_cond_part_t *top,
                                 pddl_cond_t *cond,
                                 const pddl_t *pddl,
                                 const pddl_params_t *params,
                                 const bor_iset_t *imp_params,
                                 int pidx,
                                 pddl_obj_id_t *arg)
{
    const pddl_obj_id_t *obj;
    int obj_size;

    if (pidx == borISetSize(imp_params)){
        pddl_cond_t *c = instantiate(cond, imp_params, arg,
                                     pddl->pred.eq_pred);
        pddlCondPartAdd(top, c);
    }else{
        int param = borISetGet(imp_params, pidx);
        obj = pddlTypesObjsByType(&pddl->type, params->param[param].type,
                                  &obj_size);
        for (int i = 0; i < obj_size; ++i){
            arg[pidx] = obj[i];
            removeStaticImplyRec(top, cond, pddl, params,
                                 imp_params, pidx + 1, arg);
        }
    }
}
                                 
/** Implications are removed by instantiation of the left sides and putting
 *  the instantiated objects to (= ...) predicate. */
static int removeStaticImply(pddl_cond_t **cond, const pddl_t *pddl,
                             const pddl_params_t *params)
{
    pddl_cond_part_t *or;
    BOR_ISET(imply_params);
    pddl_obj_id_t *obj;

    if (params == NULL)
        return 0;

    pddlCondTraverse(*cond, NULL, implyParams, &imply_params);
    if (borISetSize(&imply_params) > 0){
        obj = BOR_ALLOC_ARR(pddl_obj_id_t, borISetSize(&imply_params));
        or = condPartNew(PDDL_COND_OR);
        removeStaticImplyRec(or, *cond, pddl, params, &imply_params, 0, obj);
        BOR_FREE(obj);
        pddlCondDel(*cond);
        *cond = &or->cls;
    }
    borISetFree(&imply_params);
    return 0;
}

pddl_cond_t *pddlCondNormalize(pddl_cond_t *cond, const pddl_t *pddl,
                               const pddl_params_t *params)
{
    pddl_cond_t *c = cond;

    // TODO: Check return values
    pddlCondInstantiateQuant(&c, &pddl->type);
    pddlCondRebuild(&c, NULL, removeNonStaticImply, (void *)pddl);
    removeStaticImply(&c, pddl, params);
    pddlCondRebuild(&c, NULL, removeBool, (void *)pddl);
    pddlCondRebuild(&c, NULL, flatten, NULL);
    pddlCondRebuild(&c, NULL, moveDisjunctionsUp, NULL);
    pddlCondRebuild(&c, NULL, flatten, NULL);
    c = pddlCondDeduplicate(c, pddl);
    return c;
}

static void _deduplicate(pddl_cond_part_t *p)
{
    bor_list_t *item, *item2;
    pddl_cond_t *c1, *c2;

    BOR_LIST_FOR_EACH(&p->part, item){
        c1 = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
        if (c1->type != PDDL_COND_ATOM)
            continue;

        item2 = borListNext(item);
        for (; item2 != &p->part; item2 = borListNext(item2)){
            c2 = BOR_LIST_ENTRY(item2, pddl_cond_t, conn);
            if (c2->type != PDDL_COND_ATOM)
                continue;
            if (pddlCondAtomCmp(OBJ(c1, atom), OBJ(c2, atom)) == 0){
                borListDel(item2);
                pddlCondDel(c2);
                break;
            }
        }
    }
}

static int deduplicate(pddl_cond_t **c, void *data)
{
    if ((*c)->type == PDDL_COND_AND || (*c)->type == PDDL_COND_OR)
        _deduplicate(OBJ(*c, part));
    return 0;
}

pddl_cond_t *pddlCondDeduplicate(pddl_cond_t *cond, const pddl_t *pddl)
{
    pddl_cond_t *c = cond;
    pddlCondRebuild(&c, NULL, deduplicate, NULL);
    return c;
}


struct deconflict_pre {
    const pddl_t *pddl;
    int change;
};

static int preHasConflict(pddl_cond_part_t *p, const pddl_t *pddl)
{
    bor_list_t *item, *item2;
    pddl_cond_t *c1, *c2;
    pddl_cond_atom_t *a1, *a2;

    BOR_LIST_FOR_EACH(&p->part, item){
        c1 = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
        if (c1->type != PDDL_COND_ATOM)
            continue;
        a1 = OBJ(c1, atom);

        item2 = borListNext(item);
        for (; item2 != &p->part; item2 = borListNext(item2)){
            c2 = BOR_LIST_ENTRY(item2, pddl_cond_t, conn);
            if (c2->type != PDDL_COND_ATOM)
                continue;
            a2 = OBJ(c2, atom);

            if (pddlCondAtomInConflict(a1, a2, pddl))
                return 1;
        }
    }

    return 0;
}

static int deconflictPre(pddl_cond_t **c, void *data)
{
    struct deconflict_pre *dp = data;

    if ((*c)->type == PDDL_COND_AND || (*c)->type == PDDL_COND_OR){
        if (preHasConflict(OBJ(*c, part), dp->pddl)){
            pddlCondDel(*c);
            *c = &(condBoolNew(0)->cls);
            dp->change = 1;
        }
    }
    return 0;
}

pddl_cond_t *pddlCondDeconflictPre(pddl_cond_t *cond, const pddl_t *pddl,
                                   const pddl_params_t *params)
{
    struct deconflict_pre dp;
    pddl_cond_t *c = cond;

    dp.pddl = pddl;
    dp.change = 0;
    pddlCondRebuild(&c, NULL, deconflictPre, &dp);
    if (dp.change)
        c = pddlCondNormalize(c, pddl, params);
    return c;
}

static int removeConflictsInEff(pddl_cond_part_t *p)
{
    bor_list_t *item, *item2, *tmp;
    pddl_cond_t *c1, *c2;
    pddl_cond_atom_t *a1, *a2;
    int change = 0;

    for (item = borListNext(&p->part); item != &p->part;){
        c1 = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
        if (c1->type != PDDL_COND_ATOM){
            item = borListNext(item);
            continue;
        }
        a1 = OBJ(c1, atom);

        for (item2 = borListNext(item); item2 != &p->part;){
            c2 = BOR_LIST_ENTRY(item2, pddl_cond_t, conn);
            if (c2->type != PDDL_COND_ATOM){
                item2 = borListNext(item2);
                continue;
            }
            a2 = OBJ(c2, atom);

            if (pddlCondAtomInConflict(a1, a2, NULL)){
                if (a1->neg){
                    tmp = borListPrev(item);
                    borListDel(item);
                    pddlCondDel(&a1->cls);
                    item = tmp;
                    change = 1;
                    break;

                }else{
                    tmp = borListPrev(item2);
                    borListDel(item2);
                    pddlCondDel(&a2->cls);
                    item2 = tmp;
                    change = 1;
                }
            }
            item2 = borListNext(item2);
        }

        item = borListNext(item);
    }

    return change;
}

static int deconflictEffPost(pddl_cond_t **c, void *data)
{
    if ((*c)->type == PDDL_COND_AND || (*c)->type == PDDL_COND_OR){
        if (removeConflictsInEff(OBJ(*c, part)))
            *((int *)data) = 1;
    }
    return 0;
}

static int deconflictEffPre(pddl_cond_t **c, void *data)
{
    if ((*c)->type == PDDL_COND_WHEN){
        pddl_cond_when_t *w = OBJ(*c, when);
        pddlCondRebuild(&w->eff, deconflictEffPre, deconflictEffPost, data);
        return -1;
    }
    return 0;
}

pddl_cond_t *pddlCondDeconflictEff(pddl_cond_t *cond, const pddl_t *pddl,
                                   const pddl_params_t *params)
{
    pddl_cond_t *c = cond;
    int change = 0;
    pddlCondRebuild(&c, deconflictEffPre, deconflictEffPost, &change);
    if (change)
        c = pddlCondNormalize(c, pddl, params);
    return c;
}

int pddlCondAtomIsGrounded(const pddl_cond_atom_t *atom)
{
    for (int i = 0; i < atom->arg_size; ++i){
        if (atom->arg[i].param >= 0)
            return 0;
    }
    return 1;
}

static int cmpAtomArgs(const pddl_cond_atom_t *a1, const pddl_cond_atom_t *a2)
{
    int cmp = 0;
    if (a1->arg_size != a2->arg_size)
        return a1->arg_size - a2->arg_size;
    for (int i = 0; i < a1->arg_size && cmp == 0; ++i){
        cmp = a1->arg[i].param - a2->arg[i].param;
        if (cmp == 0)
            cmp = a1->arg[i].obj - a2->arg[i].obj;
    }
    return cmp;
}

static int cmpAtoms(const pddl_cond_atom_t *a1, const pddl_cond_atom_t *a2,
                    int neg)
{
    int cmp;

    cmp = a1->pred - a2->pred;
    if (cmp == 0){
        cmp = cmpAtomArgs(a1, a2);
        if (cmp == 0 && neg)
            return a1->neg - a2->neg;
    }

    return cmp;
}

int pddlCondAtomCmp(const pddl_cond_atom_t *a1,
                    const pddl_cond_atom_t *a2)
{
    return cmpAtoms(a1, a2, 1);
}

int pddlCondAtomCmpNoNeg(const pddl_cond_atom_t *a1,
                         const pddl_cond_atom_t *a2)
{
    return cmpAtoms(a1, a2, 0);
}

static int atomNegPred(const pddl_cond_atom_t *a, const pddl_t *pddl)
{
    int pred = a->pred;
    if (pddl->pred.pred[a->pred].neg_of >= 0)
        pred = BOR_MIN(pred, pddl->pred.pred[a->pred].neg_of);
    return pred;
}

int pddlCondAtomInConflict(const pddl_cond_atom_t *a1,
                           const pddl_cond_atom_t *a2,
                           const pddl_t *pddl)
{
    if (a1->pred == a2->pred && a1->neg != a2->neg)
        return cmpAtomArgs(a1, a2) == 0;
    if (pddl != NULL
            && atomNegPred(a1, pddl) == atomNegPred(a2, pddl)
            && a1->neg == a2->neg){
        return cmpAtomArgs(a1, a2) == 0;
    }
    return 0;
}


/*** PRINT ***/
static void condPartPrint(const pddl_t *pddl,
                          pddl_cond_part_t *cond,
                          const char *name,
                          const pddl_params_t *params,
                          FILE *fout)
{
    bor_list_t *item;
    const pddl_cond_t *child;

    fprintf(fout, "(%s", name);
    BOR_LIST_FOR_EACH(&cond->part, item){
        child = BOR_LIST_ENTRY(item, pddl_cond_t, conn);
        fprintf(fout, " ");
        pddlCondPrint(pddl, child, params, fout);
    }
    fprintf(fout, ")");
}

static void condQuantPrint(const pddl_t *pddl,
                           const pddl_cond_quant_t *q,
                           const char *name,
                           const pddl_params_t *params,
                           FILE *fout)
{
    fprintf(fout, "(%s", name);

    fprintf(fout, " (");
    pddlParamsPrint(&q->param, fout);
    fprintf(fout, ") ");

    pddlCondPrint(pddl, q->cond, &q->param, fout);

    fprintf(fout, ")");
}

static void condWhenPrint(const pddl_t *pddl,
                          const pddl_cond_when_t *w,
                          const pddl_params_t *params,
                          FILE *fout)
{
    fprintf(fout, "(when ");
    pddlCondPrint(pddl, w->pre, params, fout);
    fprintf(fout, " ");
    pddlCondPrint(pddl, w->eff, params, fout);
    fprintf(fout, ")");
}

static void condAtomPrint(const pddl_t *pddl,
                          const pddl_cond_atom_t *atom,
                          const pddl_params_t *params,
                          FILE *fout, int is_func)
{
    const pddl_pred_t *pred;
    int i;

    if (is_func){
        pred = pddl->func.pred + atom->pred;
    }else{
        pred = pddl->pred.pred + atom->pred;
    }

    fprintf(fout, "(");
    if (atom->neg)
        fprintf(fout, "N:");
    if (pred->read)
        fprintf(fout, "R");
    if (pred->write)
        fprintf(fout, "W");
    fprintf(fout, ":%s", pred->name);

    for (i = 0; i < atom->arg_size; ++i){
        fprintf(fout, " ");
        if (atom->arg[i].param >= 0){
            fprintf(fout, "%s", params->param[atom->arg[i].param].name);
        }else{
            fprintf(fout, "%s", pddl->obj.obj[atom->arg[i].obj].name);
        }
    }

    fprintf(fout, ")");
}

static void condBoolPrint(const pddl_cond_bool_t *b, FILE *fout)
{
    if (b->val){
        fprintf(fout, "TRUE");
    }else{
        fprintf(fout, "FALSE");
    }
}

static void condImplyPrint(const pddl_cond_imply_t *imp,
                           const pddl_t *pddl,
                           const pddl_params_t *params,
                           FILE *fout)

{
    fprintf(fout, "(imply ");
    if (imp->left)
        pddlCondPrint(pddl, imp->left, params, fout);
    fprintf(fout, " ");
    if (imp->right)
        pddlCondPrint(pddl, imp->right, params, fout);
    fprintf(fout, ")");
}

void pddlCondPrint(const struct pddl *pddl,
                   const pddl_cond_t *cond,
                   const pddl_params_t *params,
                   FILE *fout)
{
    if (cond->type == PDDL_COND_AND){
        condPartPrint(pddl, OBJ(cond, part), "and", params, fout);

    }else if (cond->type == PDDL_COND_OR){
        condPartPrint(pddl, OBJ(cond, part), "or", params, fout);

    }else if (cond->type == PDDL_COND_FORALL){
        condQuantPrint(pddl, OBJ(cond, quant), "forall", params, fout);

    }else if (cond->type == PDDL_COND_EXIST){
        condQuantPrint(pddl, OBJ(cond, quant), "exists", params, fout);

    }else if (cond->type == PDDL_COND_WHEN){
        condWhenPrint(pddl, OBJ(cond, when), params, fout);

    }else if (cond->type == PDDL_COND_ATOM){
        condAtomPrint(pddl, OBJ(cond, atom), params, fout, 0);

    }else if (cond->type == PDDL_COND_ASSIGN){
        condFuncOpPrintPDDL(OBJ(cond, func_op), pddl, params, fout);

    }else if (cond->type == PDDL_COND_INCREASE){
        condFuncOpPrintPDDL(OBJ(cond, func_op), pddl, params, fout);

    }else if (cond->type == PDDL_COND_BOOL){
        condBoolPrint(OBJ(cond, bool), fout);

    }else if (cond->type == PDDL_COND_IMPLY){
        condImplyPrint(OBJ(cond, imply), pddl, params, fout);

    }else{
        fprintf(stderr, "Fatal Error: Unknown type!\n");
        exit(-1);
    }
}

void pddlCondPrintPDDL(const pddl_cond_t *cond,
                       const pddl_t *pddl,
                       const pddl_params_t *params,
                       FILE *fout)
{
    cond_cls[cond->type].print_pddl(cond, pddl, params, fout);
}


const pddl_cond_atom_t *pddlCondConstItAtomInit(pddl_cond_const_it_atom_t *it,
                                                const pddl_cond_t *cond)
{
    bzero(it, sizeof(*it));

    if (cond == NULL)
        return NULL;

    if (cond->type == PDDL_COND_ATOM)
        return PDDL_COND_CAST(cond, atom);

    if (cond->type == PDDL_COND_AND || cond->type == PDDL_COND_OR){
        const pddl_cond_part_t *p = PDDL_COND_CAST(cond, part);
        it->list = &p->part;
        for (it->cur = borListNext((bor_list_t *)it->list);
                it->cur != it->list;
                it->cur = borListNext((bor_list_t *)it->cur)){
            const pddl_cond_t *c = BOR_LIST_ENTRY(it->cur, pddl_cond_t, conn);
            if (c->type == PDDL_COND_ATOM)
                return PDDL_COND_CAST(c, atom);
        }
        return NULL;
    }

    return NULL;
}

const pddl_cond_atom_t *pddlCondConstItAtomNext(pddl_cond_const_it_atom_t *it)
{
    if (it->cur == it->list)
        return NULL;

    for (it->cur = borListNext((bor_list_t *)it->cur);
            it->cur != it->list;
            it->cur = borListNext((bor_list_t *)it->cur)){
        const pddl_cond_t *c = BOR_LIST_ENTRY(it->cur, pddl_cond_t, conn);
        if (c->type == PDDL_COND_ATOM)
            return PDDL_COND_CAST(c, atom);
    }

    return NULL;
}



static const pddl_cond_t *constItEffNextCond(pddl_cond_const_it_eff_t *it,
                                             const pddl_cond_t **pre)
{
    if (pre != NULL)
        *pre = NULL;

    if (it->when_cur != NULL){
        it->when_cur = borListNext((bor_list_t *)it->when_cur);
        if (it->when_cur == it->when_list){
            it->when_cur = it->when_list = NULL;
            it->when_pre = NULL;
        }else{
            if (pre != NULL)
                *pre = it->when_pre;
            return BOR_LIST_ENTRY(it->when_cur, pddl_cond_t, conn);
        }
    }

    if (it->list == it->cur)
        return NULL;
    if (it->cur == NULL){
        it->cur = borListNext((bor_list_t *)it->list);
    }else{
        it->cur = borListNext((bor_list_t *)it->cur);
    }
    if (it->list == it->cur)
        return NULL;
    return BOR_LIST_ENTRY(it->cur, pddl_cond_t, conn);
}

static const pddl_cond_atom_t *constItEffWhen(pddl_cond_const_it_eff_t *it,
                                              const pddl_cond_when_t *w,
                                              const pddl_cond_t **pre)
{
    if (w->eff == NULL){
        return NULL;

    }else if (w->eff->type == PDDL_COND_ATOM){
        if (pre != NULL)
            *pre = w->pre;
        return PDDL_COND_CAST(w->eff, atom);

    }else if (w->eff->type == PDDL_COND_AND){
        const pddl_cond_part_t *p = PDDL_COND_CAST(w->eff, part);
        it->when_pre = w->pre;
        it->when_list = &p->part;
        it->when_cur = it->when_list;
        return NULL;

    }else{
        ASSERT_RUNTIME_M(
            w->eff->type != PDDL_COND_OR
                && w->eff->type != PDDL_COND_FORALL
                && w->eff->type != PDDL_COND_EXIST
                && w->eff->type != PDDL_COND_IMPLY
                && w->eff->type != PDDL_COND_WHEN,
            "Effect is not normalized.");
    }
    return NULL;
}

const pddl_cond_atom_t *pddlCondConstItEffInit(pddl_cond_const_it_eff_t *it,
                                               const pddl_cond_t *cond,
                                               const pddl_cond_t **pre)
{
    bzero(it, sizeof(*it));

    if (pre != NULL)
        *pre = NULL;
    if (cond == NULL)
        return NULL;

    if (cond->type == PDDL_COND_ATOM){
        return PDDL_COND_CAST(cond, atom);

    }else if (cond->type == PDDL_COND_AND){
        const pddl_cond_part_t *p = PDDL_COND_CAST(cond, part);
        it->list = &p->part;
        return pddlCondConstItEffNext(it, pre);

    }else if (cond->type == PDDL_COND_WHEN){
        const pddl_cond_when_t *w = PDDL_COND_CAST(cond, when);
        const pddl_cond_atom_t *a = constItEffWhen(it, w, pre);
        if (a != NULL)
            return a;
        return pddlCondConstItEffNext(it, pre);

    }else{
        ASSERT_RUNTIME_M(
            cond->type != PDDL_COND_OR
                && cond->type != PDDL_COND_FORALL
                && cond->type != PDDL_COND_EXIST
                && cond->type != PDDL_COND_IMPLY,
            "Effect is not normalized.");
    }
    return NULL;
}

const pddl_cond_atom_t *pddlCondConstItEffNext(pddl_cond_const_it_eff_t *it,
                                               const pddl_cond_t **pre)
{
    const pddl_cond_t *c;

    while (1){
        c = constItEffNextCond(it, pre);
        if (c == NULL){
            return NULL;

        }else if (c->type == PDDL_COND_ATOM){
            return PDDL_COND_CAST(c, atom);

        }else if (c->type == PDDL_COND_WHEN){
            const pddl_cond_when_t *w = PDDL_COND_CAST(c, when);
            const pddl_cond_atom_t *a = constItEffWhen(it, w, pre);
            if (a != NULL)
                return a;
        }else{
            ASSERT_RUNTIME_M(
                c->type != PDDL_COND_AND
                    && c->type != PDDL_COND_OR
                    && c->type != PDDL_COND_FORALL
                    && c->type != PDDL_COND_EXIST
                    && c->type != PDDL_COND_IMPLY
                    && c->type != PDDL_COND_WHEN,
                "Effect is not normalized.");
        }
    }
}
