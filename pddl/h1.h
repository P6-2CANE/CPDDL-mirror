/***
 * Copyright (c)2024 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */


#ifndef __PDDL_H1_H__
#define __PDDL_H1_H__

#include <pddl/iset.h>
#include <pddl/fdr.h>
#include <pddl/pq.h>

#ifdef __cplusplus
extern "C" {
#endif /

/****************** Types ******************/

typedef struct pddl_h1_op {
    pddl_iset_t eff;
    int cost;
    int pre_size;
    int unsat;    
} pddl_h1_op_t;

typedef struct pddl_h1_fact {
    pddl_iset_t pre_op; 
    pddl_pq_el_t heap;
} pddl_h1_fact_t;

typedef struct pddl_h1 {
    pddl_h1_fact_t *fact; 
    int fact_size;
    int fact_goal; 
    int fact_nopre; 

    pddl_h1_op_t *op;
    int op_size;
    int op_goal;
} pddl_h1_t;

/****************** Function declarations ******************/

void pddlH1Init(pddl_h1_t *h, const pddl_fdr_t *fdr);
void pddlH1Free(pddl_h1_t *h1);
int pddlH_1(pddl_h1_t *h,
           const int *s,
           const pddl_fdr_vars_t *vars);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
