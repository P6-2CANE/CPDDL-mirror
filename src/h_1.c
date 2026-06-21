#include "pddl/h1.h"
#include "internal.h" /* Used for ZALLOC_ARR and ZEROIZE etc. */

//Return id of fact in h1 object
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

/* Free the memory of the fields in the h1 object that are pointers
    But are some fields not freed like alloc and size in iset?
*/
void pddlH1Free(pddl_h1_t *h1) {
    for (int i = 0; i < h1->fact_size; ++i) 
        pddlISetFree(&h1->fact[i].pre_op); /* Free memory for pre_op for each element in the fact array */
    if (h1->fact != NULL)
        FREE(h1->fact); /* If fact array is not empty, free now  */

    for (int i = 0; i < h1->op_size; ++i)
        pddlISetFree(&h1->op[i].eff); /* Free memory for eff for every operator */
    if (h1->op != NULL)
        FREE(h1->op); /* If operator array is not empty, free now  */
}

/* Allocate memory for h1 object */
void pddlH1Init(pddl_h1_t *h, const pddl_fdr_t *fdr) {
    // Allocate facts and add one for empty-precondition fact and one for goal fact
    h->fact_size = fdr->var.global_id_size + 2; /* make space for an empty-precondition and goal fact */
    h->fact = ZALLOC_ARR(pddl_h1_fact_t, h->fact_size); /* Allocate memory based on the updated fact size */
    h->fact_goal = h->fact_size - 2; /* The index of the goal fact in h->fact array */
    h->fact_nopre = h->fact_size - 1; /* The index of the empty-precondition in h->fact array */

    // Allocate operators and add one artificial for goal
    h->op_size = fdr->op.op_size + 1; /* make space for operators and one artifical goal */
    h->op = ZALLOC_ARR(pddl_h1_op_t, h->op_size); /* Allocate memory based on the updated operator size */
    h->op_goal = h->op_size - 1; /* The index of the goal operator in h->op array */

    PDDL_ISET(pre); /* Initialize empty set for preconditions (pddl_iset_t pre = { NULL, 0, 0 }) */

    /* Iterate through operators in the fdr */
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

        /* If the operator has no preconditions, associate it with the "nopre" fact we initialised earlier.
            This fact explicitly represents a lack of preconditions */
        if (op->pre_size == 0){ 
            // Add operator to nopre's set of operators who have it as a precondition
            pddlISetAdd(&h->fact[h->fact_nopre].pre_op, op_id);
            /* Operator now technically has one precondition */
            op->pre_size = 1;
        }
    }

    /* Lastly, we initialize fact_goal and op_goal, which mark that a goal state has been achieved.
    The operator op_goal has all the actual goal facts from FDR as its preconditions,
    applying it costs nothing, and the effect is the artificial goal_fact.
    In this way, a single fact can represent that all goal facts were indeed achieved.*/
    pddl_h1_op_t *op = h->op + h->op_goal; // Pointer to op_goal
    pddlISetAdd(&op->eff, h->fact_goal); // Set its effect to fact_goal
    op->cost = 0; // This operator should not cost anything as it is artificially inserted

    pddlISetEmpty(&pre); // Empty the set of preconditions used earlier (so we can reuse it)
    pddlFDRPartStateToGlobalIDs(&fdr->goal, &fdr->var, &pre); // Store all goal facts from FDR in pre
    int fact;
    PDDL_ISET_FOR_EACH(&pre, fact) // For each of the facts in pre
        pddlISetAdd(&h->fact[fact].pre_op, h->op_goal); // Add them to the preconditions of op_goal
    op->pre_size = pddlISetSize(&pre); // Update the size of the preconditions in op_goal to match

    pddlISetFree(&pre); // Free memory from pre as we are now done with it
}

/* Initially mark all facts as dead ends */
static void initFacts(pddl_h1_t *h) {
    for (int i = 0; i < h->fact_size; ++i) {
        FVALUE_INIT(h->fact + i);
    }
}

/* Initialise the "unsatisfied" field for all operators to match the size of all its preconditions
(none are shown to be satisfied yet)*/
static void initOps(pddl_h1_t *h) {
    for (int i = 0; i < h->op_size; ++i) {
        h->op[i].unsat = h->op[i].pre_size;
    }
}

/* Push all facts with no preconditions onto the priority queue C */
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
static void enqueueOpEffects(pddl_h1_t *h,
                             pddl_h1_op_t *op,
                             int fact_val,
                             pddl_pq_t *C) {
    //Add h-value of the given fact to the cost of the operator
    int val = op->cost + fact_val;
    
    /*For each fact in the effect of the operator, 
    if its h-value in priority queue C is higher than the newly computed value,
    push this new h-value to C */ 
    int fid;
    PDDL_ISET_FOR_EACH(&op->eff, fid) {
        pddl_h1_fact_t *fact = h->fact + fid;
        if (FVALUE(fact) > val)
            FPUSH(C, val, fact);
    }
}

/*Parameters: Takes h1 object, a state s and variables from the FDR, 
Returns: Heuristic value for state s */
int pddlH_1(pddl_h1_t *h,
                  const int *s,
                  const pddl_fdr_vars_t *vars) 
{
    /*Define and initialize priority queue C 
    which holds facts sorted by current h-value */
    pddl_pq_t C;
    pddlPQInit(&C);

    //Initialize facts, operators and inital state for h1 object h
    initFacts(h);
    initOps(h);
    addInitState(h, s, vars, &C);

    //While the priority queue C is not empty, compute h-value for s
    while (!pddlPQEmpty(&C)) {
        //Set aside memory for the value of the fact we are processing
        int val;
        //Pop the lowest value element from the queue
        pddl_pq_el_t *el = pddlPQPop(&C, &val);
        //Get pointer to the outer container (entire fact which the popped element references)
        pddl_h1_fact_t *fact = pddl_container_of(el, pddl_h1_fact_t, heap);

        //Get ID of the fact (?)
        int fact_id = FID(h, fact);
        //If this fact is a goal fact, break from the while loop
        if (fact_id == h->fact_goal)
            break;

        //Go through each operator which has a precondition satisfiable by the current fact
        int op_id;
        PDDL_ISET_FOR_EACH(&fact->pre_op, op_id) {
            pddl_h1_op_t *op = h->op + op_id;
            //If this was the last unsatisfied precondition for this operator, enqueue the facts in the operator's effects
            if (--op->unsat == 0)
                enqueueOpEffects(h, op, val, &C);
        }
    }
    
    //The h-value has been computed and is stored in the heap, so we can free memory from the queue
    pddlPQFree(&C);

    //Return the computed h-value, or dead end value if no path was found
    if (FVALUE_IS_SET(h->fact + h->fact_goal))
        return FVALUE(h->fact + h->fact_goal);
    else 
        return PDDL_COST_DEAD_END;
}
