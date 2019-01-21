#!/bin/sh
#
# File:    build_estimnetdirected_estimation_results_tab.sh
# Author:  Alex Stivala
# Created: November 2016
#
#
# Build table for reading into R of results of EstimNetDirected estimation 
# 
# Usage: build_estimnetdirected_estimation_results_tab.sh joboutputroot
#
# E.g.:
#   build_estimnetdirected_estimation_results_tab.sh  ~/estimnetdirected_estimations_n1000_binattr_50_50_sim2
#
# Output is to stdout
#
# Uses various GNU utils options on echo, etc.

if [ $# -ne 1 ]; then
    echo "usage: $0 joboutputrootdir" >&2
    exit 1
fi

joboutputroot=$1

echo "# Generated by: $0 $*"
echo "# At: " `date`
echo "# On: " `uname -a`
echo -e "Effect\tEstimate\tsdEstimate\tStdErr\tt-ratio\tsampleId\tnodeCount\tconvergedRuns\ttotalRuns"
for sampledir in ${joboutputroot}/sample*
do
    estimationresults=${sampledir}/estimation.out
    sampleid=`basename "${sampledir}" | sed 's/sample//g'`
    nodecount=`cat ${sampledir}/arclist.txt | tr -d '\015' | grep -i '^*Vertices'| awk '{print $2}'`
    totalruns=`fgrep -w TotalRuns ${estimationresults} | awk '{print $2}'`
    convergedruns=`fgrep -w ConvergedRuns ${estimationresults} | awk '{print $2}'`
    # also convert effect names into same as used in Snowball PNet 
    # https://unix.stackexchange.com/questions/78472/print-lines-between-start-and-end-using-sed
    cat ${estimationresults} | sed -n -e '/^Pooled/,/^TotalRuns/{//!p}' | tr -d '*' | fgrep -vw AcceptanceRate | fgrep -vw TotalRuns | fgrep -vw ConvergedRuns  |  sed 's/AltInStars/AinS/;s/AltOutStars/AoutS/;s/AltKTrianglesT/AKT-T/;s/AltTwoPathsTD/A2P-TD/;s/Sender_binaryAttribute/Sender/;s/Receiver_binaryAttribute/Receiver/;s/Interaction_binaryAttribute/Interaction/;s/Matching_categoricalAttribute/Matching/;s/MatchingReciprocity_categoricalAttribute/MatchingReciprocity/' | tr ' ' '\t' | sed "s/\$/\t${sampleid}\t${nodecount}\t${convergedruns}\t${totalruns}/"
done
