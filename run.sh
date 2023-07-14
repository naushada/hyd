#!/usr/bin/sh

if [ $# -eq 0 ]
    then
        echo 'Please provide the container ID'
        return
fi

if [ -d "cert" ]; then 
    docker cp cert/. $1:/opt/xAPP/cert
else
    echo "cert directory isnotpresent"
    echo "execute command - openssl req -x509 -nodes -days 365 -newkey rsa:1024 -keyout pkey.pem -out cert.pem"
    return
fi
docker run -p 10.0.2.15:58080:58080 -p 58989:58989 -p 65344:65344 $1 \
                /opt/xAPP/uniimage/uniimage --role server --web-port 58080 --server-port 58989 --protocol tls --timeout 10000
