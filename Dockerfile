FROM alpine:latest
RUN apk add git g++ make cmake unzip libtool curl-dev automake
#RUN apk update && apk add --update alpine-sdk && apk add --no-cache make build-base && apk add cmake && apk add git && apk add g++
WORKDIR /root

# RUN git clone -b feature/x86 https://github.com/naushada/uniimage.git
RUN git clone https://github.com/naushada/uniimage.git
RUN cd uniimage
RUN mkdir build
WORKDIR /root/uniimage/build
RUN cmake .. && make


WORKDIR /opt/xAPP
RUN mkdir uniimage
RUN cd uniimage
WORKDIR /opt/xAPP/uniimage

# copy from previoud build stage
RUN cp /root/uniimage/build/uniimage .

# CMD_ARGS --role server --server-ip <ip-address> --server-port <server-port> --web-port <web-port> --protocol tcp
ENV ARGS="--role server --server-port 58989  --protocol tcp"
ENV PORT=58080
CMD "/opt/xAPP/uniimage/uniimage" --web-port ${PORT} ${ARGS}
