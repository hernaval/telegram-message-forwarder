import asyncio
from telegram_forwarder import TelegramForwarder

async def main():
    forwarder = TelegramForwarder()
    await forwarder.run()

if __name__ == "__main__":
    asyncio.run(main())