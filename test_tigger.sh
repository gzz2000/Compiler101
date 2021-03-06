#!/bin/bash

# @brief Batch test suite.

cases=`ls ./local/functional/*.sy`
badcases="92_matrix_add 93_matrix_sub 94_matrix_mul 95_matrix_tran 96_many_param_call 97_many_global_var"

function isbadcase {
    # usage: isbadcase {filename}
    for bad in $badcases; do
        if [[ $1 == *$bad* ]]; then
            echo $1 is a bad case
            return 0;
        fi
    done
    return 1
}

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
    if isbadcase $c; then
        continue
    fi
    echo $c
    
    # dump eeyore for easy debugging
    mon "debug compiler RE" ./build/zcc -S -e $c -o local/minivm/output.eeyore
    
    mon "compiler RE" ./build/zcc -S -t $c -o local/minivm/output.tigger
    input="${c%.sy}.in"
    if [ -f $input ]; then
        echo $input
    else
        input=/dev/null
    fi
    ./minivm/build/minivm -t local/minivm/output.tigger < $input > local/minivm/output.out
    ret=$?
    #sed -i -e '$ s/\n*$/\n/g' output.out
    #sed -i -e '$ s/^\n\n$/\n/g' output.out
    echo >> local/minivm/output.out
    echo $ret >> local/minivm/output.out
    mon "program WA" diff -B --ignore-all-space local/minivm/output.out "${c%.sy}.out"
done
