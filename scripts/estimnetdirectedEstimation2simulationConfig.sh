#!/bin/sh
#
# File:    estimnetdirectedEstimation2simulationConfig.sh
# Author:  Alex Stivala
# Created: November 2019
#
#
# Read output of computeEstimNetDirectedCovariance.R with the estimate
# computed from EstimNetDirected results
# and build config script for SimulateERGM to simulate networks from
# the estimated parmeter values. Also needs to parse some filenames
# from the configuration file used for the estimation, to get arc list
# file (for number of nodes), output file names (obseved network statistics)
# etc.
# 
# Usage: estimnetdirectedEstimation2simulationConfig.sh estimation_config_file estimationoutputfile statsoutputfile
#
#  estmiation_config_file is the config file that generated the
#      estimationoutputfile
#  statsoutputfile is the file to write the simulated network stats to
#    (this is written in the output config file)
#
# E.g.:
#   estimnetdirectedEstimation2simulationConfig.sh config_example.txt estimation.out stats_estimation.out
#
# Output is to stdout
#
# Uses various GNU utils options on echo, etc.



if [ $# -ne 3 ]; then
    echo "usage: $0 estimation_config.txt estimation.out statsoutputfilename" >&2
    exit 1
fi

estimationconfig=$1
estimationresults=$2
statsFile=$3

estimnet_tmpfile=`mktemp`

echo "# Generated by: $0 $*"
echo "# At: " `date`
echo "# On: " `uname -a`

arclistFile=`grep -i arclistFile ${estimationconfig} | awk -F= '{print $2}'`
observedStatsFilePrefix=`grep -i observedStatsFilePrefix ${estimationconfig} | awk -F= '{print $2}'`

echo "# arclistFile = ${arclistFile}"
echo "# observedStatsFilePrefix = ${observedStatsFilePrefix}"

numNodes=`cat ${arclistFile} | grep -i '^*Vertices'| awk '{print $2}'`

echo "numNodes = ${numNodes}"

echo <<EOF
useTNTsampler = True # use the tie-no-tie sampler
sampleSize = 100 #number of network samples to take from simulation
interval = 100000 # interval (iterations) between samples
burnin = 100000000 # number of iterations to throw away before first sample
outputSimulatedNetworks = False
EOF

# we need the attribute files, directly from the esetimation config file
grep -i binattrFile ${estimationconfig}
grep -i catattrFile ${estimationconfig}
grep -i contattrFile ${estimationconfig}
grep -i setattrFile ${estimationconfig}


echo "# Filename of file to write statistics to"
echo "statsFile = ${statsFile}"


# new version has results starting at line following "Pooled" at start
# of line (pooling the individual run estimates values printed earlier) and
# 5 columns:
# Effect   estimate   sd(theta)   est.std.err  t.ratio
# (and maybe *) plus
# TotalRuns and ConvergedRuns e.g.:
#Diff_completion_percentage -0.002270358 0.005812427 0.01295886 0.021386
#TotalRuns 2
#ConvergedRuns 2
# (see computeEstimNetDirectedCovariance.R)
# https://unix.stackexchange.com/questions/78472/print-lines-between-start-and-end-using-sed
cat ${estimationresults} | sed -n -e '/^Pooled/,/^TotalRuns/{//!p}'  | tr -d '*' | fgrep -vw AcceptanceRate | fgrep -vw TotalRuns | fgrep -vw ConvergedRuns | awk '{print $1,$2,$4,$5}'  |  tr ' ' '\t' >> ${estimnet_tmpfile}

effectlist=`cat ${estimnet_tmpfile} |  awk '{print $1}' | sort | uniq`

for effect in ${effectlist}
do
  estimnet_point=`grep -w ${effect} ${estimnet_tmpfile} | awk '{print $2}'`
  echo $effect $estimnet_point #XXX
done


rm ${estimnet_tmpfile}
