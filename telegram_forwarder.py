import logging
from telethon import TelegramClient, events
from telethon.sessions import StringSession

from telethon.errors import FloodWaitError
import os
from dotenv import load_dotenv
import asyncio

load_dotenv()

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='[%(levelname)s] %(asctime)s - %(name)s - %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S',
    handlers=[
        logging.FileHandler('telegram_forwarder.log'),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger(__name__)

class TelegramForwarder:
    def __init__(self):
        self.api_id = os.getenv('TELEGRAM_API_ID')
        self.api_hash = os.getenv('TELEGRAM_API_HASH')
        session_string = os.getenv('TELEGRAM_SESSION_STRING')
        self.source_channel = os.getenv('SOURCE_CHANNEL')
        self.destination_channel = os.getenv('DESTINATION_CHANNEL')

        # Validate required env
        if not all([self.api_id, self.api_hash, self.source_channel, self.destination_channel, session_string]):
            raise ValueError("Missing required environment variables.")
        
        self.client = TelegramClient(StringSession(session_string), self.api_id, self.api_hash)
    
    async def get_channel_entity(self, channel_identifier):
        """Get channel entity from username or ID"""
        try:
            if channel_identifier.startswith('@'):
                entity = await self.client.get_entity(channel_identifier)
            elif channel_identifier.isdigit() or (channel_identifier.startswith('-') and channel_identifier[1:].isdigit()):
                entity = await self.client.get_entity(int(channel_identifier))
            else:
                entity = await self.client.get_entity(channel_identifier)
            return entity
        except Exception as e:
            logger.error(f"Failed to get entity for {channel_identifier}: {e}")
            raise

    async def setup_message_handler(self):
        """Setup message event handler for source channel"""
        try: 
            source_entity = await self.get_channel_entity(self.source_channel)
            logger.info(f"Listening for messages from: {source_entity.title if hasattr(source_entity, 'title') else source_entity.id}")

            # Register event handler for new message in source channel
            @self.client.on(events.NewMessage(chats=source_entity))
            async def message_handler(event):
                logger.info(f"New message received: {event.message.text[:50]}...")
                await self.forward_message(event)

        except Exception as e:
            logger.error(f"Failed to setup message handler: {e}")
            raise

    async def forward_message(self, event):
        """Forward message to destination channel"""
        try:
            destination_entity = await self.get_channel_entity(self.destination_channel)
            # Forward the message
            await self.client.send_message(
                entity=destination_entity,
                message=event.message,
            )
            logger.info(f"Message forwarded to {self.destination_channel}")
            
        except FloodWaitError as e:
            logger.warning(f"Rate limited. Waiting {e.seconds} seconds...")
            await asyncio.sleep(e.seconds)
            await self.forward_message(event)  # Retry after wait
            
        except Exception as e:
            logger.error(f"Failed to forward message: {e}")

    async def run(self):
        """Main method to run the message listener"""
        logger.info("Running Telegram forwarder with API_ID=%s", self.api_id)

        async with self.client:
             await self.setup_message_handler()
             await self.client.run_until_disconnected()
