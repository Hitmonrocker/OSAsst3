sf570#!/bin/bash

for((number=0; number < 200; number++))
  do
	   touch "/tmp/sf570/mountdir9/file$number"
  done

for((number=0; number < 200; number++))
  do
    for((input1=0; input1 < 200000; input1++))
      do
        echo "$input1"
        echo "write$input1" >> "/tmp/sf570/mountdir9/file$number.txt"
      done
  done
