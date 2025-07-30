#!/bin/bash
# Interactive CDN startup script with real-time monitoring

echo "Starting Orkid Asset CDN Server..."
echo "================================="
echo "HTTPS: https://localhost:8443"
echo "HTTP:  http://localhost:8080"
echo "================================="
echo ""
echo "Startup Options:"
echo "1) Start with colored logs (requires bash)"
echo "2) Start with simple logs"
echo "3) Start in background with log monitor"
echo ""
read -p "Choose option (1-3): " choice

case $choice in
    1)
        echo "Starting with colored logs..."
        docker-compose up
        ;;
    2)
        echo "Starting with simple logs..."
        docker-compose -f docker-compose-simple.yml up
        ;;
    3)
        echo "Starting in background..."
        docker-compose up -d
        echo "Waiting for CDN to start..."
        sleep 3
        echo "Starting log monitor..."
        python monitor_logs.py ./logs
        ;;
    *)
        echo "Invalid choice. Starting with default settings..."
        docker-compose up
        ;;
esac