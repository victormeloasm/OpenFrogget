#!/bin/bash
set -e

echo "[*] Generating keys..."
./openfrogget --generate-keys

echo "[*] Creating sample file..."
echo "This is a secret message from the Froggiverse." > examples/sample.txt

echo "[*] Encrypting sample file..."
./openfrogget --encrypt examples/sample.txt

echo "[+] Encrypted file generated: encrypted.enc"
