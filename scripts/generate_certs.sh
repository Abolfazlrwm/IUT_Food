#!/usr/bin/env sh
# ---------------------------------------------------------------------------
# Generates a self-signed TLS certificate for local IUT-Food demos:
#
#   sh scripts/generate_certs.sh
#   ./build/iut-food-server --tls-cert certs/server.crt --tls-key certs/server.key
#
# The certificate is valid for localhost and 127.0.0.1 (SAN included, as
# modern TLS clients require) and is meant for development only.  For a
# real deployment use a CA-signed certificate (e.g. via Let's Encrypt) and
# keep the private key out of any shared folder.
# ---------------------------------------------------------------------------
set -e

cd "$(dirname "$0")/.."
mkdir -p certs

openssl req -x509 -newkey rsa:2048 -nodes -days 825 \
    -keyout certs/server.key -out certs/server.crt \
    -subj "/CN=localhost/O=IUT-Food Demo" \
    -addext "subjectAltName=DNS:localhost,IP:127.0.0.1"

chmod 600 certs/server.key

echo
echo "Done: certs/server.crt + certs/server.key"
echo "Start the server with:"
echo "  ./build/iut-food-server --tls-cert certs/server.crt --tls-key certs/server.key"
echo "Then tick 'TLS' in the client login dialog (and 'accept self-signed')."
