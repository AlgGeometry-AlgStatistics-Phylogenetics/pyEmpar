/*
 *  Empar.cpp
 *  
 *  Created by Ania M. Kedzierska on 11/11/11.
 *  Copyright 2011 Politecnic University of Catalonia, Center for Genomic Regulation.  This is program can be redistributed, modified or else as given by the terms of the GNU General Public License. 
 *  
 */

#include<iostream>
#include<cstdlib>
#include<fstream>
#include<ctime>
#include<cmath>
#include<list>

#include "em.h"
#include "tree.h"
#include "parameters.h"
#include "alignment.h"
#include "model.h"
#include "sampling.h"
#include "miscelania.h"
#include "random.h"
#include "fisher.h"
#include "permutation.h"

void help(void) {
  std::cout << "Usage: Empar <tree file> <fasta file> <model>" << std::endl;
  std::cout << std::endl;
}


// Prints an informative warning if the tree is nonidentifiable.
bool nonident_warning(Tree &T) {
  std::list<long> Lnon;
  std::list<long>::iterator it;
  std::list<long>::iterator itlast;

  for(long i = 0; i < T.nnodes; i++) {
    // Any node (root or not) with valence 2 leads to nonident.
    if(valence(T, i) == 2) {
      Lnon.push_back(i);
    }
    
    // A root of valence 1 leads to nonident.
    if(valence(T, i) == 1 && i == T.nleaves) {
      Lnon.push_back(i);
    }
  }

  if (Lnon.size() > 0) {
    std::cout << "WARNING: The following nodes lead to non-identifiability of the parameters: ";
    itlast = Lnon.end();
    --itlast;
    for(it=Lnon.begin(); it != Lnon.end(); it++) {
      std::cout << *it;
      if (it != itlast) {     // if not the last element
        std::cout << ", ";
      }
    }
    std::cout << "." << std::endl << std::endl;

    std::cout << "This may happen for two reasons:" << std::endl;
    std::cout << " 1) The root has valence 1. If the model has non-uniform distribution, the length of the outgoing edge cannot be recovered reliably." << std::endl;
    std::cout << " 2) There is a node (typically thought of as the root) with exactly two incident edges. In this case only the sum of lengths of the two incident edges can be recovered reliably." << std::endl << std::endl;
  }

  return Lnon.size() > 0;
}


// Uses the EM algorithm to estimate the parameters for a given
// alignment. Additionally, puts the estimated parameters in a file with
// the same name as the fasta file, but ending with ".dat".

int main(int argc, char* argv[]){
  Model Mod;                 // The model
  Tree T;                    // the tree
  Counts data;               // the counts
  Parameters Par;            // the parameters
  std::vector<double> br;    // branch lengths
  double eps = 1e-8;         // The threshold for the EM algorithm.

  Parameters Parsim;         // used for simulating data.
  std::vector<double> brsim; // branch lengths of simulated data. 

  std::vector<std::vector<double> > Cov;  // Covariance matrix
  std::vector<double> variances;          // The variances

  std::string tree_filename; // Filenames.
  std::string fasta_filename;
  std::string parameters_filename;
  std::string covariances_filename;
  std::string model_name;

  bool simulate;
  bool nonident;

  // sets the precision in which to print floats and doubles.
  std::cout.precision(15);

  // initialize random number generator with time(0).
  random_initialize();
  // random_initialize(23);


  // Check for command line arguments
  if (argc == 1) {
    std::cout << "No input arguments. Tree and MSA in fasta format requiered." << std::endl;
    help();
    exit(1);
  } else if (argc >= 5) {
    std::cout << "Too many arguments." << std::endl;
    help();
    exit(1);
  }

  // Catch the command line arguments
  tree_filename.assign(argv[1]);
  fasta_filename.assign(argv[2]);
  parameters_filename = strip_extension(fasta_filename) + ".dat";
  covariances_filename = strip_extension(fasta_filename) + ".cov";
  model_name.assign(argv[3]);

  // Creates the pointers to the model-specific functions.
  Mod = create_model(model_name);
  std::cout << "Model: " << Mod.name << std::endl;

  // Reads the tree.
  read_tree(T, tree_filename);      

  // Prints the Tree
  std::cout << "Tree:" << std::endl;
  print_tree(T);                    

  // Check for possible nonidentifiability issues.
  nonident = nonident_warning(T);

  // Initialize the parameters for simulation of K81 data for testing
  create_parameters(Parsim, T);

  if (fasta_filename == ":test") {      // if fasta file is :test generate random data.
    simulate = true;

    // Warn
    std::cout << "WARNING: Using simulated data " << std::endl << std::endl;

    // Generate random parameters
    random_parameters_length(T, Mod, Parsim);
    //    read_parameters(Parsim, "bla0.sim.dat");

    // Simulate the data
    random_fake_counts(T, 1000, data, Parsim);

    // Prints branch-lengths for future check.
    branch_lengths(Parsim, brsim);
    std::cout << "Simulated branch lengths:" << std::endl;
    print_vector(brsim);

  } else {                                  // otherwise read the data
    simulate = false;

    // Read the counts.
    std::cout << "Reading fasta file:" << std::endl;
    read_counts(T, data, fasta_filename);
    add_pseudocounts(0.01, data);
    std::cout << std::endl;
  }
 
  // Check whether the data and the tree match.
  if (T.nalpha != data.nalpha || T.nleaves != data.nspecies) {
    std::cout << "The order of the sequences or their number and the phylogenetic tree do not match." << std::endl;
    exit(1);
  }

  // Assigns memory for the parameters.
  create_parameters(Par, T);

  // Chose initial parameters at random
  //  random_parameters(Mod, Par);

  // fixed initial conditions
  initial_parameters(Mod, Par);


  // Check the time
  clock_t start_time, end_time;
  start_time = clock();
  std::cout << "Starting the EM algorithm" << std::endl;

  // Runs the EM algorithm. Par is used as initial parameters.
  // After execution, Par contains the MLE computed by the algorithm.
  EMalgorithm(T, Mod, Par, data, eps);
  
  // Choses the best permutation.
  guess_permutation(T, Mod, Par);

  // Check the end time
  end_time = clock();

  // Compute branch lengths
  branch_lengths(Par, br);
  
  // If parameters are not identifiable, the computation of the covariance matrix will
  // fail as the Fisher info matrix will not be invertible.
  if (!nonident) {
    // Compute the covariance matrix using observed Fisher.
    full_MLE_observed_covariance_matrix(T, Mod, Par, data, Cov);
    variances.resize(Cov.size());
    for(unsigned int i=0; i < Cov.size(); i++) {
      variances[i] = Cov[i][i];
    }

    // Save the sigmas into a file
    std::fstream fcov;
    fcov.precision(15);
    fcov.open(covariances_filename.c_str(), std::ios::out);
    if (!fcov.is_open()) {
      std::cout << "Could not open file: covariances.dat" << std::endl;
    }
    for(unsigned int i=0; i < Cov.size(); i++) {
      for(unsigned int j=0; j < Cov.size(); j++) {
        fcov << Cov[i][j] << " ";
      }
      fcov << std::endl;
    }
    fcov << std::endl;
    fcov.close();
  }

  std::cout << std::endl;
  std::cout << "Finished." << std::endl;
  std::cout << "Elapsed time: " << ((double)(end_time - start_time)) / CLOCKS_PER_SEC << " s" << std::endl;
  std::cout << std::endl;
  std::cout << "Likelihood: " << log_likelihood(T, Par, data) << std::endl << std::endl;
  std::cout << "Branch lengths: " << std::endl;
  print_vector(br);

  if (!nonident) {
    std::cout << "Parameter variances: " << std::endl;
    print_vector(variances);
  }

  std::cout << "Newick Tree:" << std::endl;
  print_newick_tree(T, br);

  // if is a simulation, print the L2 distance !
  if (simulate) {
    std::cout << "L2 distance:   " << parameters_distance(Par, Parsim) << std::endl;
    std::cout << "KL divergence: " << KL_divergence(T, Par, Parsim) << std::endl;
    std::cout << std::endl;
  }

  // if it is not a simulation, store the parameters in a file !
  if (!simulate) {
    std::fstream st;
    st.precision(15);
    st.setf(std::ios::fixed,std::ios::floatfield); 
    st.open(parameters_filename.c_str(), std::ios::out);
    print_parameters(Par, st);
  }
}
