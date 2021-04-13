#!/bin/bash

# @brief Batch test suite.
# currently, we only test if the program does not crash on the test inputs. (so it builds the AST with no big issue)

cases=`ls ./open-test-cases/sysy/section1/functional_test/*.sy`

for c in $cases; do
    echo $c
    ./build/sysy_test $c
    ret=$?
    if [ $ret -ne 0 ]; then
        echo "returned value is $ret"
        break
    fi
done
