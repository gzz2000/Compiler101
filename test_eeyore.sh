#!/bin/bash

# @brief Batch test suite.

cases=`ls ./local/performance/*.sy`

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
    mon "compiler RE" ./build/zcc -S -e $c -o local/minivm/output.eeyore
    input="${c%.sy}.in"
    if [ -f $input ]; then
        echo $input
    else
        input=/dev/null
    fi
    ./minivm/build/minivm local/minivm/output.eeyore < $input > local/minivm/output.out
    ret=$?
    #sed -i -e '$ s/\n*$/\n/g' output.out
    #sed -i -e '$ s/^\n\n$/\n/g' output.out
    echo >> local/minivm/output.out
    echo $ret >> local/minivm/output.out
    mon "program WA" diff -B --ignore-all-space local/minivm/output.out "${c%.sy}.out"
done
