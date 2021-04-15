#!/bin/bash

# @brief Batch test suite.
# currently, we only test if the program does not crash on the test inputs. (so it builds the AST with no big issue)

cases=`ls ./open-test-cases/sysy/section1/functional_test/*.sy`

function mon {
    # usage: mon <error info> <command..>
    "${@:2}"
    ret=$?
    if [ $ret -ne 0 ]; then
        echo "$1. returned value is $ret"
        exit $ret
    fi
}

for c in $cases; do
    echo $c
    mon "compiler RE" ./build/sysy_eeyore -S -e $c -o output.eeyore
    input="${c%.sy}.in"
    if [ -f $input ]; then
        echo $input
    else
        input=/dev/null
    fi
    mon "program RE" ./minivm/build/minivm output.eeyore < $input > output.out
    echo $ret >> output.out
    mon "program WA" diff output.out "${c%.sy}.out"
done
