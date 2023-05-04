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

#include "pddl/pddl.h"
#include "pddl/require.h"

#include "err.h"

/**
 * Mapping between keywords and require flags.
 */
struct _require_mask_t {
    int kw;
    unsigned mask;
};
typedef struct _require_mask_t require_mask_t;

#define PDDL_REQUIRE_ADL (PDDL_REQUIRE_STRIPS \
                            | PDDL_REQUIRE_TYPING \
                            | PDDL_REQUIRE_NEGATIVE_PRE \
                            | PDDL_REQUIRE_DISJUNCTIVE_PRE \
                            | PDDL_REQUIRE_EQUALITY \
                            | PDDL_REQUIRE_EXISTENTIAL_PRE \
                            | PDDL_REQUIRE_UNIVERSAL_PRE \
                            | PDDL_REQUIRE_CONDITIONAL_EFF)

static require_mask_t require_mask[] = {
    { PDDL_KW_STRIPS, PDDL_REQUIRE_STRIPS },
    { PDDL_KW_TYPING, PDDL_REQUIRE_TYPING },
    { PDDL_KW_NEGATIVE_PRE, PDDL_REQUIRE_NEGATIVE_PRE },
    { PDDL_KW_DISJUNCTIVE_PRE, PDDL_REQUIRE_DISJUNCTIVE_PRE },
    { PDDL_KW_EQUALITY, PDDL_REQUIRE_EQUALITY },
    { PDDL_KW_EXISTENTIAL_PRE, PDDL_REQUIRE_EXISTENTIAL_PRE },
    { PDDL_KW_UNIVERSAL_PRE, PDDL_REQUIRE_UNIVERSAL_PRE },
    { PDDL_KW_CONDITIONAL_EFF, PDDL_REQUIRE_CONDITIONAL_EFF },
    { PDDL_KW_NUMERIC_FLUENT, PDDL_REQUIRE_NUMERIC_FLUENT },
    { PDDL_KW_OBJECT_FLUENT, PDDL_REQUIRE_OBJECT_FLUENT },
    { PDDL_KW_DURATIVE_ACTION, PDDL_REQUIRE_DURATIVE_ACTION },
    { PDDL_KW_DURATION_INEQUALITY, PDDL_REQUIRE_DURATION_INEQUALITY },
    { PDDL_KW_CONTINUOUS_EFF, PDDL_REQUIRE_CONTINUOUS_EFF },
    { PDDL_KW_DERIVED_PRED, PDDL_REQUIRE_DERIVED_PRED },
    { PDDL_KW_TIMED_INITIAL_LITERAL, PDDL_REQUIRE_TIMED_INITIAL_LITERAL },
    { PDDL_KW_DURATIVE_ACTION, PDDL_REQUIRE_DURATIVE_ACTION },
    { PDDL_KW_PREFERENCE, PDDL_REQUIRE_PREFERENCE },
    { PDDL_KW_CONSTRAINT, PDDL_REQUIRE_CONSTRAINT },
    { PDDL_KW_ACTION_COST, PDDL_REQUIRE_ACTION_COST },
    { PDDL_KW_MULTI_AGENT, PDDL_REQUIRE_MULTI_AGENT },
    { PDDL_KW_UNFACTORED_PRIVACY, PDDL_REQUIRE_UNFACTORED_PRIVACY },
    { PDDL_KW_FACTORED_PRIVACY, PDDL_REQUIRE_FACTORED_PRIVACY },

    { PDDL_KW_QUANTIFIED_PRE, PDDL_REQUIRE_EXISTENTIAL_PRE |
                                   PDDL_REQUIRE_UNIVERSAL_PRE },
    { PDDL_KW_FLUENTS, PDDL_REQUIRE_NUMERIC_FLUENT |
                            PDDL_REQUIRE_OBJECT_FLUENT },
    { PDDL_KW_ADL, PDDL_REQUIRE_ADL },
};
static int require_mask_size = sizeof(require_mask) / sizeof(require_mask_t);

static unsigned requireMask(int kw)
{
    int i;

    for (i = 0; i < require_mask_size; ++i){
        if (require_mask[i].kw == kw)
            return require_mask[i].mask;
    }
    return 0u;
}

int pddlRequireParse(pddl_t *pddl, bor_err_t *err)
{
    const pddl_lisp_node_t *req_node, *n;
    unsigned m;

    pddl->require = 0u;
    if (pddl->cfg.force_adl)
        pddl->require = PDDL_REQUIRE_ADL;

    req_node = pddlLispFindNode(&pddl->domain_lisp->root, PDDL_KW_REQUIREMENTS);
    // No :requirements implies :strips
    if (req_node == NULL){
        pddl->require |= PDDL_REQUIRE_STRIPS;
        return 0;
    }

    for (int i = 1; i < req_node->child_size; ++i){
        n = req_node->child + i;
        if (n->value == NULL){
            BOR_ERR_RET(err, -1, "Invalid :requirements definition in %s"
                                 " on line %d.",
                        pddl->domain_lisp->filename, n->lineno);
        }
        if ((m = requireMask(n->kw)) == 0u){
            BOR_ERR_RET(err, -1, "Invalid :requirements definition in %s"
                                 " on line %d: Unknown keyword `%s'.",
                        pddl->domain_lisp->filename, n->lineno, n->value);
        }

        pddl->require |= m;
    }

    return 0;
}

void pddlRequirePrintPDDL(unsigned require, FILE *fout)
{
    fprintf(fout, "(:requirements\n");
    if (require & PDDL_REQUIRE_STRIPS)
        fprintf(fout, "    :strips\n");
    if (require & PDDL_REQUIRE_TYPING)
        fprintf(fout, "    :typing\n");
    if (require & PDDL_REQUIRE_NEGATIVE_PRE)
        fprintf(fout, "    :negative-preconditions\n");
    if (require & PDDL_REQUIRE_DISJUNCTIVE_PRE)
        fprintf(fout, "    :disjunctive-preconditions\n");
    if (require & PDDL_REQUIRE_EQUALITY)
        fprintf(fout, "    :equality\n");
    if (require & PDDL_REQUIRE_EXISTENTIAL_PRE)
        fprintf(fout, "    :existential-preconditions\n");
    if (require & PDDL_REQUIRE_UNIVERSAL_PRE)
        fprintf(fout, "    :universal-preconditions\n");
    if (require & PDDL_REQUIRE_CONDITIONAL_EFF)
        fprintf(fout, "    :conditional-effects\n");
    if (require & PDDL_REQUIRE_NUMERIC_FLUENT)
        fprintf(fout, "    :numeric-fluents\n");
    if (require & PDDL_REQUIRE_OBJECT_FLUENT)
        fprintf(fout, "    :object-fluents\n");
    if (require & PDDL_REQUIRE_DURATIVE_ACTION)
        fprintf(fout, "    :durative-actions\n");
    if (require & PDDL_REQUIRE_DURATION_INEQUALITY)
        fprintf(fout, "    :duration-inequalities\n");
    if (require & PDDL_REQUIRE_CONTINUOUS_EFF)
        fprintf(fout, "    :continuous-effects\n");
    if (require & PDDL_REQUIRE_DERIVED_PRED)
        fprintf(fout, "    :derived-predicates\n");
    if (require & PDDL_REQUIRE_TIMED_INITIAL_LITERAL)
        fprintf(fout, "    :timed-initial-literals\n");
    if (require & PDDL_REQUIRE_PREFERENCE)
        fprintf(fout, "    :preferences\n");
    if (require & PDDL_REQUIRE_CONSTRAINT)
        fprintf(fout, "    :constraints\n");
    if (require & PDDL_REQUIRE_ACTION_COST)
        fprintf(fout, "    :action-costs\n");
    if (require & PDDL_REQUIRE_MULTI_AGENT)
        fprintf(fout, "    :multi-agent\n");
    if (require & PDDL_REQUIRE_UNFACTORED_PRIVACY)
        fprintf(fout, "    :unfactored-privacy\n");
    if (require & PDDL_REQUIRE_FACTORED_PRIVACY)
        fprintf(fout, "    :factored-privacy\n");
    fprintf(fout, ")\n");
}
