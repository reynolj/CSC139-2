#!/bin/bash
#Program 2 Script

wmic cpu get name, numberofcores

echo "Test #1"
MTFindProd 100000000 2 50000001
echo "Test #2"
MTFindProd 100000000 4 75000001
echo "Test #3"
MTFindProd 100000000 8 88000000
echo "Test #4"
MTFindProd 100000000 2 -1
echo "Test #5"
MTFindProd 100000000 4 -1
echo "Test #6"
MTFindProd 100000000 8 -1

