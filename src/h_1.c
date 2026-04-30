#include "pddl/h1.h"
#include "internal.h" 

#define FID(h, f) ((f) - (h)->fact)
#define FVALUE(fact) (fact)->heap.key
#define FVALUE_SET(fact, val) do { (fact)->heap.key = val; } while(0)
#define FVALUE_INIT(fact) FVALUE_SET((fact), PDDL_COST_DEAD_END)
#define FVALUE_IS_SET(fact) (FVALUE(fact) != PDDL_COST_DEAD_END)

// Push fact onto priority queue C (or simply update if it was already set)
#define FPUSH(C, val, fact) \
    do { \
        if (FVALUE_IS_SET(fact)){ \
            pddlPQUpdate((C), (val), &(fact)->heap); \
        } else { \
            pddlPQPush((C), (val), &(fact)->heap); \
        } \
    } while (0)

// Free the memory of the fields in the h1 object that are pointers
void pddlH1Free(pddl_h1_t *h1) {
    for (int i = 0; i < h1->fact_size; ++i) 
        pddlISetFree(&h1->fact[i].pre_op); 
    if (h1->fact != NULL)
        FREE(h1->fact); 

    for (int i = 0; i < h1->op_size; ++i)
        pddlISetFree(&h1->op[i].eff);
    if (h1->op != NULL)
        FREE(h1->op);
}

/* Allocate memory for h1 object. First, allocate facts and add one for empty-precondition fact and one for goal fact. 
Then, allocate operators and add one artificial for goal. Afterwards, iterate through operators in the fdr.
If the operator has no preconditions, associate it with the "nopre" fact we initialised earlier.
Lastly, we initialize fact_goal and op_goal, which mark that a goal state has been achieved.
The operator op_goal has all the actual goal facts from FDR as its preconditions,
applying it costs nothing, and the effect is the artificial goal_fact.
In this way, a single fact can represent that all goal facts were indeed achieved. */
void pddlH1Init(pddl_h1_t *h, const pddl_fdr_t *fdr) {


    h->fact_size = fdr->var.global_id_size + 2;
    h->fact = ZALLOC_ARR(pddl_h1_fact_t, h->fact_size);
    h->fact_goal = h->fact_size - 2;
    h->fact_nopre = h->fact_size - 1;

    h->op_size = fdr->op.op_size + 1;
    h->op = ZALLOC_ARR(pddl_h1_op_t, h->op_size);
    h->op_goal = h->op_size - 1;

    PDDL_ISET(pre);

    for (int op_id = 0; op_id < fdr->op.op_size; ++op_id){

        const pddl_fdr_op_t *src = fdr->op.op[op_id];
        pddl_h1_op_t *op = h->op + op_id;

        pddlFDRPartStateToGlobalIDs(&src->eff, &fdr->var, &op->eff);
        op->cost = src->cost;

        pddlISetEmpty(&pre);
        pddlFDRPartStateToGlobalIDs(&src->pre, &fdr->var, &pre);
        
        int fact;
        PDDL_ISET_FOR_EACH(&pre, fact)
            pddlISetAdd(&h->fact[fact].pre_op, op_id);
        op->pre_size = pddlISetSize(&pre);

        if (op->pre_size == 0){ 
            pddlISetAdd(&h->fact[h->fact_nopre].pre_op, op_id);
            op->pre_size = 1;
        }
    }

    pddl_h1_op_t *op = h->op + h->op_goal;
    pddlISetAdd(&op->eff, h->fact_goal);
    op->cost = 0;

    pddlISetEmpty(&pre);
    pddlFDRPartStateToGlobalIDs(&fdr->goal, &fdr->var, &pre);
    int fact;
    PDDL_ISET_FOR_EACH(&pre, fact)
        pddlISetAdd(&h->fact[fact].pre_op, h->op_goal);
    op->pre_size = pddlISetSize(&pre);

    pddlISetFree(&pre);
}

// Initially mark all facts as dead ends
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

// Push all facts with no preconditions onto the priority queue C
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

// Apply effects of fully satisfied operators, enqueue all facts that are now achievable
static void enqueueOpEffects(pddl_h1_t *h,
                             pddl_h1_op_t *op,
                             int fact_val,
                             pddl_pq_t *C) {
    int val = op->cost + fact_val;
    
    int fid;
    PDDL_ISET_FOR_EACH(&op->eff, fid) {
        pddl_h1_fact_t *fact = h->fact + fid;
        if (FVALUE(fact) > val)
            FPUSH(C, val, fact);
    }
}

/* Parameters: Takes h1 object, a state s and variables from the FDR
Returns: the computed heuristic value for state s, or dead end value if no path was found */
int pddlH_1(pddl_h1_t *h,
                  const int *s,
                  const pddl_fdr_vars_t *vars) 
{
    pddl_pq_t C;
    pddlPQInit(&C);

    initFacts(h);
    initOps(h);
    addInitState(h, s, vars, &C);

    while (!pddlPQEmpty(&C)) {
        int val;
        pddl_pq_el_t *el = pddlPQPop(&C, &val);
        pddl_h1_fact_t *fact = pddl_container_of(el, pddl_h1_fact_t, heap);

        int fact_id = FID(h, fact);
        if (fact_id == h->fact_goal)
            break;

        int op_id;
        PDDL_ISET_FOR_EACH(&fact->pre_op, op_id) {
            pddl_h1_op_t *op = h->op + op_id;
            if (--op->unsat == 0)
                enqueueOpEffects(h, op, val, &C);
        }
    }
    

    pddlPQFree(&C);

    if (FVALUE_IS_SET(h->fact + h->fact_goal))
        return FVALUE(h->fact + h->fact_goal);
    else 
        return PDDL_COST_DEAD_END;
}
