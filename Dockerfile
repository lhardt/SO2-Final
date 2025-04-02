FROM ubuntu:24.10

WORKDIR /app

RUN apt-get update && apt-get install -y   g++ make

RUN mkdir inc lib obj bin
COPY . .

RUN make clean all

ENTRYPOINT ["./bin/main"]