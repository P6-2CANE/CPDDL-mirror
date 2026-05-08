#include "pddl/h1.h"
#include "internal.h"
#include "_heur.h"

typedef struct pddl_heur_h1 {
    pddl_heur_t heur;
    pddl_fdr_vars_t fdr_vars; 
    pddl_h1_t h1; 
} pddl_heur_h1_t;

// Free the memory of the h object as well as the memory of its pointers
static void heurDel(pddl_heur_t *_h) {
    pddl_heur_h1_t *h = pddl_container_of(_h, pddl_heur_h1_t, heur); 
    _pddlHeurFree(&h->heur); 
    pddlFDRVarsFree(&h->fdr_vars); 
    pddlH1Free(&h->h1); 
    FREE(h);
}

static int heurEstimate(pddl_heur_t *_h,
                        const pddl_fdr_state_space_node_t *node,
                        const pddl_fdr_state_space_t *state_space)
{
    pddl_heur_h1_t *h = pddl_container_of(_h, pddl_heur_h1_t, heur);
    return pddlH_1(&h->h1, node->state, &h->fdr_vars);
}

/* First, initialise the h1 heuristic object and then initalise the heuristic object with facts and operators from the fdr.
Then copy the variables from the fdr into the heuristic object. */
pddl_heur_t *pddlHeurH1(const pddl_fdr_t *fdr, pddl_err_t *err){

    pddl_heur_h1_t *h = ZALLOC(pddl_heur_h1_t);

    pddlH1Init(&h->h1, fdr);

    pddlFDRVarsInitCopy(&h->fdr_vars, &fdr->var);

    _pddlHeurInit(&h->heur, heurDel, heurEstimate);

    return &h->heur;
}