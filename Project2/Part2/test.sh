#!/bin/bash

#

sleep 1
echo "Configuring the pins and parameters"
echo -n "19" > /sys/class/HCSR/HCSR_0/trigger
echo -n "0" > /sys/class/HCSR/HCSR_0/echo
echo -n "3" > /sys/class/HCSR/HCSR_0/number_samples
echo -n "5000" > /sys/class/HCSR/HCSR_0/sampling_period
sleep 1

#Reading the Configured Values of HCSR0
echo ""
echo "Reading trigger pin value: " | cat - /sys/class/HCSR/HCSR_0/trigger
echo ""
echo "Reading Echo pin value : " | cat - /sys/class/HCSR/HCSR_0/echo
echo ""
echo "Reading m value : " | cat - /sys/class/HCSR/HCSR_0/number_samples
echo ""
echo "Reading delta value : " | cat - /sys/class/HCSR/HCSR_0/sampling_period
echo ""

sleep 1

#Enabling measurement 
echo -n "1" > /sys/class/HCSR/HCSR_0/enable
#Reading the enable value 
echo "Reading Enable value : " | cat - /sys/class/HCSR/HCSR_0/enable
sleep 4


sleep 1

#Disabling measurement
echo "Disabling sensor : " 
echo -n "0" > /sys/class/HCSR/HCSR_0/enable
sleep 6

# Reading recent distance
echo "Distance value : " | cat - /sys/class/HCSR/HCSR_0/distance
sleep 1



