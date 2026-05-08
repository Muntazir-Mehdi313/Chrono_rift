FROM ubuntu:22.04

# Avoid prompts during installation
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    g++ \
    gdb \
    make \
    libsfml-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app