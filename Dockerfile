FROM alpine:latest
RUN apk add git g++ make cmake unzip libtool curl-dev automake
#RUN apk update && apk add --update alpine-sdk && apk add --no-cache make build-base && apk add cmake && apk add git && apk add g++

WORKDIR /root

RUN git clone -b uniimage https://github.com/naushada/hyd.git
RUN cd hyd/uniimage
RUN mkdir build

WORKDIR /root/hyd/uniimage/build
RUN cmake .. && make

WORKDIR /opt/xAPP
RUN mkdir uniimage
RUN cd uniimage
WORKDIR /opt/xAPP/uniimage

# copy from previoud build stage
RUN cp /root/hyd/uniimage/build/uniimage .
RUN rm -fr /root/hyd/uniimage/

# CMD_ARGS --role server --server-ip <ip-address> --server-port <server-port> --web-port <web-port> --protocol tcp
ENV ARGS="--role client --server-port 58989  --protocol tcp"
ENV IP="127.0.0.1"
CMD "/opt/xAPP/uniimage/uniimage" --server-ip ${IP} ${ARGS}
