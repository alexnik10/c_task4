#!/bin/bash

rm delay_sum.log

./server &> /dev/null &

SERVER_PID=$!


echo "Задание 2. Проверим, что сервер корректно работает при повторных выполнениях тестового скрипта."
chmod +x ./run_one_hundred_clients.sh
./run_one_hundred_clients.sh
echo -e "Первый запуск. Состояние сервера:"
echo "0" | ./client 1

./run_one_hundred_clients.sh
echo -e "Второй запуск. Состояние сервера:"
echo "0" | ./client 1

./run_one_hundred_clients.sh
echo -e "Третий запуск. Состояние сервера:"
echo "0" | ./client 1

./run_one_hundred_clients.sh
echo -e "Четвёртый запуск. Состояние сервера: "
echo "0" | ./client 1

kill -SIGINT $SERVER_PID

echo "Номер дескриптора  и граница кучи при подключении первого клиента:"
grep 'New client connected' < server.log | head -n 1
echo "Номер дескриптора  и граница кучи при подключении последнего клиента:"
grep 'New client connected' < server.log | tail -n 1