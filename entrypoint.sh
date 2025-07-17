#!/bin/bash

# Exit on any error
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to log messages
log() {
  echo -e "${GREEN}[$(date +'%Y-%m-%d %H:%M:%S')]${NC} $1"
}

error() {
  echo -e "${RED}[$(date +'%Y-%m-%d %H:%M:%S')] ERROR:${NC} $1" >&2
}

warning() {
  echo -e "${YELLOW}[$(date +'%Y-%m-%d %H:%M:%S')] WARNING:${NC} $1"
}

# Check if .env file exists
if [ ! -f .env ]; then
  error ".env file not found!"
  exit 1
fi

log "Checking required environment variables..."

# Source the .env file
source .env

# Check required variables
missing_vars=()

if [ -z "$TELEGRAM_API_ID" ]; then
  missing_vars+=("TELEGRAM_API_ID")
fi

if [ -z "$TELEGRAM_API_HASH" ]; then
  missing_vars+=("TELEGRAM_API_HASH")
fi

if [ -z "$SOURCE_CHANNEL" ]; then
  missing_vars+=("SOURCE_CHANNEL")
fi

# If any required variables are missing, exit with error
if [ ${#missing_vars[@]} -gt 0 ]; then
  error "Missing required environment variables: ${missing_vars[*]}"
  error "Please add these variables to your .env file"
  exit 1
fi

log "Required variables found: TELEGRAM_API_ID, TELEGRAM_API_HASH, SOURCE_CHANNEL"

# Check if TELEGRAM_SESSION_STRING exists
if [ -z "$TELEGRAM_SESSION_STRING" ]; then
  warning "TELEGRAM_SESSION_STRING not found. Generating session string..."

  # Check if session_string_gen.py exists
  if [ ! -f "session_string_gen.py" ]; then
    error "session_string_gen.py not found!"
    exit 1
  fi

  # Run the session string generator and capture output
  log "Running session string generator..."
  session_output=$(python session_string_gen.py)

  # Extract the session string from the output
  session_string=$(echo "$session_output" | grep "TELEGRAM_SESSION_STRING=" | cut -d'=' -f2-)

  if [ -z "$session_string" ]; then
    error "Failed to generate session string"
    exit 1
  fi

  # Add the session string to .env file
  echo "TELEGRAM_SESSION_STRING=$session_string" >>.env
  log "Session string added to .env file"

  # Show the session generation output
  echo "$session_output"

  # Re-source the .env file to load the new session string
  source .env
else
  log "TELEGRAM_SESSION_STRING found in environment"
fi

# Check if telegram_forwarder.py exists
if [ ! -f "telegram_forwarder.py" ]; then
  error "telegram_forwarder.py not found!"
  exit 1
fi

log "Starting Telegram forwarder..."

# Start the telegram forwarder in the background
python main.py &
forwarder_pid=$!

# Function to handle cleanup on exit
cleanup() {
  log "Shutting down..."
  if kill -0 $forwarder_pid 2>/dev/null; then
    kill $forwarder_pid
    wait $forwarder_pid
  fi
  exit 0
}

# Set up signal handlers
trap cleanup SIGTERM SIGINT

wait $forwarder_pid
