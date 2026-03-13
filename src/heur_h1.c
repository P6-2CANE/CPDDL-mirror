#include "pddl/h1.h"
#include "internal.h" /* used for ZALLOC etc. */
#include "_heur.h" /* used for pddl_heur_t */

/* This struct declaration and typedef cannot be in the h1.h header file */
typedef struct pddl_heur_h1 {
    pddl_heur_t heur; /* Maybe: The heuristic values */
    pddl_fdr_vars_t fdr_vars; /* Maybe: The variables/facts from the fdr */
    pddl_h1_t h1; /* relevant facts and operators from fdr */
} pddl_heur_h1_t;

void printHeurH1Struct(pddl_heur_h1_t *h, pddl_err_t *err); 

/* Free the memory of the h object as well as the memory of its pointers */
static void heurDel(pddl_heur_t *_h) {
    pddl_heur_h1_t *h = pddl_container_of(_h, pddl_heur_h1_t, heur); /* Find the container/place in memory where h is stored */
    //_pddlHeurFree(&h->heur); /* Empty function???? */
    pddlFDRVarsFree(&h->fdr_vars); /* Free the memory of the fdr variable fields that are pointers */
    pddlH1Free(&h->h1); /* Free the memory of the fields in the h1 object that are pointers */
    FREE(h); /* Free the memory of the h object */
}

pddl_heur_t *pddlHeurH1(const pddl_fdr_t *fdr, pddl_err_t *err){

    //initialise the h1 heuristic object
    pddl_heur_h1_t *h = ZALLOC(pddl_heur_h1_t);
    PDDL_LOG(err, "Before initializing h1 heuristic:\n");
    printHeurH1Struct(h, err);

    /* Initalise the heuristic object with facts and operators from the fdr */
    pddlH1Init(&h->h1, fdr);
    PDDL_LOG(err,"After initializing h1 heuristic:\n");
    printHeurH1Struct(h, err);
    
    /* Copy the variables from the fdr into the heuristic object */
    pddlFDRVarsInitCopy(&h->fdr_vars, &fdr->var);
    PDDL_LOG(err,"Initialized h1 heuristic with %d facts and %d operators\n", h->h1.fact_size, h->h1.op_size);
    
    PDDL_LOG(err,"Efter frigjort h1 heuristik\n");
    FREE(h);
    printHeurH1Struct(h, err);

    PDDL_LOG(err,"FDR vars:\n");

    return NULL;
}



void printHeurH1Struct(pddl_heur_h1_t *h, pddl_err_t *err) {
    if (!h) {
        PDDL_LOG(err, "printHeurH1Struct: h is NULL\n");
        return;
    }
    pddl_h1_t h1 = h->h1; 

    PDDL_LOG(err, "Printing the h1 heuristic struct:\n fact size: %d\n fact goal: %d\n fact nopre: %d\n operation size: %d\n operation goal: %d\n",
         h1.fact_size, 
         h1.fact_goal, 
         h1.fact_nopre, 
         h1.op_size, 
         h1.op_goal
    );

    PDDL_LOG(err, "Giving the facts:\n");
    for (int i = 0; i < h1.fact_size; i++)
    {
        printf("\n|fact iter: %10d\n", i);
        printf("|Size: %8d |Alloc: %8d |\n", 
            h1.fact[i].pre_op.size, 
            h1.fact[i].pre_op.alloc
        );

        for (int k = 0; k < 67; k++) { printf("="); }
        printf("\n");
        int count = 0;
        if (h1.fact[i].pre_op.s != NULL) {
            for (int j = 0; j < h1.fact[i].pre_op.size; j++)
            {
                if (h1.fact[i].pre_op.s[j] == 0) {
                    printf("|N/A");
                } 
                else {
                    printf("|%10d", h1.fact[i].pre_op.s[j]);
                }
                count++;
                if (count % 6 == 0 || j == h1.fact[i].pre_op.size - 1) {
                    printf("|\n");
                }
            }
        } else {
            printf("| No pre_op data |\n");
        }
        for (int k = 0; k < 67; k++) { printf("="); }
        printf("\n");
    }

    PDDL_LOG(err, "Giving the operations:\n");
    printf("| OPERATIONS\n");
    printf("| Effect size | Cost | Precondition size | Unsatisfied conditions |\n");
    for (int k = 0; k < 67; k++) { printf("="); }
    for (int i = 0; i < h1.op_size; i++)
    {
        printf("\n|%11d |%4d |%18d |%22d |", 
            h1.op[i].eff.size, 
            h1.op[i].cost, 
            h1.op[i].pre_size, 
            h1.op[i].unsat
        );
    }
    printf("\n");
    for (int k = 0; k < 67; k++) { printf("="); }
    printf("\n");
}
