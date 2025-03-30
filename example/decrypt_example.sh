#!/bin/bash
set -e

echo "[*] Decrypting encrypted.enc..."
./openfrogget --decrypt encrypted.enc

echo "[+] Decrypted file saved as: decrypted.out"
echo
echo "[+] Contents:"
cat decrypted.out
