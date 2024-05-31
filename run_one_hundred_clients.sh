#!/bin/bash

run_client() {
    ./client 100 < numbers.txt &> /dev/null
}

for i in $(seq 1 100); do
    run_client &
done

wait
