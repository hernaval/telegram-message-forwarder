from telethon import TelegramClient
import os
import asyncio

from dotenv import load_dotenv

load_dotenv()


async def create_initial_session():
    api_id = os.getenv('TELEGRAM_API_ID')
    api_hash = os.getenv('TELEGRAM_API_HASH')
    phone_number = os.getenv('TELEGRAM_PHONE_NUMBER')

    if not all([api_id, api_hash, phone_number]):
     raise ValueError("Missing credentials for initial session creation")

    from telethon.sessions import StringSession
    client = TelegramClient(StringSession(), api_id, api_hash)

    async with client:
     await client.start(phone=phone_number)

    # Generate session string for deployment
    session_string = client.session.save()

    print(f"Session created successfully!")
    print(f"Add this to your .env file for deployment:")
    print(f"TELEGRAM_SESSION_STRING={session_string}")

    # Test the session
    me = await client.get_me()
    print(f"Logged in as: {me.first_name} {me.last_name or ''} (@{me.username or 'no username'})")


async def main():
   await create_initial_session()

if __name__ == "__main__":
    asyncio.run(main())

