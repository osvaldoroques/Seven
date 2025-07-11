import asyncio
import os
from nats.aio.client import Client as NATS
import messages_pb2

async def main():
    nc = NATS()
    
    # Get NATS URL from environment or use default
    nats_url = os.getenv("NATS_URL", "nats://localhost:4222")
    await nc.connect(nats_url)

    uid = "py-client-001"

    # Function to handle the HealthCheckResponse
    async def message_handler(msg):
        res = messages_pb2.HealthCheckResponse()
        res.ParseFromString(msg.data)
        print(f"✅ Received HealthCheckResponse: service_name={res.service_name}, uid={res.uid}, status={res.status}")

    # Subscribe to point-to-point response
    subject = f"system.direct.{uid}.Trevor.HealthCheckResponse"
    await nc.subscribe(subject, cb=message_handler)

    # Send HealthCheckRequest
    req = messages_pb2.HealthCheckRequest()
    req.service_name = "PythonClient"
    req.uid = uid
    data = req.SerializeToString()

    target_uid = "svc-portfolio-001"
    send_subject = f"system.direct.{target_uid}.Trevor.HealthCheckRequest"
    await nc.publish(send_subject, data)

    print(f"✅ Sent HealthCheckRequest to {send_subject} with UID={uid}")

    # Wait to receive the response
    await asyncio.sleep(3)

    await nc.close()

if __name__ == "__main__":
    asyncio.run(main())
