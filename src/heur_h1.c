#include "pddl/h1.h"
#include "internal.h"
#include "_heur.h" //use this for pddl_heur_t

/* This struct declaration and typedef cannot be in the h1.h header file */
typedef struct pddl_heur_h1 {
    pddl_heur_t heur; /*!< Maybe: The heuristic values */
    pddl_fdr_vars_t fdr_vars; /*!< Maybe: The variables/facts from the fdr */
    pddl_h1_t h1;
} pddl_heur_h1_t;

void printHeurH1Struct(pddl_heur_h1_t *h);

pddl_heur_t *pddlHeurH1(const pddl_fdr_t *fdr, pddl_err_t *err){

    //initialise the h1 heuristic object
    pddl_heur_h1_t *h = ZALLOC(pddl_heur_h1_t);
    printf("Før Zeroize:\n");
    printHeurH1Struct(h);
    ZEROIZE(h);
    printf("Efter Zeroize:\n");
    printHeurH1Struct(h);

    //print the fdr varriables
    /* pddlFDRVarsPrintTable(&fdr->var, 10, NULL, err); */

    printf("Nu laver vi h1 heuristikken\n");
    printf("Fact size: %d\n", h->h1.fact_size);
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
