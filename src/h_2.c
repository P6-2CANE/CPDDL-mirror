#include "pddl/h2.h"
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


/* Free memory of the h2 object */
void pddlH2Free(pddl_h2_t *h2) {
    for (int i = 0; i < h2->fact_size; ++i) {
        pddlISetFree(&h2->fact[i].pre_op);
    }
    if (h2->fact != NULL) {
        FREE(h2->fact);
    }

    for (int i = 0; i < h2->op_size; ++i) {
        pddlISetFree(&h2->op[i].eff);
        pddlISetFree(&h2->op[i].pfact);
    }
    if (h2->op != NULL) {
        FREE(h2->op);
    }

    if (h2->ops != NULL) {
        h2->ops = NULL;
    }

    if (h2->global_id_to_var) {
        FREE(h2->global_id_to_var);
    }
}


/* Initialising facts and operators of h2 */
void pddlH2Init(pddl_h2_t *h, const pddl_fdr_t *fdr) {
    ZEROIZE(h);
    PDDL_ISET(pre);

    // Store original number of facts n from fdr + nopre and goal fact
    int n = fdr->var.global_id_size + 2;
    h->n = n;


    // Size of facts allocated for all facts, pairs of facts and auxiliary facts
    h->fact_size = factPair(n-2, n-1, n) + 1;
    h->fact = ZALLOC_ARR(pddl_h2_fact_t, h->fact_size);
    h->fact_goal = h->n - 2;
    h->fact_nopre = h->n - 1;

    //Set up original operators
    h->op_size = fdr->op.op_size + 1;
    h->op = ZALLOC_ARR(pddl_h2_op_t, h->op_size);
    h->op_goal = h->op_size - 1;
    h->ops = &fdr->op;

    /* Iterate through operators in the fdr and assign effects and preconditions to operators in the h2 struct */
    for (int op_id = 0; op_id < fdr->op.op_size; ++op_id){
        const pddl_fdr_op_t *src = fdr->op.op[op_id];
        pddl_h2_op_t *op = h->op + op_id;
        op->global_id = op_id;

        pddlFDRPartStateToGlobalIDs(&src->eff, &fdr->var, &op->eff);
        op->cost = src->cost;

        pddlISetEmpty(&pre);
        pddlFDRPartStateToGlobalIDs(&src->pre, &fdr->var, &pre);

        int fact;
        PDDL_ISET_FOR_EACH(&pre, fact) {
            pddlISetAdd(&h->fact[fact].pre_op, op_id);
        }
        PDDL_ISET(eff);
        pddlFDRPartStateToGlobalIDs(&src->eff, &fdr->var, &eff);
        pddlISetFree(&eff);
        op->pre_size = pddlISetSize(&pre);

        /* If the operator has no preconditions, associate it with the "nopre" */
        if (op->pre_size == 0) { 
            pddlISetAdd(&h->fact[h->fact_nopre].pre_op, op_id);
            op->pre_size = 1;
        }

    }

    //Initialising op_goal
    pddl_h2_op_t *op = h->op + h->op_goal;
    pddlISetAdd(&op->eff, h->fact_goal);
    op->cost = 0;
    op->global_id = h->op_goal;

    pddlISetEmpty(&pre);
    pddlFDRPartStateToGlobalIDs(&fdr->goal, &fdr->var, &pre);

    int fact;
    PDDL_ISET_FOR_EACH(&pre, fact) {
        pddlISetAdd(&h->fact[fact].pre_op, h->op_goal);
    }
    op->pre_size = pddlISetSize(&pre);
    pddlISetFree(&pre);

    h->global_id_to_var = ZALLOC_ARR(int, n);

    //Initialising the variables of all facts
    for (int i = 0; i < fdr->var.var_size; i++) {
        for (int j = 0; j < fdr->var.var[i].val_size; j++) {
            h->global_id_to_var[fdr->var.var[i].val[j].global_id]=i;
        }
    }

    //Initialising the auxiliary facts to have no variables
    h->global_id_to_var[n-2]=-1;
    h->global_id_to_var[n-1]=-1;

}

/* From two fact ids, return the id representing their pair */
int factPair(int x, int y, int n) {
    int id;

    PANIC_IF(x < 0 || y < 0 , "x or y in factPair i negative");

    if (x == y) {
        return x;
    } else if (x < y) {
        id = (x*(2*n-x-1))/2;
        return n + id + (y-x-1);
    } else {
        id = (y*(2*n-y-1))/2;
        return n + id + (x-y-1);
    }
}


/* Reverse the id of a pair, to the id of its two facts */
void factPairReverse(int id, int n, int *x, int *y) {
    long k = id - n;
    double x_val = 0, y_val = 0;

    PANIC_IF(k < 0 || id < 0 , "k < 0 or id < 0 Fact pair reverse calculation resulted in out-of-bounds value");
    double x_pow = (2.0 * n - 1.0) * (2.0 * n - 1.0);
    PANIC_IF(x_pow < 0 , "x_pow < 0Fact pair reverse calculation resulted in out-of-bounds value");
    double x_sqrt = sqrt(x_pow - 8.0 * k);
    PANIC_IF(x_sqrt < 0 , "x_sqrt < 0 Fact pair reverse calculation resulted in out-of-bounds value");
    
    x_val = (int)floor((2.0 * n - 1.0 - x_sqrt) / 2.0);
    *x = (int) x_val;

    y_val = (x_val * (2 * n - x_val - 1)) / 2;
    *y = (int) (x_val + 1 + k - y_val);
    
    PANIC_IF(*x < 0 || *y < 0, "Fact pair reverse calculation resulted in out-of-bounds value");
    PANIC_IF(*y >= n, "y is too big! Fact pair reverse calculation resulted in out-of-bounds value");
}

static void initFacts(pddl_h2_t *h) {
    for (int i = 0; i < h->fact_size; i++) {
        FVALUE_INIT(h->fact + i);
    }
}

/*  Initialising the unsat counter for all operators
    using the number of all possible combinations of unsatisfied preconditions */
static void initOps(pddl_h2_t *h) {
    for (int i = 0; i < h->op_size; i++) {
        int pre_size = h->op[i].pre_size;
        h->op[i].unsat = ((pre_size * pre_size)+pre_size)/2; 
    }
}

/*  Initialising the initial state by setting their heuristic value to 0 and inserting them into the queue  
    Includes all possible pairs of facts in the initial state */
static void addInitState(pddl_h2_t *h, 
                         const int *s, 
                         const pddl_fdr_vars_t *vars, 
                         pddl_pq_t *C) {
    for (int i = 0; i < vars->var_size; ++i) {
        int id_f = vars->var[i].val[s[i]].global_id;
        FPUSH(C, 0, h->fact + id_f);
        for (int j = i + 1; j < vars->var_size; j++){
            int id_q = vars->var[j].val[s[j]].global_id;
            FPUSH(C, 0, h->fact + factPair(id_f, id_q, h->n));
        }
    }
    FPUSH(C, 0, h->fact + h->fact_nopre);
}

/* Applies additional context (persistent/prevailing fact) to an action */
static void applyAdditionalContext( pddl_h2_t *h,
                        pddl_h2_op_t *op,
                        int id_q, 
                        int h_val_k, 
                        pddl_pq_t *C) {
    int val = op->cost + h_val_k;
    
    int id_f;
    /* iterating through the facts of effect applying as pairs with q */
    PDDL_ISET_FOR_EACH(&op->eff, id_f) {
        pddl_h2_fact_t *fact;

        fact = h->fact + factPair(id_f, id_q, h->n);
        if (FVALUE(fact) > val) {
            FPUSH(C, val, fact);
        }
    }
}

/* Retrieving the preconditions of an operator using FDR */
void getPreconditions(  pddl_h2_t *h, 
                        pddl_h2_op_t *op,
                        const pddl_fdr_vars_t *vars,
                        pddl_iset_t *pre) {
    const pddl_fdr_op_t *fdr_op = h->ops->op[op->global_id];
    pddlFDRPartStateToGlobalIDs(&fdr_op->pre, vars, pre);
}

/* Applying an action */
static void applyAction(pddl_h2_t *h,
                        pddl_h2_op_t *op,
                        const pddl_fdr_vars_t *vars,
                        int h_val_k,
                        pddl_pq_t *C) {
    //If goal op is reached, push the goal fact to the queue and do nothing else
    if (op->global_id == h->op_goal) {
        if (FVALUE(h->fact + h->fact_goal) > op->cost + h_val_k) {
            FPUSH(C, op->cost + h_val_k, h->fact + h->fact_goal);
        }
        return;
    }
   
   PDDL_ISET(pre);

    // iterate through all singleton facts to find prevailing and persisten facts
    for (int id_q = 0; id_q < h->n-2; id_q++) {
        int q_var = h->global_id_to_var[id_q];
        
        // if any of the effects of the operator share a variable with q, continue for loop for next q
        if (sameVariable(&op->eff, q_var, h->global_id_to_var)) {
            continue;
        }
        
        pddlISetEmpty(&pre);
        getPreconditions(h, op, vars, &pre);
        
        // if q is in the precondition, add context for the prevail fact
        if (pddlISetHas(&pre, id_q)) {
            applyAdditionalContext(h, op, id_q, h_val_k, C);
        } 
        // else if q shares a variable with any precondition p, continue outer for loop for next q
        else if (sameVariable(&pre, q_var, h->global_id_to_var)) {
            continue;
        } 
        // else if all pairs of {p, q} have an h-value, add context for the persistent fact
        else if (allHValuesAreSet(&pre, id_q, h)) {
            applyAdditionalContext(h, op, id_q, h_val_k, C);
        } 
        // else, the persistent fact is not yet applicable, store in operator's pfact set
        else {
            pddlISetAdd(&op->pfact, id_q);
        }

    }
    pddlISetFree(&pre);

    /* Apply the action itself for all singletons and pairs of effects */
    int id_f;
    int id_q;
    int val = op->cost + h_val_k;

    PDDL_ISET_FOR_EACH(&op->eff, id_f) {
        PDDL_ISET_FOR_EACH(&op->eff, id_q) {
            int pair_id = factPair(id_f, id_q, h->n);
            pddl_h2_fact_t *fact = h->fact + pair_id;
            if (FVALUE(fact) > val) {
                FPUSH(C, val, fact);
            }
        }
    }
}


/* Checks if h-value is set for each pair {p,q} */
int allHValuesAreSet(pddl_iset_t *fact_set, int fact_id, pddl_h2_t *h) {
    int pre_fact;
    int pair_id;
    PDDL_ISET_FOR_EACH(fact_set, pre_fact) {
        pair_id = factPair(pre_fact, fact_id, h->n);

        pddl_h2_fact_t *fact = &h->fact[pair_id];
        if (!FVALUE_IS_SET(fact)) {
            return 0;
        }
    }
    return 1;
}

/* Checks if any fact in a set shares a variable q_var */
int sameVariable(pddl_iset_t *fact_set, int var_q, int *global_id_to_var) {
    int fact_id;
    PDDL_ISET_FOR_EACH(fact_set, fact_id) {
        if(global_id_to_var[fact_id]==var_q) {
            return 1;
        }
    }
    return 0;
}

/* Main h2 algorithm */
int pddlH_2(pddl_h2_t *h,
                  const int *s,
                  const pddl_fdr_vars_t *vars) {
    pddl_pq_t C;
    pddlPQInit(&C);

    initFacts(h);
    initOps(h);
    addInitState(h, s, vars, &C);

    PDDL_ISET(intersec); 
    PDDL_ISET(pre);

    // Heuristic value of latest popped k
    int h_val_k; 
    
    while (!pddlPQEmpty(&C)) {
        pddl_pq_el_t *el = pddlPQPop(&C, &h_val_k);
        pddl_h2_fact_t *fact = pddl_container_of(el, pddl_h2_fact_t, heap);
        
        int k = FID(h, fact); // Finding the id of the latest popped fact k
        int isPair = 0; 
        int id_f, id_q;
        int op_id;

        if (k == h->fact_goal) {
            break;
        }

        if (k < h->n) { // If k is a singleton
            PDDL_ISET_FOR_EACH(&fact->pre_op, op_id) {
                pddl_h2_op_t *op = h->op + op_id;

                if (--op->unsat == 0) {
                    applyAction(h, op, vars, h_val_k, &C);
                }
            }
        } else { // If k is a pair
            factPairReverse(k, h->n, &id_f, &id_q);
            
            isPair = 1;
            pddl_h2_fact_t *fact_f = h->fact + id_f;
            pddl_h2_fact_t *fact_q = h->fact + id_q;

            // Finding the operators where both f and q are preconditions
            pddlISetIntersect2(&intersec, &fact_f->pre_op, &fact_q->pre_op); 
            op_id = 0;
            PDDL_ISET_FOR_EACH(&intersec, op_id) {
                pddl_h2_op_t *op = h->op + op_id;

                if (--op->unsat == 0) {
                    applyAction(h, op, vars, h_val_k, &C);
                }
            }
        }

        // Iterating through all operators and applies persistent facts
        for (int i = 0; i < h->op_size-1; i++) {
            pddl_h2_op_t *op = h->op + i;
            pddlISetEmpty(&pre);
            getPreconditions(h, op, vars, &pre);
            if (isPair == 0 && pddlISetHas(&op->pfact, k) && allHValuesAreSet(&pre, k, h)) {
                applyAdditionalContext(h, op, k, h_val_k, &C);
                pddlISetRm(&op->pfact, k);
                continue;
            }
            if (isPair == 1 && pddlISetHas(&op->pfact, id_f) && allHValuesAreSet(&pre, id_f, h)) {
                applyAdditionalContext(h, op, id_f, h_val_k, &C);
                pddlISetRm(&op->pfact, id_f);
            } 
            if (isPair == 1 && pddlISetHas(&op->pfact, id_q) && allHValuesAreSet(&pre, id_q, h)) {
                applyAdditionalContext(h, op, id_q, h_val_k, &C);
                pddlISetRm(&op->pfact, id_q);
            }
        }
    }
    
    // Free memory
    pddlISetFree(&pre);
    pddlISetFree(&intersec);
    pddlPQFree(&C);
    
    if (FVALUE_IS_SET(h->fact + h->fact_goal)) {
        return FVALUE(h->fact + h->fact_goal);
    } else {
        return PDDL_COST_DEAD_END;
    }
}
