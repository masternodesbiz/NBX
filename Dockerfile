FROM ubuntu:18.04 as builder

RUN apt-get update \
    && apt-get install --no-install-recommends --yes \
        software-properties-common automake autoconf pkg-config libtool build-essential \
        libboost-all-dev bsdmainutils libssl1.0-dev libevent-dev libgmp-dev libminiupnpc-dev

RUN add-apt-repository ppa:bitcoin/bitcoin
RUN apt-get update \
    && apt-get install --no-install-recommends --yes \
        libdb4.8-dev libdb4.8++-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /netboxwallet

COPY / /netboxwallet

RUN ./autogen.sh
RUN ./configure
RUN make -j$(nproc)
#RUN make install

RUN strip $PWD/src/nbxd $PWD/src/nbx-cli $PWD/src/nbx-tx

FROM ubuntu:18.04

RUN apt-get update \
    && apt-get install --no-install-recommends --yes \
        software-properties-common \
    && add-apt-repository ppa:bitcoin/bitcoin \
    && apt-get update \
    && apt-get install --no-install-recommends --yes \
        libboost-system1.65.1 libboost-filesystem1.65.1 libboost-program-options1.65.1 libboost-thread1.65.1 \
        libboost-chrono1.65.1 libdb4.8++ libevent-2.1 libevent-pthreads-2.1 libminiupnpc10 libssl1.0.0 curl \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /netboxwallet/src/nbxd /netboxwallet/src/nbx-cli /netboxwallet/src/nbx-tx /usr/local/bin/

ENTRYPOINT ["nbxd"]
