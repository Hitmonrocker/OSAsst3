#!/bin/bash

for((number=0; number < 1; number++))
  do
	   touch "/tmp/nt284/mountdir/file$number"
  done

for((number=0; number <1 ; number++))
  do

    for((input1=0; input1 < 2000000; input1++))
      do
        #echo "$input1"
        echo "write$input1\n" >> "/tmp/nt284/mountdir/file$number"
      done
  done
