#!/bin/bash
a="./rso-client list" 
printf $a >> res.csv

fline=$(head -n 1 res.csv)
IFS=',' read -r -a columns <<< "$fline"

sed 1d res.csv | while read line
do
	IFS=', ' read -r -a array <<< "$line"
	echo -e "\e[1;33m${array[0]} \
	========================\e[0m\n\
	${columns[1]}: \t ${array[1]}\n\
	${columns[2]}: \t ${array[2]}\n"
done
