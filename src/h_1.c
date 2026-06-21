#include "pddl/h1.h"
#include "internal.h"

#define FID(h, f) ((f) - (h)->fact)
#define FVALUE(fact) (fact)->heap.key
#define FVALUE_SET(fact, val) do { (fact)->heap.key = val; } while(0)
#define FVALUE_INIT(fact) FVALUE_SET((fact), PDDL_COST_DEAD_END)
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

/* Free memory of the h1 object */
void pddlH1Free(pddl_h1_t *h1) {
    for (int i = 0; i < h1->fact_size; ++i) {
        pddlISetFree(&h1->fact[i].pre_op);
    }
    if (h1->fact != NULL) {
        FREE(h1->fact);
    }
    for (int i = 0; i < h1->op_size; ++i) {
        pddlISetFree(&h1->op[i].eff);
    }
    if (h1->op != NULL) {
        FREE(h1->op);
    }
}

/* Initialising facts and operators of h1 */
void pddlH1Init(pddl_h1_t *h, const pddl_fdr_t *fdr) {
    // Allocate facts and add one for empty-precondition fact and one for goal fact
    h->fact_size = fdr->var.global_id_size + 2;
    h->fact = ZALLOC_ARR(pddl_h1_fact_t, h->fact_size);
    h->fact_goal = h->fact_size - 2;
    h->fact_nopre = h->fact_size - 1;

    // Allocate operators and add one artificial for goal
    h->op_size = fdr->op.op_size + 1;
    h->op = ZALLOC_ARR(pddl_h1_op_t, h->op_size);
    h->op_goal = h->op_size - 1;

    PDDL_ISET(pre);

    /* Iterate through operators in the fdr */
    for (int i = 0; i < fdr->op.op_size; ++i) {
        const pddl_fdr_op_t *src = fdr->op.op[i];
        pddl_h1_op_t *op = h->op + i;

        pddlFDRPartStateToGlobalIDs(&src->eff, &fdr->var, &op->eff);
        op->cost = src->cost; 

        pddlISetEmpty(&pre);
        pddlFDRPartStateToGlobalIDs(&src->pre, &fdr->var, &pre);
        
        int fact_id;
        PDDL_ISET_FOR_EACH(&pre, fact_id) {
            pddlISetAdd(&h->fact[fact_id].pre_op, i);
        }
        op->pre_size = pddlISetSize(&pre);

        /* If the operator has no preconditions, associate it with the "nopre" */
        if (op->pre_size == 0) { 
            pddlISetAdd(&h->fact[h->fact_nopre].pre_op, i);
            op->pre_size = 1;
        }
    }

    //Initialising op_goal
    pddl_h1_op_t *op = h->op + h->op_goal; 
    pddlISetAdd(&op->eff, h->fact_goal); 
    op->cost = 0; 

    pddlISetEmpty(&pre);
    pddlFDRPartStateToGlobalIDs(&fdr->goal, &fdr->var, &pre);
    int fact_id;

    // For each of the goal facts add them to the preconditions of op_goal
    PDDL_ISET_FOR_EACH(&pre, fact_id) { 
        pddlISetAdd(&h->fact[fact_id].pre_op, h->op_goal); 
    }    
    op->pre_size = pddlISetSize(&pre);

    pddlISetFree(&pre);
}

/* Initially mark all facts as dead ends */
static void initFacts(pddl_h1_t *h) {
    for (int i = 0; i < h->fact_size; ++i) {
        FVALUE_INIT(h->fact + i);
    }
}

/* Initialise the "unsatisfied" field for all operators to match the size of all its preconditions */
static void initOps(pddl_h1_t *h) {
    for (int i = 0; i < h->op_size; ++i) {
        h->op[i].unsat = h->op[i].pre_size;
    }
}

/* Enqueue all facts of the initial state and assign a h value of 0 */
static void addInitState(pddl_h1_t *h, 
                         const int *s, 
                         const pddl_fdr_vars_t *vars, 
                         pddl_pq_t *C) {
    for (int i = 0; i < vars->var_size; ++i) {
        int fact_id = vars->var[i].val[s[i]].global_id;
        FPUSH(C, 0, h->fact + fact_id);
    }
    FPUSH(C, 0, h->fact + h->fact_nopre);
}

/* Apply effects of fully satisfied operators, enqueue all facts that are now achievable */
static void applyAction(pddl_h1_t *h,
                             pddl_h1_op_t *op,
                             int h_val_k,
                             pddl_pq_t *C) {
    int h_val = op->cost + h_val_k;
    
    int fact_id;
    PDDL_ISET_FOR_EACH(&op->eff, fact_id) {
        pddl_h1_fact_t *fact = h->fact + fact_id;
        if (FVALUE(fact) > h_val) {
            FPUSH(C, h_val, fact);
        }
    }
}

/* Main h1 algorithm */
int pddlH_1(pddl_h1_t *h,
            const int *s,
            const pddl_fdr_vars_t *vars) {
    pddl_pq_t C;
    pddlPQInit(&C);

    initFacts(h);
    initOps(h);
    addInitState(h, s, vars, &C);

    while (!pddlPQEmpty(&C)) {
        int h_val;
        pddl_pq_el_t *el = pddlPQPop(&C, &h_val);
        pddl_h1_fact_t *fact = pddl_container_of(el, pddl_h1_fact_t, heap);

        int k = FID(h, fact); // Finding the ID of the latest popped fact

        if (k == h->fact_goal) {
            break;
        }

        //Go through each operator which has a precondition satisfiable by the current fact
        int op_id;
        PDDL_ISET_FOR_EACH(&fact->pre_op, op_id) {
            pddl_h1_op_t *op = h->op + op_id;
            if (--op->unsat == 0) {
                applyAction(h, op, h_val, &C);
            }
        }
    }
    
    pddlPQFree(&C);

    if (FVALUE_IS_SET(h->fact + h->fact_goal)) {
        return FVALUE(h->fact + h->fact_goal);
    } else {
        return PDDL_COST_DEAD_END;
    } 
}
