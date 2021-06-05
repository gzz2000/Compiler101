#!/bin/bash

# @brief Batch test suite.

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
    
    # dump eeyore for easy debugging
    mon "debug compiler RE" ./build/zcc -S -e $c -o output.eeyore
    
    mon "compiler RE" ./build/zcc -S -t $c -o output.tigger
    input="${c%.sy}.in"
    if [ -f $input ]; then
        echo $input
    else
        input=/dev/null
    fi
    ./minivm/build/minivm -t output.tigger < $input > output.out
    ret=$?
    #sed -i -e '$ s/\n*$/\n/g' output.out
    #sed -i -e '$ s/^\n\n$/\n/g' output.out
    echo >> output.out
    echo $ret >> output.out
    mon "program WA" diff -B --ignore-all-space output.out "${c%.sy}.out"
done
