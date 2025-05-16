FROM ubuntu:24.10

WORKDIR /app

RUN apt-get update && apt-get install -y   libssl-dev g++ make libgtest-dev

RUN mkdir inc lib obj bin
COPY . .

RUN make clean all

ENTRYPOINT ["./run.sh", "$TARGET"]
