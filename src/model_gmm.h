/*
 *  model_gmm.h
 *  
 *  Created by Ania M. Kedzierska on 11/11/11.
 *  Copyright 2011 Politecnic University of Catalonia, Center for Genomic Regulation.  This is program can be redistributed, modified or else as given by the terms of the GNU General Public License. 
 *  
 */


#ifndef __MODEL_GMM_H__
#define __MODEL_GMM_H__

#include "parameters.h"
#include "permutation.h"
#include <list>

void GMM_mle_edge(TMatrix &N, TMatrix &tm);
void GMM_mle_root(Root &s, Root &r);
void GMM_random_edge(TMatrix &tm);
void GMM_random_edge_length(double len, TMatrix &tm);
void GMM_random_edge_bio_length(double len, TMatrix &tm);
void GMM_random_root(Root &r);
long GMM_matrix_structure(long i, long j);
long GMM_root_structure(long i);
void GMM_list_permutations(std::list<Permutation> &L);

#endif
