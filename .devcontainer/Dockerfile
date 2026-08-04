FROM mcr.microsoft.com/devcontainers/base:ubuntu-24.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       build-essential \
       ca-certificates \
       clang \
       clang-format \
       cmake \
       curl \
       git-lfs \
       golang-go \
       jq \
       ninja-build \
       nodejs \
       npm \
       python3 \
       python3-pip \
       shellcheck \
       unzip \
       zip \
    && npm install --global corepack \
    && corepack enable \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

ENV GO111MODULE=on
