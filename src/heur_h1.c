#include "pddl/h1.h"
#include "internal.h"
#include "_heur.h" //use this for pddl_heur_t

/* This struct declaration and typedef cannot be in the h1.h header file */
typedef struct pddl_heur_h1 {
    pddl_heur_t heur; /*!< Maybe: The heuristic values */
    pddl_fdr_vars_t fdr_vars; /*!< Maybe: The variables/facts from the fdr */
    pddl_h1_t h1;
} pddl_heur_h1_t;

pddl_heur_t *pddlHeurH1(const pddl_fdr_t *fdr, pddl_err_t *err){

    pddl_heur_h1_t *h = ZALLOC(pddl_heur_h1_t);

    printf("Nu laver vi h1 heuristikken\n");
    printf("Fact size: %d\n", h->h1.fact_size);
    return NULL;
}
