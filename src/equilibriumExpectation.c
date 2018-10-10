/*****************************************************************************
 * 
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
 * The main difference between the algorithms is that Algorithm S does 
 * not actually perform the MCMC moves in the sampler, while algorithm EE does,
 * and Algoritm EE accumulates the change dzA values, which are zeroed
 * every iteration in Algorithm S (see reference below).
 *
 * Reference for the algorithm (originally for undirected networks) is
 *
 *   Byshkin M, Stivala A, Mira A, Robins G, Lomi A 2018 "Fast
 *   maximum likelihood estimation via equilibrium expectation for
 *   large network data". Scientific Reports 8:11509
 *   doi:10.1038/s41598-018-29725-8
 *
 *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <limits.h>
#include "utils.h"
#include "digraph.h"
#include "basicSampler.h"
#include "ifdSampler.h"
#include "equilibriumExpectation.h"

FILE *Kafile;  /* FIXME should not be a file global and should be one per task for MPI */

/*
 * Algorithm S for estimating parameters of digraph generated by ERGM,
 * and estimating the derivatives for use in Algorithm EE.
 *
 * Parameters:
 *   g      - digraph object.
 *   n      - number of parameters (length of theta vector = total
 *            length of all change statistics functions)
 *   n_attr - number of attribute change stats funcs
 *   n_dyadic -number of dyadic covariate change stats funcs
 *   change_stats_funcs - array of pointers to change statistics functions
 *                        length is n - n_attr
 *   attr_change_stats_funcs - array of pointers to change statistics functions
 *                             length is n_attr
 *   dyadic_change_stats_funcs - array of pointers to dyadic change stats funcs
 *                             length is n_dyadic
 *   attr_indices   - array of n_attr attribute indices (index into g->binattr
 *                    or g->catattr) corresponding to attr_change_stats_funcs
 *                    E.g. for Sender effect on the first binary attribute,
 *                    attr_indices[x] = 0 and attr_change_stats_funcs[x] =
 *                    changeSender
 *   M1          - Number of iterations of Algorithm S
 *   sampler_m   - Number of proposals (sampling iterations) [per step of Alg.S]
 *   ACA         -  multiplier of da to get K1A step size multiplier 
 *   theta  - (Out) array of n parameter values corresponding to
 *                  change stats funcs. Allocated by caller.
 *   Dmean - (Out) array of n derivative estimate values corresponding to theta.
 *                 Allocated by caller
 *   tasknum - task number (MPI rank)
 *   theta_outfile - open (write) file to write theta values to
 *   useIFDsampler - use IFD sampler instead of basic sampler
 *   ifd_K         - constant for multipliying IFD auxiliary parameter
 *                   (only used if useIFDsampler is True).
 *
 * Return value:
 *   None.
 *
 * The theta and Dmean array parameters, which must be allocted by caller,
 * are set to the parameter estimtes and derivative estimtes respectively.
 */

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
                 FILE * theta_outfile,
                 bool useIFDsampler,
                 double ifd_K)
{
  uint_t t, l;
  double acceptance_rate;
  double *addChangeStats = (double *)safe_malloc(n*sizeof(double));
  double *delChangeStats = (double *)safe_malloc(n*sizeof(double));
  double *sumChangeStats = (double *)safe_malloc(n*sizeof(double));
  double *dzA = (double *)safe_malloc(n*sizeof(double));
  double *da = (double *)safe_malloc(n*sizeof(double));
  double *theta_step = (double *)safe_malloc(n*sizeof(double));  
  /* 1/D0 is squared derivatives */  
  double *D0 = (double *)safe_calloc(n, sizeof(double));
  double  dzArc; /* (unused) required only for IFD sampler */
  double  arc_correction_val; /* only used for IFD sampler */
  double ifd_aux_param = 0; /* auxiliary parameter for IFD sampler */

  if (useIFDsampler)
    arc_correction_val = arcCorrection(g);

  for (l = 0; l < n; l++)
    theta[l] = 0;
  for (t = 0; t < M1; t++) {
    fprintf(theta_outfile, "%d ", t-M1);
    if (useIFDsampler) {
      acceptance_rate = ifdSampler(g, n, n_attr, n_dyadic, change_stats_funcs,
                                   attr_change_stats_funcs,
                                   dyadic_change_stats_funcs,
                                   attr_indices, theta,
                                   addChangeStats, delChangeStats, sampler_m,
                                   FALSE,
                                   ifd_K, &dzArc, &ifd_aux_param);
      /* Arc parameter for IFD is auxiliary parameter adjusted by correction value */
      fprintf(theta_outfile, "%g ", ifd_aux_param - arc_correction_val);
    } else {
      acceptance_rate = basicSampler(g, n, n_attr, n_dyadic, change_stats_funcs,
                                     attr_change_stats_funcs,
                                     dyadic_change_stats_funcs,
                                     attr_indices, theta,
                                     addChangeStats, delChangeStats, sampler_m,
                                     FALSE);
    }
    for (l = 0; l < n; l++) {
      dzA[l] = delChangeStats[l] - addChangeStats[l];
      ALGS_DEBUG_PRINT(("addChangeStats[%u] = %g delChangeStats[%u] = %g\n",
                        l, addChangeStats[l], l, delChangeStats[l]));
      sumChangeStats[l] = addChangeStats[l] + delChangeStats[l];
      //The expectation of square of change of statistics is computed
      //It approximates the deriviative with respect to parameter
      D0[l] += dzA[l]*dzA[l];
      da[l] = 0;
      if (sumChangeStats[l] < 0 || sumChangeStats[l] > 0)
        da[l] = ACA / (sumChangeStats[l]*sumChangeStats[l]);
      theta_step[l] = (dzA[l] < 0 ? -1 : 1) * da[l] * dzA[l]*dzA[l];
      theta[l] += theta_step[l];
      fprintf(theta_outfile, "%g ", theta[l]);
    }
    fprintf(theta_outfile, "%g\n", acceptance_rate);
  }
  for (l = 0; l < n; l++)
    Dmean[l] = sampler_m / D0[l];
  
  free(D0);
  free(theta_step);
  free(da);
  free(dzA);
  free(sumChangeStats);
  free(delChangeStats);
  free(addChangeStats);
}


/*
 * Algorithm EE for estimating ERGM parameters of arbitrary digraph.
 *
 * Parameters:
 *   g      - digraph object. NB modifed by sampler.
 *   n      - number of parameters (length of theta vector = 
 *            total number of change stats funcs)
 *   n_attr - number of attribute change stats functions
 *   n_dyadic -number of dyadic covariate change stats funcs
 *   change_stats_funcs - array of pointers to change statistics functions
 *                        length is n - n_attr
 *   attr_change_stats_funcs - array of pointers to change statistics functions
 *                              length is n_attr
 *   dyadic_change_stats_funcs - array of pointers to dyadic change stats funcs
 *                             length is n_dyadic
 *   attr_indices   - array of n_attr attribute indices (index into g->binattr
 *                    or g->catattr) corresponding to attr_change_stats_funcs
 *                    E.g. for Sender effect on the first binary attribute,
 *                    attr_indices[x] = 0 and attr_change_stats_funcs[x] =
 *                    changeSender
 *   Mouter     - Number of iteratoins of Algorithm EE (outer loop)
 *   Minner     - Number of iteratoins of Algorithm EE (inner loop)
 *   sampler_m  - Number of proposals (sampling iterations) 
 *                 [per step of Alg.EE]
 *   ACA        - multiplier of D0 to get K_A step size multiplier
 *   compC      - multiplier of sd(theta)/mean(theta) to limit
 *                  theta variance 
 *   D0     -   array of n derivative estimate values corresponding to theta,
 *              results of algorithm_S()
 *   theta  - (In/Out) array of n parameter values corresponding to
 *                  change stats funcs. Input starting values (from 
 *                  alorithm_S(), output EE values.
 *  tasknum - task number (MPI rank)
 *  theta_outfile - open (write) file to write theta values to.
 *  dzA_outfile   - open (write) file to write dzA values to.
 *  outputAllSteps - if True, output theta and dzA values every iteration,
 *                   otherwise only on every outer iteration.
 *  useIFDsampler  - if True, use IFD sampler instead of basic sampler.
 *  ifd_K          - constant for multipliying IFD auxiliary parameter step
 *                   size (only used if useIFDsampler is True).
 *
 * Return value:
 *   None.
 *
 * The theta and Dmean array parameters, which must be allocted by caller,
 * are set to the parameter estimtes and derivative estimtes respectively.
 */
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
                  bool useIFDsampler, double ifd_K)
{
  uint_t touter, tinner, l, t = 0;
  double acceptance_rate;
  double theta_mean, theta_sd;
  double *addChangeStats = (double *)safe_malloc(n*sizeof(double));
  double *delChangeStats = (double *)safe_malloc(n*sizeof(double));
  double *sumChangeStats = (double *)safe_malloc(n*sizeof(double));
  double *da = (double *)safe_malloc(n*sizeof(double));
  double *theta_step = (double *)safe_malloc(n*sizeof(double));
  /* dzA is only zeroed here, and accumulates in the loop */
  double *dzA = (double *)safe_calloc(n, sizeof(double));
  /* each element of thetamatrix is an array of Minner theta[l]
     values, one for each of the 0 <= l < n elements of theta, used to
     accumulate them to compute mean and sd over innter iterations for
     each outer iteration */
  double **thetamatrix = (double **)safe_malloc(n*sizeof(double *));
  double arc_correction_val; /* only used for IFD sampler */
  double dzArc; /* only used for IFD sampler */
  double ifd_aux_param = 0; /* auxiliary parameter for IFD sampler */

  if (useIFDsampler) 
    arc_correction_val = arcCorrection(g);

  for (l = 0; l < n; l++)
    thetamatrix[l] = (double *)safe_malloc(Minner*sizeof(double));

  for (touter = 0; touter < Mouter; touter++) {
    for (tinner = 0; tinner < Minner; tinner++) {
      if (outputAllSteps || tinner == 0) {      
        fprintf(theta_outfile, "%u ", t);
        fprintf(dzA_outfile, "%u ", t);
        if(Kafile) fprintf(Kafile, "%u ", t); /* FIXME should be task local*/
      }
      if (useIFDsampler) {
        acceptance_rate = ifdSampler(g, n, n_attr, n_dyadic, change_stats_funcs, 
                                     attr_change_stats_funcs,
                                     dyadic_change_stats_funcs,
                                     attr_indices, theta,
                                     addChangeStats, delChangeStats, sampler_m,
                                     TRUE, /*Algorithm EE actually does moves */
                                     ifd_K, &dzArc, &ifd_aux_param);
        if (useIFDsampler && (outputAllSteps || tinner == 0)) {
          /* difference of Arc statistic for IFD sampler is just Ndel-Nadd */
          fprintf(dzA_outfile, "%g ", dzArc);
          /* Arc parameter for IFD sampler is auxiliary parameter adjusted */
          fprintf(theta_outfile, "%g ", ifd_aux_param - arc_correction_val);
        }
      } else {
        acceptance_rate = basicSampler(g, n, n_attr, n_dyadic, change_stats_funcs, 
                                      attr_change_stats_funcs,
                                      dyadic_change_stats_funcs,
                                      attr_indices, theta,
                                      addChangeStats, delChangeStats, sampler_m,
                                      TRUE); /*Algorithm EE actually does moves */
      }
      for (l = 0; l < n; l++) {
        dzA[l] += addChangeStats[l] - delChangeStats[l]; /* dzA accumulates */
        ALGEE_DEBUG_PRINT(("addChangeStats[%u] = %g delChangeStats[%u] = %g\n",
                          l, addChangeStats[l], l, delChangeStats[l]));
        da[l] = D0[l] * ACA;
        theta_step[l] = (dzA[l] < 0 ? 1 : -1) * da[l] * dzA[l]*dzA[l];
        theta[l] += theta_step[l];
        if (outputAllSteps || tinner == 0) {
          fprintf(dzA_outfile, "%g ", dzA[l]);
          fprintf(theta_outfile, "%g ", theta[l]);
        }
        thetamatrix[l][tinner] = theta[l];
      }
      if (outputAllSteps || tinner == 0) {      
        fprintf(theta_outfile, "%g\n", acceptance_rate);
        fprintf(dzA_outfile, "\n");
      }
      t++;
    }
    /* get mean and sd of each theta value over inner loop iterations
       and adjust D0 to limit variance of theta (see S.I.) */
    for (l = 0; l < n; l++) {
      theta_mean = mean_and_sd(thetamatrix[l], Minner, &theta_sd);
      /* force minimum magnitude to stop theta sticking at zero */
      /* TODO 0.1 in next two lines was changed in an earlier commit
         from another value with no explanation. It should be made a
         parameter setting instead, or at least a named constant. */
      if (fabs(theta_mean) < 0.1)
          theta_mean = 0.1;
      if(Kafile) fprintf(Kafile, "%g ", theta_sd / fabs(theta_mean)); /* FIXME should be task local*/
      /* theta_sd is a standard deviation so must be non-negative */
      assert(theta_sd >= 0);
      if (theta_sd > 1e-10) { /* TODO make this a parameter */
        /* as per email from Max 21 July 2018, only adjust D0 this way
         * if sd(theta) is large enough */
        D0[l] *= sqrt(compC / (theta_sd / fabs(theta_mean)));
      }
    }
      if(Kafile) { fprintf(Kafile, "\n"); fflush(Kafile);  } /* FIXME should be task local*/
      fflush(dzA_outfile);
      fflush(theta_outfile); 
  }
  for (l = 0; l < n; l++)
    free(thetamatrix[l]);
  free(thetamatrix);
  free(theta_step);
  free(da);
  free(dzA);
  free(sumChangeStats);
  free(delChangeStats);
  free(addChangeStats);
}



/*
 * Estimate ERGM parameters by using Algorithm S followed by Algorithm EE.
 *
 * Parameters:
 *   g      - digraph object. Modifed if performMove is true.
 *   n      - number of parameters (length of theta vector and total
 *            number of change statistics functions)
 *   n_attr - number of attribute change statistics functions
 *   n_dyadic -number of dyadic covariate change stats funcs
 *   change_stats_funcs - array of pointers to change statistics functions
 *                        length is n - n_attr
 *   attr_change_stats_funcs - array of pointers to change statistics functions
 *                             length is n_attr
 *   dyadic_change_stats_funcs - array of pointers to dyadic change stats funcs
 *                             length is n_dyadic
 *   attr_indices   - array of n_attr attribute indices (index into g->binattr
 *                    or g->catattr) corresponding to attr_change_stats_funcs
 *                    E.g. for Sender effect on the first binary attribute,
 *                    attr_indices[x] = 0 and attr_change_stats_funcs[x] =
 *                    changeSender
 *   sampler_m      - sampler iterations (per algorithm step)
 *   M1_steps       - Steps of Algorithm 1 
 *   Mouter         - outer iteration of Algorihtm EE
 *   Msteps         - number of inner steps of Algorithm EE
 *   ACA_S          -  multiplier of da to get K1A step size multiplier 
 *   ACA_EE         - multiplier of D0 to get K_A step size multiplier
 *   compC      - multiplier of sd(theta)/mean(theta) to limit
 *                   theta variance 
 *   theta  - (Out) array of n parameter values corresponding to
 *                  change stats funcs. Allocated by caller.
 *   tasknum - task number (MPI rank)
 *   theta_outfile - open (write) file to write theta values to
 *   dzA_outfile   - open (write) file to write dzA values to
 *   outputAllSteps - in Algorithm EE, output theta and dzA values on every
 *                    iteration, not just every outer iteration.
 *   useIFDsampler  - if true, use the IFD sampler instead of the basic 
 *                    sampler
 *   ifd_K          - consant for multiplying IFD auxiliary parameter
 *                    (only used if useIFDsampler is True).
 *   
 *
 * Return value:
 *   None.
 *
 * The theta and Dmean array parameters, which must be allocted by caller,
 * are set to the parameter estimtes and derivative estimtes respectively.
 */
void ee_estimate(digraph_t *g, uint_t n, uint_t n_attr, uint_t n_dyadic,
                 change_stats_func_t *change_stats_funcs[],
                 attr_change_stats_func_t *attr_change_stats_funcs[],
                 dyadic_change_stats_func_t *dyadic_change_stats_funcs[],
                 uint_t attr_indices[],
                 uint_t sampler_m, uint_t M1_steps, uint_t Mouter,
                 uint_t Msteps, double ACA_S, double ACA_EE, double compC,
                 double theta[], uint_t tasknum,
                 FILE *theta_outfile, FILE *dzA_outfile, bool outputAllSteps,
                 bool useIFDsampler, double ifd_K)
{
  struct timeval start_timeval, end_timeval, elapsed_timeval;
  int            etime;
  uint_t         i;

  /*array of n derivative estimate values corresponding to theta. */  
  double *Dmean = (double *)safe_malloc(n*sizeof(double));

  printf("task %u: ACA_S = %g, ACA_EE = %g, compC = %g, samplerSteps = %u, "
         "Ssteps = %u, EEsteps = %u, EEinnerSteps = %u\n", tasknum,
         ACA_S, ACA_EE, compC, sampler_m, M1_steps, Mouter, Msteps);

  if (useIFDsampler)
    printf("task %u: IFD sampler ifd_K = %g, arcCorrection = %g\n",
           tasknum, ifd_K, arcCorrection(g));

   /* steps of algorithm S (M1_steps adjusted by number of nodes) */
  uint_t M1 = (uint_t)(M1_steps *g->num_nodes / sampler_m);

  /* inner steps of Algorithm EE */
  /*uint_t M = (uint_t)(Msteps *g->num_nodes / sampler_m);*/
  /* as per email from Max 20 July 2018, better to have this as constant
   * rather than scaled by network size */
  uint_t M = Msteps;
  


  printf("task %u: M1 = %u, Mouter = %u, M = %u\n", tasknum, M1, Mouter, M);


  printf("task %u: running Algorithm S...\n", tasknum);
  gettimeofday(&start_timeval, NULL);

  algorithm_S(g, n, n_attr, n_dyadic, change_stats_funcs,
              attr_change_stats_funcs, dyadic_change_stats_funcs, attr_indices, 
              M1, sampler_m, ACA_S, theta, Dmean, theta_outfile, useIFDsampler,
              ifd_K);

  gettimeofday(&end_timeval, NULL);
  timeval_subtract(&elapsed_timeval, &end_timeval, &start_timeval);
  etime = 1000 * elapsed_timeval.tv_sec + elapsed_timeval.tv_usec/1000;
  printf("task %u: Algorithm S took %.2f s\n", tasknum, (double)etime/1000);
  printf("task %u: theta = ", tasknum);
  for (i = 0; i < n; i++) 
    printf("%g ", theta[i]);
  printf("\ntask %u: Dmean = ", tasknum);
  for (i = 0; i < n; i++) 
    printf("%g ", Dmean[i]);
  printf("\n");
  fflush(theta_outfile);

  /* set derivative estimate to 1e-10 constant for start of algorithm_EE 
     regardless of output of algorithm_S */
//XXX  for (i = 0; i < n; i++) Dmean[i]=1e-10; 
  printf("\ntask %u: initial value of D0 for algorithm_EE = ", tasknum);
  for (i = 0; i < n; i++) 
    printf("%g ", Dmean[i]);
  printf("\n");

  printf("task %u: running Algorithm EE...\n", tasknum);
  gettimeofday(&start_timeval, NULL);

  algorithm_EE(g, n, n_attr, n_dyadic, change_stats_funcs, 
               attr_change_stats_funcs, dyadic_change_stats_funcs, attr_indices,
               Mouter, M, sampler_m, ACA_EE, compC,
               Dmean, theta, theta_outfile, dzA_outfile, outputAllSteps,
               useIFDsampler, ifd_K);

  gettimeofday(&end_timeval, NULL);
  timeval_subtract(&elapsed_timeval, &end_timeval, &start_timeval);
  etime = 1000 * elapsed_timeval.tv_sec + elapsed_timeval.tv_usec/1000;
  printf("task %u: Algorithm EE took %.2f s\n", tasknum, (double)etime/1000);
  free(Dmean);
}

/*
 * Do estimation using the S and EE algorithms for digraph read from
 * Pajek format.
 *
 * Parameters:
 *   config - (in/out)configuration settings structure  - this is modified
 *            by calling build_attr_indices_from_names() etc.
 *   tasknum - task number (MPI rank)
 *
 * Return value:
 *    0 if OK else -ve value for error.
 */
int do_estimation(config_t * config, uint_t tasknum)
{
  struct timeval start_timeval, end_timeval, elapsed_timeval;
  int            etime;
  FILE          *arclist_file;
  digraph_t     *g;
  uint_t         i;
  FILE          *theta_outfile;
  FILE          *dzA_outfile;
  FILE          *sim_outfile;
  char           theta_outfilename[PATH_MAX+1];
  char           dzA_outfilename[PATH_MAX+1];
  char           sim_outfilename[PATH_MAX+1];
  char           suffix[16]; /* only has to be large enough for "_xx.txt" 
                                where xx is tasknum */
  uint_t         n_struct, n_attr, n_dyadic, num_param;
  double        *theta;
#define HEADER_MAX 65536
  char fileheader[HEADER_MAX];  
  
  if (!(arclist_file = fopen(config->arclist_filename, "r"))) {
    fprintf(stderr, "error opening file %s (%s)\n", 
            config->arclist_filename, strerror(errno));
    return -1;
  }
  gettimeofday(&start_timeval, NULL);
  printf("loading arc list from %s and building two-path matrices...",
         config->arclist_filename);
  g = load_digraph_from_arclist_file(arclist_file,
                                     config->binattr_filename,
                                     config->catattr_filename,
                                     config->contattr_filename);
  gettimeofday(&end_timeval, NULL);
  timeval_subtract(&elapsed_timeval, &end_timeval, &start_timeval);
  etime = 1000 * elapsed_timeval.tv_sec + elapsed_timeval.tv_usec/1000;
  printf("%.2f s\n", (double)etime/1000);
#ifdef DEBUG_DIGRAPH
  dump_digraph_arclist(g);
#endif /*DEBUG_DIGRAPH*/

  if (config->zone_filename) {
    if (add_snowball_zones_to_digraph(g, config->zone_filename)) {
      fprintf(stderr, "ERROR: reading snowball sampling zones from %s failed\n",
              config->zone_filename);
      return -1;
    }
#ifdef DEBUG_SNOWBALL
    dump_zone_info(g);
#endif /* DEBUG_SNOWBALL */
  }
  
  if (tasknum == 0) {
    print_data_summary(g);
    print_zone_summary(g);
  }

  /* now that we have attributes loaded in g, build the attr_indices
     array in the config struct */
  if (build_attr_indices_from_names(config, g) != 0)  {
    fprintf(stderr, "ERROR in attribute parameters\n");
    return -1;
  }
  /* and similary for dyadic covariates */
  if (build_dyadic_indices_from_names(config, g) != 0)  {
    fprintf(stderr, "ERROR in dyadic covariate parameters\n");
    return -1;
  }

  /* note num_param is computed here as build_dyadic_indices_from_names()
     can decrease config->num_dyadic_change_stats_funcs from its 
     initial value */
  n_struct = config->num_change_stats_funcs;
  n_attr = config->num_attr_change_stats_funcs;
  n_dyadic = config->num_dyadic_change_stats_funcs;
  num_param =  n_struct + n_attr + n_dyadic;
    
  theta = (double *)safe_malloc(num_param*sizeof(double));

  /* Open the output files (separate ones for each task), for writing */
  strncpy(theta_outfilename, config->theta_file_prefix,
          sizeof(theta_outfilename)-1);
  strncpy(dzA_outfilename, config->dzA_file_prefix,
          sizeof(dzA_outfilename)-1);
  sprintf(suffix, "_%d.txt", tasknum);
  strncat(theta_outfilename, suffix, sizeof(theta_outfilename) - 1 -
          strlen(suffix));
  strncat(dzA_outfilename, suffix, sizeof(dzA_outfilename) - 1 -
          strlen(suffix));
  if (!(theta_outfile = fopen(theta_outfilename, "w"))) {
    fprintf(stderr, "ERROR: task %d could not open file %s for writing "
            "(%s)\n", tasknum, theta_outfilename, strerror(errno));
    return -1;
  }
   
   /* FIXME Kafile should not be file global, should be one per task,
    * and filename should be configuration variable not hardcoded here */
   Kafile=NULL;
   //uncomment next line to debug
   //Kafile = fopen("variance_by_mean.txt", "w"); 

   /* Ensure that for the IFD sampler there is no Arc parameter included 
      as the IFD sampler computes this itself from the auxiliary parameter */
   if (config->useIFDsampler) {
     for (i = 0; i < config->num_change_stats_funcs; i++) {
       if (strcasecmp(config->param_names[i], ARC_PARAM_STR) == 0) {
         fprintf(stderr, 
                 "ERROR: cannot include Arc parameter when using IFD sampler.\n"
                 "Either unset useIFDsampler or remove Arc from %s.\n",
                 STRUCT_PARAMS_STR);
         return -1;
       }
     }
   }

  if (!(dzA_outfile = fopen(dzA_outfilename, "w"))) {
    fprintf(stderr, "ERROR: task %d could not open file %s for writing "
            "(%s)\n", tasknum, dzA_outfilename, strerror(errno));
    return -1;
  }

  /* write headers for output files */
  sprintf(fileheader, "t");
  if (config->useIFDsampler) /* IFD sampler always computes an Arc parameter */
    snprintf(fileheader+strlen(fileheader), HEADER_MAX," %s", ARC_PARAM_STR);
  for (i = 0; i < config->num_change_stats_funcs; i++) 
    snprintf(fileheader+strlen(fileheader), HEADER_MAX," %s", config->param_names[i]);
  
  for (i = 0; i < config->num_attr_change_stats_funcs; i++) 
    snprintf(fileheader+strlen(fileheader), HEADER_MAX, " %s_%s", config->attr_param_names[i],
       config->attr_names[i]);
  
   for (i = 0; i < config->num_dyadic_change_stats_funcs; i++)
     snprintf(fileheader+strlen(fileheader), HEADER_MAX, " %s", config->dyadic_param_names[i]);
  
  fprintf(theta_outfile,  "%s AcceptanceRate\n", fileheader);
  fprintf(dzA_outfile, "%s\n", fileheader);
  if(Kafile) fprintf(Kafile, "%s\n",fileheader); /* FIXME should be task local */
  
  ee_estimate(g, num_param, n_attr, n_dyadic, config->change_stats_funcs,
              config->attr_change_stats_funcs,
              config->dyadic_change_stats_funcs, config->attr_indices,
              config->samplerSteps, config->Ssteps,
              config->EEsteps, config->EEinnerSteps,
              config->ACA_S, config->ACA_EE, config->compC,
              theta, tasknum, theta_outfile, dzA_outfile,
              config->outputAllSteps,
              config->useIFDsampler, config->ifd_K);

  fclose(theta_outfile);
  fclose(dzA_outfile);
  if (config->outputSimulatedNetwork) {
    strncpy(sim_outfilename, config->sim_net_file_prefix,
            sizeof(sim_outfilename)-1);
    sprintf(suffix, "_%d.net", tasknum);
    strncat(sim_outfilename, suffix, sizeof(sim_outfilename) - 1 -
            strlen(suffix));
    sim_outfile = fopen(sim_outfilename, "w");
    write_digraph_arclist_to_file(sim_outfile, g);
    fclose(sim_outfile);
  }
  if(Kafile)fclose(Kafile);/*FIXME should be task local */
  free_digraph(g);
  free(theta);
  return 0;
}
