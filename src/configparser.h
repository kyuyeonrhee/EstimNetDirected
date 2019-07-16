#ifndef CONFIGPARSER_H
#define CONFIGPARSER_H
/*****************************************************************************
 * 
 * File:    configparser.h
 * Author:  Alex Stivala, Maksym Byshkin
 * Created: October 2017
 *
 * Parse the configuration file to get algorithm parameters, input filenames,
 * parameters to estimate, etc.
 *
 * The config file is a text file with comments marked by '#'
 * character, and "keyword = value" pairs.  See config.txt for example
 * config file.
 *
 *
 ****************************************************************************/

#include <stdio.h>
#include "utils.h"
#include "changeStatisticsDirected.h"

/* These must be macros not const to use in initializer  */
/* not case sensitive */
#define STRUCT_PARAMS_STR             "structParams"
#define ATTR_PARAMS_STR               "attrParams"
#define DYADIC_PARAMS_STR             "dyadicParams"
#define ARC_PARAM_STR                 "Arc"
#define ATTR_INTERACTION_PARAMS_STR   "attrInteractionParams"

#define DEFAULT_ACA_EE        1e-09   /* default value for ACA_EE */
#define DEFAULT_COMPC         1e-02   /* default value for compC */
#define DEFAULT_IFD_K         0.1     /* default value of ifd_K  */
#define DEFAULT_LEARNING_RATE 0.001   /* default value of learningRate */
#define DEFAULT_MIN_THETA     0.01    /* default value of minTheta */

/* config parameter types */
typedef enum param_type_e {
  PARAM_TYPE_INVALID,  /* invalid type, used as error return value */
  PARAM_TYPE_DOUBLE,   /* numeric (floating point) */
  PARAM_TYPE_UINT,     /* numeric (unsigned integer) */
  PARAM_TYPE_BOOL,     /* Boolean ("True" or "False" in config, bool in struct*/
  PARAM_TYPE_STRING,   /* string (may be quoted, not necessarily) */
  PARAM_TYPE_SET       /* comma delimited set of other params enclosed in {} */
} param_type_e;

/* ERGM attribute parameter type */
typedef enum attr_type_e {
  ATTR_TYPE_INVALID,        /* invalid type, used as error return value */
  ATTR_TYPE_BINARY,         /* binary attribute type (0/1)*/
  ATTR_TYPE_CATEGORICAL,    /* categorical attribute type (uint) */
  ATTR_TYPE_CONTINUOUS,     /* continuous attribute type (double) */
  ATTR_TYPE_SET             /* set attribute type (array of set_elem_e) */
} attr_type_e;

/* ERGM dyadic covariate parameter type */
typedef enum dyadic_type_e {
  DYADIC_TYPE_INVALID,       /* invalid type, used as error return value */
  DYADIC_TYPE_GEODISTANCE,   /* continuous geographic distance from lat/long */
  DYADIC_TYPE_EUCLIDEANDISTANCE /* continuous Euclidean distance from x/y/z */
} dyadic_type_e;



typedef struct config_s {
  /*
   * Parameters parsed directly from config file
   */
  double ACA_S;           /* multiplier for step size in Algorithm S */
  double ACA_EE;          /* multiplier for step size in Algorithm EE */
  double compC;           /* multiplier of sd/mean theta to limit variance */
  uint_t samplerSteps;    /* sampler iterations per algorithm step */
  uint_t Ssteps;          /* steps of Algorithm S */
  uint_t EEsteps;         /* steps of Algorithm EE */
  uint_t EEinnerSteps;    /* inner iterations of Algorithm EE */
  bool   outputAllSteps;   /* write theta and dzA every iteration not just outer*/
  bool   useIFDsampler;   /* Use IFD sampler instead of basic sampler */
  double ifd_K;           /* multiplier for aux parameter step size in IFD sampler */
  bool  outputSimulatedNetwork; /* output simulated network at end */
  char *arclist_filename; /* filename of Pajek file with digraph to estimate */
  char *binattr_filename; /* filename of binary attributes file or NULL */
  char *catattr_filename; /* filename of categorical attributes file or NULL */
  char *contattr_filename;/* filename of continuous attributes file or NULL */
  char *setattr_filename; /* filename of set attributes file or NULL */
  char *theta_file_prefix;/* theta output filename prefix */
  char *dzA_file_prefix;  /* dzA output filename prefix */
  char *sim_net_file_prefix; /* simulated network output filename prefix */
  char *zone_filename;    /* filename of snowball sampling zone file or NULL */
  bool  useConditionalEstimation; /*conditional estimation of snowball sample */ 
  bool  forbidReciprocity; /* do not allow reciprocated arcs in sampler */
  bool  useBorisenkoUpdate; /* use Borisenko et al. update algorithm */
  double learningRate;      /* learning rate (multiplier) in Borisenko update */
  double minTheta;          /* minimum abs theta value in Borisenko update */
  
  /*
   * values built by confiparser.c functions from parsed config settings
   */
  uint_t num_change_stats_funcs;           /* length of change_stats_funcs */
  change_stats_func_t **change_stats_funcs; /* structural parameter stats */
  const char          **param_names;        /* names corresponding to above */
  uint_t num_attr_change_stats_funcs;  /* length of attr_change_stats_funcs */
  attr_change_stats_func_t **attr_change_stats_funcs; /* attr param stats */
  char                     **attr_names; /* names of attributes for above */
  uint_t *attr_indices;   /* idx into digraph binattr/cattr/contattr for above */
  const char **attr_param_names; /* names corresponding to above two */
  uint_t num_dyadic_change_stats_funcs;  /* length of dyadic_change_stats_funcs */  
  dyadic_change_stats_func_t **dyadic_change_stats_funcs;/* dyadic change stats*/
  char                       **dyadic_names; /* names corresponding to above */
  uint_t *dyadic_indices;  /* idx into digraph binattr/cattr/contattr for above */
  dyadic_type_e *dyadic_types; /* dyadic paramter type corresponding to above */
  const char **dyadic_param_names; /* names corresponding to above two */
  uint_t num_attr_interaction_change_stats_funcs;  /* length of attr_interaction_change_stats_funcs */
  attr_interaction_change_stats_func_t **attr_interaction_change_stats_funcs; /* attr interaction param stats */
  string_pair_t *attr_interaction_pair_names; /* names of pairs of attributes for above */
  uint_pair_t  *attr_interaction_pair_indices;   /* pairs of indices into digraph binattr/cattr/contattr for above */
  const char **attr_interaction_param_names; /* names corresponding to above two */
} config_t;

void init_config_parser(void);
config_t *parse_config_file(const char *config_filename);
int build_attr_indices_from_names(config_t *config, const digraph_t *g);
int build_dyadic_indices_from_names(config_t *config, digraph_t *g);
int build_attr_interaction_pair_indices_from_names(config_t *config,
                                                   const digraph_t *g);
void free_config_struct(config_t *config);

void dump_config_names(void);
void dump_parameter_names(void);

#endif /* CONFIGPARSER_H */

