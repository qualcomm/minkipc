// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#!/bin/sh
run_and_check()
{
    ROUTE=$1
    VM=$2
    flags=''
    # echo "Route = $ROUTE; VM = $VM"
    if [ "$ROUTE" = "direct" ]; then
        flags="${flags} --d"
    fi
    if [ "$VM" = "oemvm" ]; then
        flags="${flags} --oem"
    fi
    log="log_${ROUTE}_${VM}_test.txt"
    cmd="./mink_test --gtest_shuffle $flags > $log 2>&1"
    echo "Executing: $cmd"
    echo "$cmd" | bash
    echo "Find mink test result in $log"
    grep -nrE "PASSED|FAILED|Failure" "$log"
}

rm -rf log_*_test.txt
pathway='hub direct'
for route in $pathway
do
    destination='qtvm oemvm'
    for vm in $destination
    do
        run_and_check $route $vm
    done
done
