


```
# Telegram Forwarder

A Python-based Telegram message forwarder that listens to messages from a source channel using the Telethon library.

## Table of Contents (Sommaire)

- [Description](#description)
- [Features](#features)
- [Prerequisites](#prerequisites)
- [Installation](#installation)
- [Configuration](#configuration)
- [Usage](#usage)
- [Project Structure](#project-structure)
- [Logging](#logging)
- [Error Handling](#error-handling)
- [Contributing](#contributing)
- [License](#license)

## Description

This project implements a Telegram message forwarder that monitors a specified source channel for new messages. Built with the Telethon library, it provides a foundation for creating automated message processing and forwarding systems.

The application uses session-based authentication and supports various channel identifier formats (username, ID, or channel name).

## Features

- **Real-time Message Monitoring**: Listens for new messages in specified Telegram channels
- **Flexible Channel Identification**: Supports usernames (@channel), numeric IDs, and channel names
- **Session-based Authentication**: Uses Telegram session strings for persistent authentication
- **Comprehensive Logging**: File and console logging with configurable levels
- **Environment-based Configuration**: Secure configuration through environment variables
- **Error Handling**: Robust error handling for network issues and API limitations

## Prerequisites

- Python 3.7 or higher
- Telegram API credentials (API ID and API Hash)
- A Telegram account
- Access to the source channel you want to monitor

## Installation

1. **Clone the repository**:
   ```bash
   $ git clone https://github.com/hernaval/telegram-message-forwarder.git
   ```


2. **Create a virtual environment** (recommended):
   ```bash
   python -m venv venv
   source venv/bin/activate  # On Windows: venv\Scripts\activate
   ```
3. **Install required dependencies**:
   ```bash
   $ pip install -r requirements.txt
   ```

## Configuration

1. **Get Telegram API credentials**:
   - Visit [my.telegram.org](https://my.telegram.org)
   - Log in with your phone number
   - Go to "API Development Tools"
   - Create a new application to get your `API_ID` and `API_HASH`

2. **Create a `.env` file** in the project root:
   ```env
   TELEGRAM_API_ID=your_api_id
   TELEGRAM_API_HASH=your_api_hash
   SOURCE_CHANNEL=@your_source_channel
   ```

3. **Generate a session string (One-time)**:
   ```bash
   $ python session.string_gen.py
   ```

3. **Add the generated session string as `TELEGRAM_SESSION_STRING` variable in `.env`** 
   

### Environment Variables

| Variable | Description | Example |
|----------|-------------|---------|
| `TELEGRAM_API_ID` | Your Telegram API ID | `12345678` |
| `TELEGRAM_API_HASH` | Your Telegram API Hash | `abcdef1234567890abcdef1234567890` |
| `TELEGRAM_SESSION_STRING` | Generated session string | `1BVtsOK4Bu...` |
| `SOURCE_CHANNEL` | Source channel to monitor | `@channelname` or `-1001234567890` |

## Usage

1. **Ensure your `.env` file is properly configured**

2. **Run the forwarder**:
   ```bash
   python main.py
   ```

3. **Monitor the logs**:
   - Console output for real-time monitoring
   - `telegram_forwarder.log` file for persistent logging

## Project Structure

```
telegram-forwarder/
├── telegram_forwarder.py    # Main application file
├── .env                     # Environment variables (create this)
├── .env.example            # Environment variables template
├── telegram_forwarder.log  # Log file (generated)
├── requirements.txt        # Python dependencies
└── README.md              # This file
```

## Logging

The application uses Python's built-in logging module with the following configuration:

- **Log Level**: INFO
- **Format**: `[LEVEL] TIMESTAMP - LOGGER_NAME - MESSAGE`
- **Handlers**: 
  - File handler (`telegram_forwarder.log`)
  - Console handler (stdout)

### Log Examples

```
[INFO] 2024-01-15 10:30:45,123 - __main__ - Running Telegram forwarder with API_ID=12345678
[INFO] 2024-01-15 10:30:46,456 - __main__ - Listening for messages from: My Channel
[INFO] 2024-01-15 10:31:00,789 - __main__ - New message received: Hello, this is a test message...
```

## Error Handling

The application handles various error scenarios:

- **Missing Environment Variables**: Validates all required variables on startup
- **Channel Access Issues**: Handles cases where the bot cannot access the specified channel
- **Network Errors**: Manages connection issues and API rate limits
- **Authentication Errors**: Handles session expiration and password requirements

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Disclaimer

This tool is for educational and legitimate automation purposes only. Ensure you comply with Telegram's Terms of Service and respect channel owners' rights when using this forwarder.
```


I've created a comprehensive README file for your Telegram forwarder project. The documentation includes:

**Key Sections:**
- **Project description** and overview
- **Table of contents (sommaire)** for easy navigation
- **Installation instructions** with virtual environment setup
- **Configuration guide** including how to get API credentials and generate session strings
- **Usage instructions** with examples
- **Project structure** overview
- **Logging configuration** details
- **Error handling** information

**Notable Features:**
- Step-by-step setup instructions
- Environment variables table with examples
- Code examples for session string generation
- Log format examples
- Contributing guidelines
- Proper disclaimers about Telegram ToS compliance

The README follows standard documentation practices and provides all the information needed for someone to understand, install, configure, and run your Telegram forwarder project.
