#!/bin/bash
ps x | while read pid x x x cmd
do
 if printf -- '%s' $cmd | egrep -q -- "DBserver"
 then
	echo "zabijam $pid"
	kill $pid
 fi
done
ps x | while read pid x x x cmd
do
 if printf -- '%s' $cmd | egrep -q -- "rso_election"
 then
	echo "zabijam $pid"
	kill $pid
 fi
done
