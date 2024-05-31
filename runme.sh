#!/bin/bash

make

./server &> /dev/null &

SERVER_PID=$!

if ps -p $SERVER_PID > /dev/null; then
    echo "Сервер успешно запущен с PID $SERVER_PID"
else
    echo "Ошибка запуска сервера"
    exit 1
fi

echo "/tmp/brownian_bot_socket" > config

seq -500 500 | grep -v '^0$' | shuf > numbers.txt

chmod +x ./run_one_hundred_clients.sh
./run_one_hundred_clients.sh

echo "Задание 1. Итоговое состояние сервера:"
echo "0" | ./client 1

kill -SIGINT $SERVER_PID

chmod +x ./many_client_launches.sh
./many_client_launches.sh

chmod +x ./benchmarks.sh
./benchmarks.sh
echo -e "Конец тестирования."