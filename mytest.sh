#!/bin/bash
echo "This is a test script"
echo $PATH
#export PATH=$PATH:directory
#echo What is your name?
#read MY_NAME
#echo "Hello $MY_NAME - hope you're well."
#echo "MYVAR is: $MYVAR"
#MYVAR="hi there"
#echo "MYVAR is: $MYVAR"
for((number=0; number < 12; number++))
do
	touch "/tmp/nt284/mountdir/file$number"
	[ -f "/tmp/nt284/mountdir/file$number" ] && \
      	echo "/tmp/nt284/mountdir/file$number is the path of a real file" || \
      	echo "No such file: /tmp/nt284/mountdir/file$number"
	

done
echo "files created"
read MYVAR
for((input=0; input < 13000; input++))
        do
		for((input1=0; input1 < 12; input1++))
        	do
                	echo "/tmp/nt284/mountdir/file$input1 /tmp/nt284/mountdir/file$input1 /tmp/nt284/mountdir/file$input1 /tmp/nt284/mountdir/file$input1 /tmp/nt284/mountdir/file$input1 /tmp/nt284/mountdir/file$input1 /tmp/nt284/mountdir/file$input1 /tmp/nt284/mountdir/file$input1 /tmp/nt284/mountdir/file$input1 /tmp/nt284/mountdir/file$input1 /tmp/nt284/mountdir/file$input1 /tmp/nt284/mountdir/file$input1 /tmp/nt284/mountdir/file$input1 /tmp/nt284/mountdir/file$input1 /tmp/nt284/mountdir/file$input1 /tmp/nt284/mountdir/file$input1 /tmp/nt284/mountdir/file$input1 /tmp/nt284/mountdir/file$input1 /tmp/nt284/mountdir/file$input1 /tmp/nt284/mountdir/file$input1 /tmp/nt284/mountdir/file$input1 /tmp/nt284/mountdir/file$input1 /tmp/nt284/mountdir/file$input1 /tmp/nt284/mountdir/file$input1 /tmp/nt284/mountdir/file$input1" >> "/tmp/nt284/mountdir/file$input1"
                	#wc "/tmp/nt284/mountdir/file$number"
        	done
        done
echo "Files created: type anything to move ahead"
read MYVAR

for((input2=0; input2 < 12; input2++))
        do
                #echo "/tmp/nt284/mountdir/file$number" >> "/tmp/nt284/mountdir/file$number"
                wc "/tmp/nt284/mountdir/file$input2"
        done
echo "Wrote to file: type anything to move ahead"
read MYVAR

for((input3=0; input3 < 12; input3++))
        do
                #echo "/tmp/nt284/mountdir/file$number" >> "/tmp/nt284/mountdir/file$number"
                #wc "/tmp/nt284/mountdir/file$number"
		rm "/tmp/nt284/mountdir/file$input3"
        done
