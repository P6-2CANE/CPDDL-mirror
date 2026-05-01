#include "pddl/h2.h"
#include "internal.h" /* Used for ZALLOC_ARR and ZEROIZE etc. */

//Return id of fact in h2 object
#define FID(h, f) ((f) - (h)->fact) /* f points to element in fact array (h->fact), by subtracting the two pointers we get location of f */
//Return value of fact
#define FVALUE(fact) (fact)->heap.key
//Set value of fact in the heap to 'val' parameter
#define FVALUE_SET(fact, val) do { (fact)->heap.key = val; } while(0)
//Initialize fact to dead end
#define FVALUE_INIT(fact) FVALUE_SET((fact), PDDL_COST_DEAD_END)
//Check if fact has been set to value other than dead end
#define FVALUE_IS_SET(fact) (FVALUE(fact) != PDDL_COST_DEAD_END)

//Push fact onto priority queue C (or simply update if it was already set)
#define FPUSH(C, val, fact) \
    do { \
        if (FVALUE_IS_SET(fact)){ \
            pddlPQUpdate((C), (val), &(fact)->heap); \
        } else { \
            pddlPQPush((C), (val), &(fact)->heap); \
        } \
    } while (0)


void pddlH2Free(pddl_h2_t *h2) {
    for (int i = 0; i < h2->fact_size; ++i) 
        pddlISetFree(&h2->fact[i].pre_op);
    if (h2->fact != NULL)
        FREE(h2->fact); 

    for (int i = 0; i < h2->op_size; ++i)
        pddlISetFree(&h2->op[i].eff);
    if (h2->op != NULL)
        FREE(h2->op);
}

void pddlH2Init(pddl_h2_t *h, const pddl_fdr_t *fdr) {
    // Store original number of facts n from fdr
    int n = fdr->var.global_id_size;
    h->n = n;

    // Size of facts allocated for all facts, pairs of facts and auxiliary facts
    h->fact_size = n + factPair(n-1, n-1, n) + 2;
    h->fact = ZALLOC_ARR(pddl_h2_fact_t, h->fact_size);
    h->fact_goal = h->fact_size - 2;
    h->fact_nopre = h->fact_size - 1;

    // Only original operators are set up
    h->op_size = fdr->op.op_size + 1;
    h->op = ZALLOC_ARR(pddl_h2_op_t, h->op_size);
    h->op_goal = h->op_size - 1;

    // Empty set to hold preconditions
    PDDL_ISET(pre);

    /* Iterate through operators 'src' in the fdr and assign
    to operators 'op' in the h2 struct */
    for (int op_id = 0; op_id < fdr->op.op_size; ++op_id){
        // Get pointers to src and op operators
        const pddl_fdr_op_t *src = fdr->op.op[op_id];
        pddl_h2_op_t *op = h->op + op_id;

        // Transfer effects from src to op as fact ids
        pddlFDRPartStateToGlobalIDs(&src->eff, &fdr->var, &op->eff);
        // Transfer the cost of the operator
        op->cost = src->cost;

        // Empty the set of preconditions 'pre' for each iteration
        pddlISetEmpty(&pre);
        // Transfer preconditions from src to 'pre' set as fact ids
        pddlFDRPartStateToGlobalIDs(&src->pre, &fdr->var, &pre);
        
        // For each fact in the preconditions, add the id of the current operator
        int fact;
        PDDL_ISET_FOR_EACH(&pre, fact)
            pddlISetAdd(&h->fact[fact].pre_op, op_id);
        // Set size of operator's pre_size to the number of preconditions
        op->pre_size = pddlISetSize(&pre);

        // Free up the memory of 'pre' set as it is no longer needed
        pddlISetFree(&pre);
    }
}

//From two fact ids, return the id representing their pair
int factPair(int x, int y, int n) {
    return n + (x*(2*n-x-1))/2 + (y-x-1);
}

static void initFacts(pddl_h2_t *h) {
    for (int i = 0; i < h->fact_size; i++){
        FVALUE_INIT(h->fact + i);
    }
}

static void initOps(pddl_h2_t *h) {
    for (int i = 0; i < h->fact_size; i++){
        int pre_size  = h->op[i].pre_size;
        h->op[i].unsat = pre_size * (pre_size + 1)/2; // Number of all possible combinations of unsatisfied preconditions
        pddlISetInit(&h->op[i].pfact); // Initialises the set of persistant facts to an empty set
    }
}

static void addInitState(pddl_h2_t *h, 
                         const int *s, 
                         const pddl_fdr_vars_t *vars, 
                         pddl_pq_t *C) {
    for (int i = 0; i < vars->var_size; ++i) {
        int fact_id_i = vars->var[i].val[s[i]].global_id;
        FPUSH(C, 0, h->fact + fact_id_i);

        for (int j = i + 1; j < vars->var_size; j++){
            int fact_id_j = vars->var[j].val[s[j]].global_id;
            FPUSH(C, 0, h->fact + factPair(fact_id_i, fact_id_j, h->n));
        }

    }
    FPUSH(C, 0, h->fact + h->fact_nopre);
}

/* Function to apply additional context (persistent/prevailing fact) to an action */
static void addContext( pddl_h2_t *h, /* h is used for h->n */
                        pddl_h2_op_t *op, /* the action to be applied */
                        int fid_q, /* the id of the persistent/prevailing fact */
                        int h_val, /* the heuristic value of the latest popped fact */
                        pddl_pq_t *C) { /* the priority queue */
    int val = op->cost + h_val; /* heuristic value of the new path */
    
    int fid; /* id of the facts in eff */
    PDDL_ISET_FOR_EACH(&op->eff, fid) { /* iterating through the facts of eff */
        pddl_h2_fact_t *fact; /* local variable to hold the fact */

        /* finding the fact, using the ID calculated with factPair
           factPair(x, y, n) requires that x <= y 
           therefore we need to compare fid and fid_q in order to apply correctly. 
        */
        if (fid < fid_q) { 
            fact = h->fact + factPair(fid, fid_q, h->n);
        } else {
            fact = h->fact + factPair(fid_q, fid, h->n);
        }
        
        /* If new path is cheaper push fact to priority queue with new value */
        if (FVALUE(fact) > val)
            FPUSH(C, val, fact);
    }
}

static void enqueueOpEffects(pddl_h2_t *h,
                             pddl_h2_op_t *op,
                             int fact_val,
                             pddl_pq_t *C) {
    return;
}

int pddlH_2(pddl_h2_t *h,
                  const int *s,
                  const pddl_fdr_vars_t *vars) {
    return 0;
}
