# syntax=docker/dockerfile:1.6
# A-3.3 — scratch gate bundle: launcher + musl gate + lib2adic_shim.so

FROM rust:1.85-bookworm AS build
WORKDIR /src
RUN apt-get update && apt-get install -y --no-install-recommends musl-tools gcc \
    && rustup target add x86_64-unknown-linux-musl \
    && rm -rf /var/lib/apt/lists/*
COPY . .
RUN cargo build --release -p atom_gate --target x86_64-unknown-linux-musl \
    && make -C src/shim \
    && make -C src/gate_launcher

FROM scratch
COPY --from=build /src/src/gate_launcher/gate_launcher /gate_launcher
COPY --from=build /src/target/x86_64-unknown-linux-musl/release/a_gate_v1_kv_microbench /a_gate_v1_kv_microbench
COPY --from=build /src/src/shim/lib2adic_shim.so /lib2adic_shim.so
ENTRYPOINT ["/gate_launcher"]
