sf570#!/bin/bash

touch /tmp/sf570/mountdir9/test.txt

for((input1=0; input1 < 200000; input1++))
  do
    echo "$input1"
    echo "write$input1" >> "/tmp/sf570/mountdir9/file.txt"
  done

#rm /tmp/sf570/mountdir9/test.txt
