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


void pddlh2Free(pddl_h2_t *h2) {
    return;
}

void pddlh2Init(pddl_h2_t *h, const pddl_fdr_t *fdr) {
    return;
}

static void initFacts(pddl_h2_t *h) {
    return;
}

static void initOps(pddl_h2_t *h) {
    return;
}

static void addInitState(pddl_h2_t *h, 
                         const int *s, 
                         const pddl_fdr_vars_t *vars, 
                         pddl_pq_t *C) {
    return;
}

static void enqueueOpEffects(pddl_h2_t *h,
                             pddl_h2_op_t *op,
                             int fact_val,
                             pddl_pq_t *C) {
    return;
}

int pddlH_1(pddl_h2_t *h,
                  const int *s,
                  const pddl_fdr_vars_t *vars) {
    return 0;
}
