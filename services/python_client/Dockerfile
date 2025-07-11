FROM python:3.12-slim

WORKDIR /app

# Install dependencies first (better for Docker layer caching)
RUN pip install --no-cache-dir nats-py protobuf grpcio-tools

# Copy the entire project (since we're using root context now)
COPY . .

# Generate Python protobuf files in the correct location
RUN python -m grpc_tools.protoc -Iproto --python_out=services/python_client proto/messages.proto

# Change to the python client directory
WORKDIR /app/services/python_client

CMD ["python", "healthcheck_client.py"]
