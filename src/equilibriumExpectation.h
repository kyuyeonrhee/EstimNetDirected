#ifndef EQUILIBRIUMEXPECTATION_H
#define EQUILIBRIUMEXPECTATION_H
/*****************************************************************************
 * File:    equilibriumExpectation.c
 * Author:  Alex Stivala, Maksym Byshkin
 * Created: October 2017
 *
 * Equilibirum expectation algorithm for ERGM estimation of directed graphs.
 * In fact there are two (very similar) algorithms: Algorithm S for
 * simulated networks (i.e. those generated by an ERGM process) and Algorithm
 * EE for empirical networks. Algorithm S is used to get starting parameters
 * for Algorithm EE.
 *
 * Reference for the algorithm (originally for undirected networks) is
 *
 *   Byshkin M, Stivala A, Mira A, Robins G, Lomi A 2018  "Fast
 *   maximum likelihood estimation via equilibrium expectation for
 *   large network data". Scientific Reports 8:11509
 *   doi:10.1038/s41598-018-29725-8
 *
 ****************************************************************************/

#include "configparser.h"
#include "changeStatisticsDirected.h"

void algorithm_S(digraph_t *g, uint_t n, uint_t n_attr, uint_t n_dyadic,
                 change_stats_func_t *change_stats_funcs[],
                 attr_change_stats_func_t *attr_change_stats_funcs[],
                 dyadic_change_stats_func_t *dyadic_change_stats_funcs[],
                 uint_t attr_indices[],
                 uint_t M1,
                 uint_t sampler_m,
                 double ACA,
                 double theta[],
                 double Dmean[],
                 FILE *theta_outfile,
                 bool useIFDsampler, double ifd_K,
                 bool useConditionalEstimation);

void algorithm_EE(digraph_t *g, uint_t n, uint_t n_attr, uint_t n_dyadic,
                  change_stats_func_t *change_stats_funcs[],
                  attr_change_stats_func_t *attr_change_stats_funcs[],
                  dyadic_change_stats_func_t *dyadic_change_stats_funcs[],
                  uint_t attr_indices[],
                  uint_t Mouter, uint_t Minner,
                  uint_t sampler_m,
                  double ACA, double compC,
                  double D0[],
                  double theta[],
                  FILE *theta_outfile, FILE *dzA_outfile, bool outputAllSteps,
                  bool useIFDsampler, double ifd_K,
                  bool useConditionalEstimation);


void ee_estimate(digraph_t *g, uint_t n, uint_t n_attr, uint_t n_dyadic,
                 change_stats_func_t *change_stats_funcs[],
                 attr_change_stats_func_t *attr_change_stats_funcs[],
                 dyadic_change_stats_func_t *dyadic_change_stats_funcs[],
                 uint_t attr_indices[],
                 uint_t sampler_m, uint_t M1_steps, uint_t Mouter,
                 uint_t Msteps, double ACA_S, double ACA_EE, double compC,
                 double theta[], uint_t tasknum,
                 FILE *theta_outfile, FILE *dzA_outfile, bool outputAllSteps,
                 bool useIFDsampler, double ifd_K,
                 bool useConditionalEstimation);

int do_estimation(config_t *config, uint_t tasknum);


#endif /* EQUILIBRIUMEXPECTATION_H */

