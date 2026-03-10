#include "pddl/h1.h"
#include "internal.h" /* Used for ZALLOC_ARR and ZEROIZE etc. */

void pddlH1Free(pddl_h1_t *h1) {
    for (int i = 0; i < h1->fact_size; ++i) /* Free memory for pre_op, which is the array every fact*/
        pddlISetFree(&h1->fact[i].pre_op);
    if (h1->fact != NULL)
        FREE(h1->fact); /* If fact is not empty, free now  */

    for (int i = 0; i < h1->op_size; ++i)
        pddlISetFree(&h1->op[i].eff); /* Free memory for pre_op for every fact*/
    if (h1->op != NULL)
        FREE(h1->op);
}

void pddlH1Init(pddl_h1_t *h, const pddl_fdr_t *fdr) {
    /* ZEROIZE(h); */

    // Allocate facts and add one for empty-precondition fact and one for
    // goal fact
    h->fact_size = fdr->var.global_id_size + 2; /* make space for an empty-precondition and goal fact */
    h->fact = ZALLOC_ARR(pddl_h1_fact_t, h->fact_size); /* Allocate memory based on the updated fact size */
    h->fact_goal = h->fact_size - 2; /* The index of the goal fact in h->fact */
    h->fact_nopre = h->fact_size - 1; /* The index of the empty-precondition in h->fact */

    // Allocate operators and add one artificial for goal
    // Maybe: A conditional effect is an effect that occurs only when a specific condition holds
    h->op_size = fdr->op.op_size + 1; /* make space for operators, one artifical goal and conditional effects */
    h->op = ZALLOC_ARR(pddl_h1_op_t, h->op_size); /* Allocate memory based on the updated operator size */
    h->op_goal = h->op_size - 1; /* The index of the goal operator in h->op */

    /* We ignore conditional effects in our algorithm */
    /* int cond_eff_ins = fdr->op.op_size; */
    PDDL_ISET(pre); /* Create a variable/set pddl_iset_t pre = { NULL, 0, 0 } */

    /* We iterate through operators in the fdr */
    for (int op_id = 0; op_id < fdr->op.op_size; ++op_id){
        const pddl_fdr_op_t *src = fdr->op.op[op_id]; /* assign operator in the fdr */
        pddl_h1_op_t *op = h->op + op_id; /* assign address in the operator array in the heuristic object h */

        pddlFDRPartStateToGlobalIDs(&src->eff, &fdr->var, &op->eff); /* Add the global ID's of facts in effects into &op->eff */
        op->cost = src->cost; /* Declare the cost from the fdr */

        pddlISetEmpty(&pre); /* Empty the the pre set for each iteration */
        pddlFDRPartStateToGlobalIDs(&src->pre, &fdr->var, &pre); /* Add the global ID's of facts in preconditions into the pre set */
        
        int fact;
        PDDL_ISET_FOR_EACH(&pre, fact) /* for each fact in preconditions do pddlISetAdd */
            pddlISetAdd(&h->fact[fact].pre_op, op_id); /* Insert id of operator into fact, maybe: make link between fact and operator */
        op->pre_size = pddlISetSize(&pre); /* Set size/number of facts in precondition */

        // Record operator with no preconditions, therefore this operator can always be applied
        if (op->pre_size == 0){ 
            /* if operator has no preconditions, set this operator to the fact with empty precondition */
            pddlISetAdd(&h->fact[h->fact_nopre].pre_op, op_id);
            /* the precondition size is 1, since we need to explicitly include a fact that represents that there are no preconditions */
            op->pre_size = 1;
        }

        /* This is for conditional effects which we ignore */
        /* for (int cei = 0; cei < src->cond_eff_size; ++cei){
            const pddl_fdr_op_cond_eff_t *ce = src->cond_eff + cei;
            pddl_h1_op_t *op = h->op + cond_eff_ins;
            pddlFDRPartStateToGlobalIDs(&ce->eff, &fdr->var, &op->eff);
            op->cost = src->cost;

            PDDL_ISET(ce_pre);
            pddlISetUnion(&ce_pre, &pre);
            pddlFDRPartStateToGlobalIDs(&ce->pre, &fdr->var, &ce_pre);
            int fact;
            PDDL_ISET_FOR_EACH(&ce_pre, fact)
                pddlISetAdd(&h->fact[fact].pre_op, cond_eff_ins);
            op->pre_size = pddlISetSize(&ce_pre);
            PANIC_IF(op->pre_size == 0, "Conditional effect must have"
                     " non-empty precondition");

            ++cond_eff_ins;
        } */
    }
}
