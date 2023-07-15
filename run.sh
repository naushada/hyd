#!/usr/bin/sh

if [ $# -eq 0 ]
    then
        echo 'Please provide the container ID'
        return
fi

docker run -p 192.168.1.104:58080:58080 -p 58989:58989 -p 65344:65344 -v /home/mnahmed/sw/poc/hyd/certs:/opt/xAPP/cert naushada/unimanage:latest \
                /opt/xAPP/uniimage/uniimage --role server --web-port 58080 --server-port 58989 --protocol tls --timeout 10000
