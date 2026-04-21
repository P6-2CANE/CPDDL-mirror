/***
 * Copyright (c)2024 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */

#include "pddl/hmax.h"
#include "internal.h"
#include "_heur.h"

struct pddl_heur_hmax {
    pddl_heur_t heur;
    pddl_fdr_vars_t fdr_vars;
    pddl_hmax_t hmax;
};
typedef struct pddl_heur_hmax pddl_heur_hmax_t;

void printHeurHMaxStruct(pddl_heur_hmax_t *h);

static void heurDel(pddl_heur_t *_h)
{
    pddl_heur_hmax_t *h = pddl_container_of(_h, pddl_heur_hmax_t, heur);
    _pddlHeurFree(&h->heur);
    pddlFDRVarsFree(&h->fdr_vars);
    pddlHMaxFree(&h->hmax);
    FREE(h);
}

static int heurEstimate(pddl_heur_t *_h,
                        const pddl_fdr_state_space_node_t *node,
                        const pddl_fdr_state_space_t *state_space)
{
    pddl_heur_hmax_t *h = pddl_container_of(_h, pddl_heur_hmax_t, heur);
    return pddlHMax(&h->hmax, node->state, &h->fdr_vars);
}

pddl_heur_t *pddlHeurHMax(const pddl_fdr_t *fdr, pddl_err_t *err)
{
    pddl_heur_hmax_t *h = ZALLOC(pddl_heur_hmax_t);
    pddlHMaxInit(&h->hmax, fdr);
    printHeurHMaxStruct(h);
    pddlFDRVarsInitCopy(&h->fdr_vars, &fdr->var);
    _pddlHeurInit(&h->heur, heurDel, heurEstimate);
    return &h->heur;
}

void printHeurHMaxStruct(pddl_heur_hmax_t *h){
    pddl_hmax_t h1 = h->hmax; 

    printf("Fact size: %d, Fact goal: %d, Fact nopre: %d, Fact operation size: %d, Fact operation goal: %d\n",
         h1.fact_size, 
         h1.fact_goal, 
         h1.fact_nopre, 
         h1.op_size, 
         h1.op_goal
    );
    
    printf("Printing the facts:\n");
         for (int i = 0; i < h1.fact_size; i++)
    {
        printf("Size: %d, Alloc: %d, ", 
            h1.fact[i].pre_op.size, 
            h1.fact[i].pre_op.alloc
        );
        printf("Facts:");

        for (int j = 0; j < h1.fact[i].pre_op.size; i++)
        {
            printf("s: %d, ", h1.fact[j].pre_op.s[j]);
        }

        printf("\n");
    }

    printf("Printing the operations:\n");
    for (int i = 0; i < h1.op_size; i++)
    {
        printf("Cost: %d, Precondition size: %d, Unsatisfied conditions: %d\n", 
            h1.op[i].cost, 
            h1.op[i].pre_size, 
            h1.op[i].unsat
        );
    }
}

