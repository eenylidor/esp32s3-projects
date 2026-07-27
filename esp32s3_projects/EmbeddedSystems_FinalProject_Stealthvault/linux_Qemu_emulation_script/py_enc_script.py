import os
from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.backends import default_backend

def pad(data):
    length = 16 - (len(data) % 16)
    return data + bytes([length]) * length

def encrypt_vault(input_file, output_file, password):
    if os.path.exists(output_file):
        os.remove(output_file)
        
    salt = b"StealthVaultSalt"
    
    kdf = PBKDF2HMAC(
        algorithm=hashes.SHA256(),
        length=32,
        salt=salt,
        iterations=50000,
        backend=default_backend()
    )
    key = kdf.derive(password.encode('utf-8'))
    
    iv = os.urandom(16)
    cipher = Cipher(algorithms.AES(key), modes.CBC(iv), backend=default_backend())
    encryptor = cipher.encryptor()
    
    with open(input_file, 'rb') as f:
        plaintext = f.read()
        
    padded_data = pad(plaintext)
    ciphertext = encryptor.update(padded_data) + encryptor.finalize()
    
    with open(output_file, 'wb') as f:
        f.write(iv + ciphertext)
        
    print(f"[+] Successfully encrypted {input_file} to {output_file}")

encrypt_vault('my_passwords.txt', 'vault.enc', 'lidoryaya1')
