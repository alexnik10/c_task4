#!/bin/bash

run_test() {
    local num_clients=$1
    local delay=$2

    rm delay_sum.log

    ./server &> /dev/null &
    SERVER_PID=$!

    client_pids=()

    for i in $(seq 1 $num_clients); do
        ./client $delay < numbers.txt &> /dev/null &
        client_pids+=($!)
    done

    for pid in "${client_pids[@]}"; do
        wait $pid
    done

    kill -SIGINT $SERVER_PID

    max_sum_delay=$(sort -nr delay_sum.log | head -n 1)
    first_send_time=$(grep "Client received time in ms" server.log | head -n 1 | grep -oE '[0-9]+')
    last_send_time=$(grep "Client received time in ms" server.log | tail -n 1 | grep -oE '[0-9]+')
    send_time=$((last_send_time - first_send_time))
    effective_speed=$((send_time - max_sum_delay))
    echo "Эффективная скорость при задержке $delay мс и $num_clients клиентах: $effective_speed мс"
}

echo -e "Задание 3. Тестирование эффективной скорости.\nТест может занять 6-7 минут."
run_test 100 0
run_test 100 200
run_test 100 400
run_test 100 600
run_test 100 800
run_test 100 1000

run_test 50 0
run_test 50 200
run_test 50 400
run_test 50 600
run_test 50 800
run_test 50 1000

run_test 1 0
run_test 1 200
run_test 1 400
run_test 1 600
run_test 1 800
run_test 1 1000
