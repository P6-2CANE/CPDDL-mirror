#include "pddl/h2.h"
#include "internal.h"
#include "_heur.h"

typedef struct pddl_heur_h2 {
    pddl_heur_t heur;
    pddl_fdr_vars_t fdr_vars;
    pddl_h2_t h2;
} pddl_heur_h2_t;

static void heurDel(pddl_heur_t *_h) {
    pddl_heur_h2_t *h = pddl_container_of(_h, pddl_heur_h2_t, heur); 
    _pddlHeurFree(&h->heur);
    pddlFDRVarsFree(&h->fdr_vars);
    pddlH2Free(&h->h2); 
    FREE(h);
}

static int heurEstimate(pddl_heur_t *_h,
                        const pddl_fdr_state_space_node_t *node,
                        const pddl_fdr_state_space_t *state_space)
{
    pddl_heur_h2_t *h = pddl_container_of(_h, pddl_heur_h2_t, heur);
    return pddlH_2(&h->h2, node->state, &h->fdr_vars);
}

pddl_heur_t *pddlHeurH2(const pddl_fdr_t *fdr, pddl_err_t *err){

    pddl_heur_h2_t *h = ZALLOC(pddl_heur_h2_t);

    pddlH2Init(&h->h2, fdr);

    pddlFDRVarsInitCopy(&h->fdr_vars, &fdr->var);

    _pddlHeurInit(&h->heur, heurDel, heurEstimate);

    return &h->heur;
}
