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

// From two fact ids, return the id representing their pair
int factPair(int x, int y, int n) {
    int id = (x*(2*n-x-1))/2;
    id = n + id + (y-x-1);
    return id;
}

void factPairReverse(int id, int n, int *x, int *y) {
    // Constant k used in calculations
    int k = id-n;
    int x_val = 0, y_val = 0;

    // Calculate x-value (split into steps) and update pointer
    float x_pow = pow((2*n-1),2);
    float x_sqrt = sqrt(x_pow-8*k);
    x_val = (int) ((2*n-1-x_sqrt)/2);
    *x = x_val;

    // Calculate y-value (split into steps) and update pointer
    y_val = (x_val*2*n- x_val - 1)/2;
    *y = x_val+1+k - y_val;
}

static void initFacts(pddl_h2_t *h) {
    for (int i = 0; i < h->fact_size; i++){
        FVALUE_INIT(h->fact + i);
    }
}

static void initOps(pddl_h2_t *h) {
    for (int i = 0; i < h->op_size; i++){
        int pre_size = h->op[i].pre_size;
        h->op[i].unsat = (pre_size * (pre_size + 1))/2; // Number of all possible combinations of unsatisfied preconditions
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
    pddl_pq_t C;
    pddlPQInit(&C);

    initFacts(h);
    initOps(h);
    addInitState(h, s, vars, &C);
    
    while (!pddlPQEmpty(&C)) {
        int h_val; //Variable for heuristic value of latest popped k

        pddl_pq_el_t *el = pddlPQPop(&C, &h_val); //popping k from queue C, and set heuristic value of k
        pddl_h2_fact_t *fact = pddl_container_of(el, pddl_h2_fact_t, heap); //finding the fact object of k

        int id_k = FID(h, fact); //finding the id of k

        int op_id;
        //If k is a singleton or empty precondition fact, apply action as in h1 
        //k is a singleton if its id is less than the number of fact in the original problem
        if (id_k < h->n || id_k == h->fact_nopre) { 
            if (id_k == h->fact_goal) //break if k is equal to goal fact
                break;

            PDDL_ISET_FOR_EACH(&fact->pre_op, op_id) { //for each action where k is a precondition
                pddl_h2_op_t *op = h->op + op_id; //finding the action object
                //If this was the last unsatisfied precondition for this operator, enqueue the facts in the operator's effects
                if (--op->unsat == 0)
                enqueueOpEffects(h, op, h_val, &C);
            }
        } else { //If k is a pair
            int id_f, id_q; //Variables for the two extracted facts in k

            //variable for the intersection of actions where f and q is a precondition  
            //By finding the intersection, we can find the actions where f and q exist as a pair in the preconditions
            pddl_iset_t intersec; 
            
            factPairReverse(id_k, h->n, &id_f, &id_q); //Extracting ids of f and q from k
            pddl_h2_fact_t *fact_f = h->fact + id_f; //Finding the fact objects of f and q
            pddl_h2_fact_t *fact_q = h->fact + id_q;
            
            if (id_f == h->fact_goal || id_q == h->fact_goal) //Break if f or q in k is the goal fact
                break;

            pddlISetIntersect2(&intersec, &fact_f->pre_op, &fact_q->pre_op); //Finding the intersection
            
            PDDL_ISET_FOR_EACH(&intersec, op_id) { //for each action where {f, q} is a precondition
                pddl_h2_op_t *op = h->op + op_id;
                //If this was the last unsatisfied precondition for this operator, enqueue the facts in the operator's effects
                if (--op->unsat == 0)
                    enqueueOpEffects(h, op, h_val, &C);
            }

            pddlISetFree(&intersec);
        }
    }
    
    pddlPQFree(&C); //Free priority queue C

    if (FVALUE_IS_SET(h->fact + h->fact_goal)) 
        return FVALUE(h->fact + h->fact_goal);
    else 
        return PDDL_COST_DEAD_END;
}
