#!/usr/bin/env python3
"""
Portfolio Manager Test Client
Demonstrates interaction with the enhanced portfolio manager service
"""

import asyncio
import time
import uuid
from nats.aio.client import Client as NATS
import messages_pb2 as pb

class PortfolioClient:
    def __init__(self, nats_url="nats://localhost:4222"):
        self.nats_url = nats_url
        self.nc = None
        self.client_uid = f"python-client-{uuid.uuid4().hex[:8]}"
        
    async def connect(self):
        """Connect to NATS server"""
        self.nc = NATS()
        await self.nc.connect(self.nats_url)
        print(f"✅ Connected to NATS at {self.nats_url}")
        
    async def disconnect(self):
        """Disconnect from NATS server"""
        if self.nc:
            await self.nc.close()
            print("✅ Disconnected from NATS")
    
    async def health_check(self, target_service="svc-portfolio-001"):
        """Send health check request to portfolio manager"""
        print(f"🏥 Sending health check to {target_service}...")
        
        # Create health check request
        request = pb.HealthCheckRequest()
        request.service_name = "PythonTestClient"
        request.uid = self.client_uid
        
        # Serialize and send
        await self.nc.publish(
            f"service.{target_service}.Trevor.HealthCheckRequest",
            request.SerializeToString()
        )
        print("📤 Health check request sent")
    
    async def request_portfolio(self, account_id="TEST_ACCOUNT_001"):
        """Request portfolio information"""
        print(f"📊 Requesting portfolio for account: {account_id}")
        
        # Create portfolio request
        request = pb.PortfolioRequest()
        request.account_id = account_id
        request.requester_uid = self.client_uid
        request.symbols.extend(["AAPL", "GOOGL", "MSFT"])  # Request specific symbols
        
        # Serialize and send
        await self.nc.publish(
            f"service.svc-portfolio-001.Trevor.PortfolioRequest",
            request.SerializeToString()
        )
        print("📤 Portfolio request sent")
    
    async def send_market_data(self, symbol="AAPL", price=150.00, volume=1000000):
        """Send market data update (broadcast)"""
        print(f"📈 Broadcasting market data: {symbol} @ ${price}")
        
        # Create market data update
        update = pb.MarketDataUpdate()
        update.symbol = symbol
        update.price = price
        update.volume = volume
        update.timestamp = int(time.time() * 1000)  # milliseconds
        update.exchange = "NYSE"
        
        # Serialize and broadcast
        await self.nc.publish(
            "broadcast.Trevor.MarketDataUpdate",
            update.SerializeToString()
        )
        print("📤 Market data update broadcasted")
    
    async def listen_for_responses(self):
        """Listen for responses from the portfolio manager"""
        print("👂 Listening for responses...")
        
        async def health_response_handler(msg):
            response = pb.HealthCheckResponse()
            response.ParseFromString(msg.data)
            print(f"💚 Health Check Response: {response.service_name} - Status: {response.status}")
        
        async def portfolio_response_handler(msg):
            response = pb.PortfolioResponse()
            response.ParseFromString(msg.data)
            print(f"📊 Portfolio Response for {response.account_id}:")
            print(f"   • Total Value: ${response.total_value:,.2f}")
            print(f"   • Cash Balance: ${response.cash_balance:,.2f}")
            print(f"   • Status: {response.status}")
            print(f"   • Positions: {len(response.positions)} items")
        
        # Subscribe to responses
        await self.nc.subscribe(f"service.{self.client_uid}.Trevor.HealthCheckResponse", cb=health_response_handler)
        await self.nc.subscribe(f"service.{self.client_uid}.Trevor.PortfolioResponse", cb=portfolio_response_handler)
        
        print("✅ Subscribed to response channels")

async def main():
    """Main test function"""
    client = PortfolioClient()
    
    try:
        # Connect to NATS
        await client.connect()
        
        # Set up response listeners
        await client.listen_for_responses()
        
        # Wait a moment for subscriptions to be ready
        await asyncio.sleep(1)
        
        print("\n🚀 Starting Portfolio Manager Tests...")
        print("=" * 50)
        
        # Test 1: Health Check
        await client.health_check()
        await asyncio.sleep(2)
        
        # Test 2: Portfolio Request
        await client.request_portfolio()
        await asyncio.sleep(2)
        
        # Test 3: Market Data Updates (multiple)
        market_data = [
            ("AAPL", 155.50, 2500000),
            ("GOOGL", 2750.25, 1200000),
            ("MSFT", 305.75, 1800000),
            ("TSLA", 850.00, 3200000)
        ]
        
        for symbol, price, volume in market_data:
            await client.send_market_data(symbol, price, volume)
            await asyncio.sleep(0.5)  # Small delay between updates
        
        # Wait for all responses
        print("\n⏳ Waiting for responses...")
        await asyncio.sleep(5)
        
        print("\n✅ Tests completed!")
        
    except Exception as e:
        print(f"❌ Error: {e}")
    finally:
        await client.disconnect()

if __name__ == "__main__":
    print("🐍 Portfolio Manager Python Test Client")
    print("🔧 Make sure the Portfolio Manager service is running")
    print()
    
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n🛑 Test interrupted by user")
    except Exception as e:
        print(f"❌ Unexpected error: {e}")
