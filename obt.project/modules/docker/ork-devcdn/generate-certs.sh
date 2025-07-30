#!/bin/bash

# Generate self-signed certificate for HTTPS
# This is for development use only!

CERT_DIR="nginx/certs"

# Create certs directory if it doesn't exist
mkdir -p "$CERT_DIR"

# Generate self-signed certificate
openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
    -keyout "$CERT_DIR/nginx.key" \
    -out "$CERT_DIR/nginx.crt" \
    -subj "/C=US/ST=Development/L=Local/O=DevCDN/CN=localhost"

echo "Self-signed certificate generated in $CERT_DIR/"
echo "Certificate is valid for 365 days"
echo "Warning: This is for development use only!"