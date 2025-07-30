#!/bin/sh
# Custom entrypoint to enable real-time log monitoring in docker-compose

# Clear logs on startup
echo "Clearing logs..." >&2
rm -f /var/log/nginx/error.log
rm -f /var/log/nginx/access.log
mkdir -p /var/log/nginx
touch /var/log/nginx/error.log
touch /var/log/nginx/access.log

# Function to forward logs to stderr with color coding
forward_logs() {
    echo "Starting CDN log forwarding..." >&2
    
    # Wait for error log to be created
    while [ ! -f /var/log/nginx/error.log ]; do
        sleep 1
    done
    
    # Color codes
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    BLUE='\033[0;34m'
    PURPLE='\033[0;35m'
    CYAN='\033[0;36m'
    NC='\033[0m' # No Color
    
    # Tail the error log and colorize output
    tail -F /var/log/nginx/error.log | while read line; do
        # Add timestamp prefix for docker logs
        timestamp=$(date '+%H:%M:%S')
        
        # Color based on content
        if echo "$line" | grep -q "Upload completed:"; then
            echo -e "${timestamp} ${GREEN}✅ UPLOAD:${NC} $line" >&2
        elif echo "$line" | grep -q "Download completed:"; then
            echo -e "${timestamp} ${BLUE}⬇️  DOWNLOAD:${NC} $line" >&2
        elif echo "$line" | grep -q "Auth success:"; then
            echo -e "${timestamp} ${CYAN}🔐 AUTH:${NC} $line" >&2
        elif echo "$line" | grep -q "Auth failed"; then
            echo -e "${timestamp} ${RED}❌ AUTH FAIL:${NC} $line" >&2
        elif echo "$line" | grep -q "Upload failed\|Upload blocked"; then
            echo -e "${timestamp} ${RED}❌ UPLOAD FAIL:${NC} $line" >&2
        elif echo "$line" | grep -q "Download failed:"; then
            echo -e "${timestamp} ${RED}❌ DOWNLOAD FAIL:${NC} $line" >&2
        elif echo "$line" | grep -q "\[error\]\|\[crit\]\|\[alert\]\|\[emerg\]"; then
            echo -e "${timestamp} ${RED}🚨 ERROR:${NC} $line" >&2
        elif echo "$line" | grep -q "\[warn\]"; then
            echo -e "${timestamp} ${YELLOW}⚠️  WARNING:${NC} $line" >&2
        elif echo "$line" | grep -q "Upload started:"; then
            echo -e "${timestamp} ${PURPLE}⬆️  UPLOAD:${NC} $line" >&2
        elif echo "$line" | grep -q "Auth attempt:"; then
            echo -e "${timestamp} ${CYAN}🔐 AUTH:${NC} $line" >&2
        else
            echo -e "${timestamp} $line" >&2
        fi
    done &
}

# Start log forwarding in background
forward_logs &

# Print startup banner
echo "=====================================" >&2
echo "   Orkid Asset CDN Server" >&2
echo "=====================================" >&2
echo "HTTPS: https://localhost:8443" >&2
echo "HTTP:  http://localhost:8080 (redirects to HTTPS)" >&2
echo "=====================================" >&2
echo "" >&2

# Execute the original nginx command
exec /usr/local/openresty/bin/openresty -g "daemon off;"