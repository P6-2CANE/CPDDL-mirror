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
#endif /* __cplusplus */

/************* Types *************/

typedef struct pddl_h1_op {
    pddl_iset_t eff; /*!< Facts in its effect */
    int cost;       /*!< Cost of the operator */
    int pre_size;   /*!< Number of preconditions */
    int unsat;      /*!< Number of unsatisfied preconditions */
} pddl_h1_op_t;

typedef struct pddl_h1_fact {
    pddl_iset_t pre_op; /*!< Operators having this fact as its precondition */
    pddl_pq_el_t heap; /*!< Connection to priority heap */
} pddl_h1_fact_t;

typedef struct pddl_h1 {
    pddl_h1_fact_t *fact; /*!< Maybe: List of all facts? */
    int fact_size; /*!< Maybe: How many facts in this pddl? */
    int fact_goal; /*!< Maybe: Are we in goal state? */
    int fact_nopre; /*!< Maybe: Does the fact have a precondition? */

    pddl_h1_op_t *op; /*!< Maybe: List of all operations? */
    int op_size; /*!< Maybe: How many operations in this pddl? */
    int op_goal;/*!< Maybe: How many operators lead to goal state? */
} pddl_h1_t;

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __PDDL_H1_H__ */
