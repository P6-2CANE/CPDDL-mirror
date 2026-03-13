#include "pddl/h1.h"
#include "internal.h" /* used for ZALLOC etc. */
#include "_heur.h" /* used for pddl_heur_t */

/* This struct declaration and typedef cannot be in the h1.h header file */
typedef struct pddl_heur_h1 {
    pddl_heur_t heur; /* Maybe: The heuristic values */
    pddl_fdr_vars_t fdr_vars; /* Maybe: The variables/facts from the fdr */
    pddl_h1_t h1; /* relevant facts and operators from fdr */
} pddl_heur_h1_t;

void printHeurH1Struct(pddl_heur_h1_t *h);

static void heurDel(pddl_heur_t *_h) {
    pddl_heur_h1_t *h = pddl_container_of(_h, pddl_heur_h1_t, heur); /* Find the container/place in memory where h is stored */
    //_pddlHeurFree(&h->heur); /* Empty function???? */
    pddlFDRVarsFree(&h->fdr_vars); /* Free the memory of the pointers in the fdr variables */
    pddlH1Free(&h->h1); /* Free the memory of the pointers in the h1 object */
    FREE(h); /* Free the actual memory of the h object */
}

pddl_heur_t *pddlHeurH1(const pddl_fdr_t *fdr, pddl_err_t *err){

    //initialise the h1 heuristic object
    pddl_heur_h1_t *h = ZALLOC(pddl_heur_h1_t);
    printf("før init:\n");
    printHeurH1Struct(h);

    /* Initalise the heuristic object with facts and operators from the fdr */
    pddlH1Init(&h->h1, fdr);
    printf("Efter init:\n");
    printHeurH1Struct(h);
    
    printf("før variabel kopi:\n");
    pddlFDRVarsPrintTable(&h->fdr_vars, 10, NULL, err);
    
    printf("efter variabel kopi:\n");
    /* Copy the variables from the fdr into the heuristic object */
    pddlFDRVarsInitCopy(&h->fdr_vars, &fdr->var);
    
    //print the fdr varriables
    pddlFDRVarsPrintTable(&h->fdr_vars, 10, NULL, err);

    printf("Nu laver vi h1 heuristikken\n");
    
    printf("Efter frigjort h1 heuristik\n");
    FREE(h);
    printHeurH1Struct(h);

    printf("FDR vars:\n");
    pddlFDRVarsPrintTable(&h->fdr_vars, 10, NULL, err);

    return NULL;
}

void printHeurH1Struct(pddl_heur_h1_t *h){
    pddl_h1_t h1 = h->h1; 

    printf("Fact size: %d, Fact goal: %d, Fact nopre: %d, Fact operation size: %d, Fact operation goal: %d\n",
         h1.fact_size, 
         h1.fact_goal, 
         h1.fact_nopre, 
         h1.op_size, 
         h1.op_goal
    );
    
    printf("Printing the facts:\n");
         for (int i = 0; i < h1.fact_size; i++)
    {
        printf("Size: %d, Alloc: %d, ", 
            h1.fact[i].pre_op.size, 
            h1.fact[i].pre_op.alloc
        );
        printf("Facts:");

        for (int j = 0; j < h1.fact[i].pre_op.size; i++)
        {
            printf("s: %d, ", h1.fact[j].pre_op.s[j]);
        }

        printf("\n");
    }

    printf("Printing the operations:\n");
    for (int i = 0; i < h1.op_size; i++)
    {
        printf("Cost: %d, Precondition size: %d, Unsatisfied conditions: %d\n", 
            h1.op[i].cost, 
            h1.op[i].pre_size, 
            h1.op[i].unsat
        );
    }
}
