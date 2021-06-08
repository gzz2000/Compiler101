#!/bin/bash

# Before running this, please setup the docker environment as follows:
# risc-v % docker build --platform linux/amd64 -t riscv-dev-env-x86 .
# project % docker run -dt --name riscv -v `pwd`/local/:/local riscv-dev-env-x86

# @brief Batch test suite.

cases=`ls ./open-test-cases/sysy/section1/functional_test/*.sy`
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
    mon "debug compiler RE" ./build/zcc -S -e $c -o local/output.eeyore
    mon "debug compiler RE" ./build/zcc -S -t $c -o local/output.tigger
    mon "compiler RE" ./build/zcc -S $c -o local/output.S
    
    input="${c%.sy}.in"
    if [ -f $input ]; then
        echo $input
    else
        input=/dev/null
    fi

    mon "assembler RE" docker exec -it riscv riscv32-unknown-linux-gnu-gcc /local/output.S -o /local/output -L/root -lsysy -static
    
    docker exec -i riscv qemu-riscv32-static /local/output < $input > local/output.out
    
    ret=$?
    #sed -i -e '$ s/\n*$/\n/g' output.out
    #sed -i -e '$ s/^\n\n$/\n/g' output.out
    echo >> local/output.out
    echo $ret >> local/output.out
    mon "program WA" diff -B --ignore-all-space local/output.out "${c%.sy}.out"
done
