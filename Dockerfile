# ===========================
# Stage 1: Build
# ===========================
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git wget curl pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy source code
COPY . .

# Configure & build
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build --target ome -j$(nproc)

# ===========================
# Stage 2: Runtime
# ===========================
FROM ubuntu:22.04

# Install only runtime libraries
RUN apt-get update && apt-get install -y --no-install-recommends \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy built engine only
COPY --from=builder /app/build/ome /app/ome

# Define persistent storage locations
VOLUME ["/data/wal", "/data/rocksdb"]

# Set environment variables (optional defaults)
ENV OME_WAL_PATH=/data/wal \
    OME_ROCKSDB_PATH=/data/rocksdb

# Run engine
CMD ["./ome"]
