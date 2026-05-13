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

void pddlH2Init(pddl_h2_t *h, const pddl_fdr_t *fdr) {
    ZEROIZE(h);
    // Empty set to hold preconditions
    PDDL_ISET(pre);

    // Store original number of facts n from fdr
    int n = fdr->var.global_id_size;
    h->n = n;


    // Size of facts allocated for all facts, pairs of facts and auxiliary facts
    h->fact_size = factPair(n, n+1, n+2) + 1; // index of the last fact +1 to get the size, +2 more for auxiliary facts
    h->fact = ZALLOC_ARR(pddl_h2_fact_t, h->fact_size);
    h->fact_goal = h->n;
    h->fact_nopre = h->n + 1;

    // Only original operators are set up
    h->op_size = fdr->op.op_size + 1;
    h->op = ZALLOC_ARR(pddl_h2_op_t, h->op_size);
    h->op_goal = h->op_size - 1;
    //Store reference to the operators of the fdr
    h->ops = &fdr->op;


    /* Iterate through operators 'src' in the fdr and assign
    to operators 'op' in the h2 struct */
    for (int op_id = 0; op_id < fdr->op.op_size; ++op_id){
        // Get pointers to src and op operators
        const pddl_fdr_op_t *src = fdr->op.op[op_id];
        pddl_h2_op_t *op = h->op + op_id;
        op->global_id = op_id;

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
        PDDL_ISET_FOR_EACH(&pre, fact) {
            pddlISetAdd(&h->fact[fact].pre_op, op_id);
        }
        PDDL_ISET(eff);
        pddlFDRPartStateToGlobalIDs(&src->eff, &fdr->var, &eff);
        pddlISetFree(&eff);
        // Set size of operator's pre_size to the number of preconditions
        op->pre_size = pddlISetSize(&pre);

        if (op->pre_size == 0) { 
            // Add operator to nopre's set of operators who have it as a precondition
            pddlISetAdd(&h->fact[h->fact_nopre].pre_op, op_id);
            //Operator now technically has one precondition
            op->pre_size = 1;
        }

    }
    /* Lastly, we initialize fact_goal and op_goal, which mark that a goal state has been achieved.
    The operator op_goal has all the actual goal facts from FDR as its preconditions,
    applying it costs nothing, and the effect is the artificial goal_fact.
    In this way, a single fact can represent that all goal facts were indeed achieved.*/
    pddl_h2_op_t *op = h->op + h->op_goal; // Pointer to op_goal
    pddlISetAdd(&op->eff, h->fact_goal); // Set its effect to fact_goal
    op->cost = 0; // This operator should not cost anything as it is artificially inserted
    
    pddlISetEmpty(&pre); // Empty the set of preconditions used earlier (so we can reuse it)
    pddlFDRPartStateToGlobalIDs(&fdr->goal, &fdr->var, &pre); // Store all goal facts from FDR in pre
    int fact;
    PDDL_ISET_FOR_EACH(&pre, fact) { // For each of the facts in pre
        pddlISetAdd(&h->fact[fact].pre_op, h->op_goal); // Add them to the preconditions of op_goal
    }
    op->pre_size = pddlISetSize(&pre); // Update the size of the preconditions in op_goal to match
    
    // Free up the memory of 'pre' set as it is no longer needed
    pddlISetFree(&pre);

    h->global_id_to_var = ZALLOC_ARR(int, n+2); //!! changed

    //iterate through variables
    for (int i = 0; i < fdr->var.var_size; i++) {
        //iterate through values
        for (int j = 0; j < fdr->var.var[i].val_size; j++) {
            //insert variable id into index of global_id of its value
            h->global_id_to_var[fdr->var.var[i].val[j].global_id]=i;
        }
    }

    //[4,5,6,7,8,0,1,2,3] ordering of values according to vars
    //[0,0,0,1,1,2,2,2,2] translated into respective variables
    //[2,2,2,2,0,0,0,1,1] global_id_to_var array
}

// From two fact ids, return the id representing their pair
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


void factPairReverse(int id, int n, int *x, int *y) {
    // Constant k represents the 0-indexed offset within the pairs
    long k = id - n;
    double x_val = 0, y_val = 0;

    PANIC_IF(k < 0 || id < 0 , "k < 0 or id < 0 Fact pair reverse calculation resulted in out-of-bounds value");
    // Calculate x-value using the quadratic formula and floor()
    double x_pow = (2.0 * n - 1.0) * (2.0 * n - 1.0);
    PANIC_IF(x_pow < 0 , "x_pow < 0Fact pair reverse calculation resulted in out-of-bounds value");
    double x_sqrt = sqrt(x_pow - 8.0 * k);
    PANIC_IF(x_sqrt < 0 , "x_sqrt < 0 Fact pair reverse calculation resulted in out-of-bounds value");
    
    // Using floor instead of round to not jump to next integer
    x_val = (int)floor((2.0 * n - 1.0 - x_sqrt) / 2.0);
    *x = (int) x_val;

    // Calculate y-value offset
    y_val = (x_val * (2 * n - x_val - 1)) / 2;
    *y = (int) (x_val + 1 + k - y_val);

    // printf("Fact pair reverse calculation: y_val: %d \n", y_val);
    
    PANIC_IF(*x < 0 || *y < 0, "Fact pair reverse calculation resulted in out-of-bounds value");
    PANIC_IF(*y >= n+2, "y is too big! Fact pair reverse calculation resulted in out-of-bounds value");
}

static void initFacts(pddl_h2_t *h) {
    for (int i = 0; i < h->fact_size; i++) {
        FVALUE_INIT(h->fact + i);
    }
}

static void initOps(pddl_h2_t *h) {
    for (int i = 0; i < h->op_size; i++) {
        int pre_size = h->op[i].pre_size;
        h->op[i].unsat = ((pre_size * pre_size)+pre_size)/2; // Number of all possible combinations of unsatisfied preconditions
    }
}

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

/* Function to apply additional context (persistent/prevailing fact) to an action */
static void applyAdditionalContext( pddl_h2_t *h, /* h is used for h->n */
                        pddl_h2_op_t *op, /* the action to be applied */
                        int id_q, /* the id of the persistent/prevailing fact */
                        int h_val_k, /* the heuristic value of the latest popped fact k */
                        pddl_pq_t *C) { /* the priority queue */
    int val = op->cost + h_val_k; /* heuristic value of the new path */
    
    int id_f; /* id of the facts in eff */
    PDDL_ISET_FOR_EACH(&op->eff, id_f) { /* iterating through the facts of eff */
        pddl_h2_fact_t *fact; /* local variable to hold the fact */

        /* finding the fact, using the ID calculated with factPair
        */
        fact = h->fact + factPair(id_f, id_q, h->n);
        /* If new path is cheaper push fact to priority queue with new value */
        if (FVALUE(fact) > val) {
            FPUSH(C, val, fact);
        }
    }
}

void getPreconditions(  pddl_h2_t *h, 
                        pddl_h2_op_t *op,
                        const pddl_fdr_vars_t *vars,
                        pddl_iset_t *pre) {
    const pddl_fdr_op_t *fdr_op = h->ops->op[op->global_id]; // find the operator in the FDR
    pddlFDRPartStateToGlobalIDs(&fdr_op->pre, vars, pre); // transfer all preconditions from FDR to our pre set
}

static void applyAction(pddl_h2_t *h,
                        pddl_h2_op_t *op,
                        const pddl_fdr_vars_t *vars,
                        int h_val_k,
                        pddl_pq_t *C) {
    
    /* now, we can tell if a fact is in a variable i by checking that its value is 
    *  >= var_limits[i-1] and < var_limits[i]
    * (in this case, variables are 0-indexed, so we start with variable 0)
    */

    /* var1: 3 values, var2: 2 values, var3: 4 values
    *  var_limits: [0, 3, 5, 9]
    *
    *  for all fid, if fid >= var_limits[i-1] && fid < var_limits[i], then fid is in variable i.
    * 
    *  fid=0, i=1: 0>= var_limits[1-1] && 0 < var_limits[1], this fid is in var1
    *  fid=3, i=2: 3>= var_limits[2-1] && 3 < var_limits[2], this fid is in var2
    *  fid=8, i=3: 8>= var_limits[3-1] && 8 < var_limits[3], this fid is in var3
    */

   PDDL_ISET(pre); // initialise empty set pre

    int id_q;
    for (int i = 0; i < h->n; i++) { // iterate through all singleton facts
        id_q = i; // id of current fact q
        int q_var = h->global_id_to_var[id_q]; // initially set variable of q to 0 (no variable)
        
        // now we have our q_var :D
        
        // if any of the effects of the operator share a variable with q, continue for loop for next q
        if (sameVariable(&op->eff, q_var, h->global_id_to_var)) {
            continue;
        }
        
        // now we know that q is either prevail or persistent fact!!
        
        // We find all preconditions for the operator:
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
            //Comment out line below to remove optimisation for a.pf
            //pddlISetAdd(&op->pfact, id_q);
        }

    }
    pddlISetFree(&pre);
    /* Apply the action itself */
    int id_f;
    int val = op->cost + h_val_k;
    // for all singletons and pairs of effects f, if the newly achieved value is cheaper than the previous, push it to the queue
    PDDL_ISET_FOR_EACH(&op->eff, id_f) {
        PDDL_ISET_FOR_EACH(&op->eff, id_q) {
            int pair_id = factPair(id_f, id_q, h->n); // find the id of the pair
            pddl_h2_fact_t *fact = h->fact + pair_id;
            if (FVALUE(fact) > val) {
                FPUSH(C, val, fact);
            }
        }
    }
}


/* Checks if h-value is set for each pair {p,q} */
int allHValuesAreSet(pddl_iset_t *fact_set, int fact_id, pddl_h2_t *h) {
    int pre_fact; //pre_fact = f in the pair f,q
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

/*Checks if any fact in a set shares a variable q_var */
int sameVariable(pddl_iset_t *fact_set, int var_q, int *global_id_to_var) {
    int fact_id;
    PDDL_ISET_FOR_EACH(fact_set, fact_id) {
        if(global_id_to_var[fact_id]==var_q) {
            return 1;
        }
    }
    return 0;
}

int pddlH_2(pddl_h2_t *h,
                  const int *s,
                  const pddl_fdr_vars_t *vars) {
    pddl_pq_t C;
    pddlPQInit(&C);

    initFacts(h);
    initOps(h);
    addInitState(h, s, vars, &C);

    //variable for the intersection of actions where f and q is a precondition  
    //By finding the intersection, we can find the actions where f and q exist as a pair in the preconditions
    PDDL_ISET(intersec); 
    PDDL_ISET(pre);

    int h_val_k; //Variable for heuristic value of latest popped k
    
    while (!pddlPQEmpty(&C)) {
        pddl_pq_el_t *el = pddlPQPop(&C, &h_val_k); //popping k from queue C, and set heuristic value of k
        pddl_h2_fact_t *fact = pddl_container_of(el, pddl_h2_fact_t, heap); //finding the fact object of k

        int k = FID(h, fact); //finding the id of the latest popped fact k
        int isPair = 0; 
        int id_f, id_q; //Variables for the two extracted facts in k
        int op_id;
        //If k is a singleton or empty precondition fact, apply action as in h1 
        //k is a singleton if its id is less than the number of fact in the original problem
        if (k == h->fact_goal) { //break if k is equal to goal fact
            break;
        }

        if (k < h->n || k == h->fact_nopre) { 
            op_id = 0;

            PDDL_ISET_FOR_EACH(&fact->pre_op, op_id) { //for each action where k is a precondition
                pddl_h2_op_t *op = h->op + op_id; //finding the action object
                //If this was the last unsatisfied precondition for this operator, enqueue the facts in the operator's effects

                if (--op->unsat == 0) {
                    applyAction(h, op, vars, h_val_k, &C);
                }
            }
        } else { //If k is a pair
            factPairReverse(k, h->n, &id_f, &id_q); //Extracting ids of f and q from k

            if (id_f == h->fact_goal || id_q == h->fact_goal) {
                break;
            }
            
            isPair = 1;
            pddl_h2_fact_t *fact_f = h->fact + id_f; //Finding the fact objects of f and q
            pddl_h2_fact_t *fact_q = h->fact + id_q;

            pddlISetIntersect2(&intersec, &fact_f->pre_op, &fact_q->pre_op); //Finding the intersection (intersec is emptied by PDDLISetIntersect2) 
            op_id = 0;
            PDDL_ISET_FOR_EACH(&intersec, op_id) { //for each action where {f, q} is a precondition
                pddl_h2_op_t *op = h->op + op_id;
                //If this was the last unsatisfied precondition for this operator, enqueue the facts in the operator's effects

                if (--op->unsat == 0) {
                    applyAction(h, op, vars, h_val_k, &C);
                }
            }
        }

        for (int i = 0; i < h->op_size; i++) {
            pddl_h2_op_t *op = h->op + i;
            pddlISetEmpty(&pre);
            getPreconditions(h, op, vars, &pre);
            //Without optimisation for a.pf
            if (isPair == 0 && allHValuesAreSet(&pre, k, h)) {
                applyAdditionalContext(h, op, k, h_val_k, &C);
                continue;
            }
            if (isPair == 1 && allHValuesAreSet(&pre, id_f, h)) {
                applyAdditionalContext(h, op, id_f, h_val_k, &C);
            } 
            if (isPair == 1 && allHValuesAreSet(&pre, id_q, h)) {
                applyAdditionalContext(h, op, id_q, h_val_k, &C);
            }

            /* With optimisation for a.pf
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
            */
        }
    }
    

    pddlISetFree(&pre);
    pddlISetFree(&intersec);

    pddlPQFree(&C); //Free priority queue C
    
    if (FVALUE_IS_SET(h->fact + h->fact_goal)) {
        return FVALUE(h->fact + h->fact_goal);
    } else {
        return PDDL_COST_DEAD_END;
    }
}
