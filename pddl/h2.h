/***
 * Copyright (c)2024 Daniel Fiser <danfis@danfis.cz>. All rights reserved.
 * This file is part of cpddl licensed under 3-clause BSD License (see file
 * LICENSE, or https://opensource.org/licenses/BSD-3-Clause)
 */


#ifndef __PDDL_H2_H__
#define __PDDL_H2_H__

#include <pddl/iset.h>
#include <pddl/fdr.h>
#include <pddl/pq.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/****************** Types ******************/

typedef struct pddl_h2_op {
    pddl_iset_t eff;
    int cost;
    int pre_size;
    int unsat;
    pddl_iset_t pfact;
    int global_id;
} pddl_h2_op_t;

typedef struct pddl_h2_fact {
    pddl_iset_t pre_op;
    pddl_pq_el_t heap;
} pddl_h2_fact_t;

typedef struct pddl_h2 {
    pddl_h2_fact_t *fact;
    int fact_size;
    int n;
    int fact_goal;
    int fact_nopre;

    pddl_h2_op_t *op;
    int op_size;
    int op_goal;
    const pddl_fdr_ops_t *ops;
    int *global_id_to_var;
} pddl_h2_t;

/****************** Function declarations ******************/
void pddlH2Init(pddl_h2_t *h, const pddl_fdr_t *fdr);
void pddlH2Free(pddl_h2_t *h2);
int pddlH_2(pddl_h2_t *h,
           const int *s,
           const pddl_fdr_vars_t *vars);
int factPair(int x, int y, int n);
int sameVariable(pddl_iset_t *fact_set, int q_var, int *global_id_to_var);
int allHValuesAreSet(pddl_iset_t *fact_set, int fact_id, pddl_h2_t *h);


#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_h2_H__ */
