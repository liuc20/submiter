#!/bin/bash
PROC_PATH=/proc/submiter/submiter_entry
FILE=data.txt
cat ${FILE} | while read line
do
	echo "${line}" > ${PROC_PATH}
done
