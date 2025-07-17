# Use Python 3.11 slim image
FROM python:3.11-slim

# Set working directory
WORKDIR /app

# Install system dependencies
RUN apt-get update && apt-get install -y \
    bash \
    && rm -rf /var/lib/apt/lists/*

# Copy requirements first for better caching
COPY requirements.txt .

# Install Python dependencies
RUN pip install --no-cache-dir -r requirements.txt

# Copy application files
COPY telegram_forwarder.py .
COPY session_string_gen.py .
COPY entrypoint.sh .
COPY main.py .

# Copy .env file
COPY .env .

# Make entrypoint script executable
RUN chmod +x entrypoint.sh

# Use entrypoint script
ENTRYPOINT ["./entrypoint.sh"]