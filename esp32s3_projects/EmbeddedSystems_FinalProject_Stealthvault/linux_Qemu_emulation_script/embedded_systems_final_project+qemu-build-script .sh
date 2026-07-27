#!/usr/bin/env bash

print_welcome_header() {
    printf "\033[H\033[2J\033[3J"
    echo "============================================================"
    echo "|      LIDOR YAYA - EMBEDDED SYSTEMS FINAL PROJECT         |"
    echo "============================================================"
    echo ""
    echo "-----------------------------------------------------------------"
    echo "This script automates building and testing the stealth vault project."
    echo "To trigger the auth state, type: 1337 / 0 (with or without spaces) and press Enter."
    echo "-----------------------------------------------------------------"
    echo ""
    echo ""
    echo "In order to begin building, an internet connection is required for downloading:"
    echo "  - Tools & dependencies (gcc, qemu, python, etc.)"
    echo "  - Encryption/decryption packages"
    echo "  - QEMU emulator"
    echo "  - Minimal Linux kernel source"
    echo "  - BusyBox utilities"
    echo "  - C language libraries (mbedTLS)"
    echo ""
    echo "The entire build process may take 15-30 minutes depending on your internet speed & hardware."
    echo ""
    echo "The demonstration requires performing steps 1-4 at least once."
    echo "------------------------------------------------------------"
}

update_vault_password() {
    while true; do
        read -p "Enter the new 10-character vault password: " NEW_PASS
        
        if [ ${#NEW_PASS} -eq 10 ]; then
            sed -i -E "s|encrypt_vault\('my_passwords.txt', 'vault.enc', '[^']*'\)|encrypt_vault('my_passwords.txt', 'vault.enc', '$NEW_PASS')|g" "$WORKSPACE_PATH/code/py_enc_script.py"
            echo "Password successfully injected into py_enc_script.py"
            break
        else
            echo "Error: Password must be exactly 10 characters long. Please try again."
        fi
    done
}

while true; do
    print_welcome_header
    
    echo ""
    echo "  MENU OPTIONS"
    echo "  ------------"
    echo ""
    echo "    [0] Exit"
    echo "    [1] Setup workspace path for downloads"
    echo "    [2] Download & build tools (kernel, busybox, libs)"
    echo "    [3] Generate project scripts and sources"
    echo "    [4] Edit passwords and rebuild vault"
    echo "    [5] Launch minimal Linux (QEMU)"
    echo ""
    
    read -p "  Select an option [0-5]: " USER_CHOICE
    
    case "$USER_CHOICE" in
        0)
            echo "Exiting..."
            exit 0
            ;;
            
        1)
            read -p "Enter workspace path [Default: ~/final_project_workspace/]: " PATH_INPUT
            
            if [ -z "$PATH_INPUT" ]; then
                WORKSPACE_PATH="$HOME/final_project_workspace/"
            else
                WORKSPACE_PATH="$PATH_INPUT"
            fi
            
            WORKSPACE_PATH="${WORKSPACE_PATH/#\~/$HOME}"
            mkdir -p "$WORKSPACE_PATH"
            ;;
            
        2)

            if [ ! -d "$WORKSPACE_PATH" ]; then
                echo "Error: Workspace directory not found. Please run Option 1 first."
                sleep 2
                continue
            fi 

            sudo apt-get update
            sudo apt-get install -y build-essential gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf \
                                    qemu-system-arm qemu-user qemu-user-static python3 python3-pip \
                                    python3-venv python3-cryptography cpio gzip bc bison flex \
                                    libssl-dev libncurses-dev wget bzip2 git libmbedtls-dev

            mkdir -p "$WORKSPACE_PATH/kernel"
            mkdir -p "$WORKSPACE_PATH/busybox"
            mkdir -p "$WORKSPACE_PATH/C_libs"
            cd "$WORKSPACE_PATH/C_libs" || exit
            
            if [ ! -f "mbedtls/library/libmbedcrypto.a" ]; then
                rm -rf mbedtls
                git clone https://github.com/Mbed-TLS/mbedtls.git
                cd mbedtls || exit
                git checkout v3.5.0
                make CC=arm-linux-gnueabihf-gcc lib
                
                cd "$WORKSPACE_PATH" || exit
            fi
            
            mkdir -p "$WORKSPACE_PATH/code"

            
            
            cd "$WORKSPACE_PATH/kernel" || exit
            wget -c "https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.12.81.tar.xz"
            tar -xf "linux-6.12.81.tar.xz"
            cd "linux-6.12.81" || exit
            export ARCH=arm
            export SRC=$PWD
            export BLD="$WORKSPACE_PATH/kernel/build-arm"
            make O=$BLD -C $SRC ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- multi_v7_defconfig
            cd "$WORKSPACE_PATH/kernel/linux-6.12.81" || exit
            make O=$BLD -C $SRC ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j$(nproc) zImage dtbs

            cd "$WORKSPACE_PATH/busybox" || exit
            wget -c "https://busybox.net/downloads/busybox-1.36.1.tar.bz2"
            tar -xjf "busybox-1.36.1.tar.bz2"
            cd "$WORKSPACE_PATH/busybox/busybox-1.36.1" || exit
            make distclean
            make defconfig
            sed -i 's/^# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config
            sed -i 's/CONFIG_TC=y/# CONFIG_TC is not set/' .config
            make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j$(nproc)
            ;;
            
        3)

            if [ ! -f "$WORKSPACE_PATH/busybox/busybox-1.36.1/busybox" ] || [ ! -f "$WORKSPACE_PATH/kernel/linux-6.12.81/MAINTAINERS" ]; then
                echo "Error: Compiled Kernel or BusyBox missing. Please run Option 2 first."
                sleep 2
                continue
            fi

            cd "$WORKSPACE_PATH" || exit
            rm -rf initramfs initramfs.cpio initramfs.cpio.gz
            mkdir -p initramfs/{bin,sbin,etc,proc,sys,usr/bin,usr/sbin,dev}
            cp "$WORKSPACE_PATH/busybox/busybox-1.36.1/busybox" initramfs/bin/

            cd initramfs/bin/ || exit
            chmod +x busybox
            ln -s busybox uname
            ln -s busybox sh
            ln -s busybox mount
            ln -s busybox echo
            ln -s busybox ls
            ln -s busybox cat

cat << 'EOF' > init
#!/bin/sh

mount -t proc none /proc
mount -t sysfs none /sys
mount -t devtmpfs none /dev

busybox clear
echo "installing busybox from /bin/busybox..."
busybox sleep 3
cd /bin
busybox --install
clear
echo "Welcome to the minimal Linux environment with BusyBox!"
sleep 3
clear
echo 3 > /proc/sys/kernel/printk

while true; do
    printf '\033[H\033[2J\033[3J'
    
    cd /bin
    ./stealthvault_static_arm
    
    if [ $? -eq 99 ]; then
        printf '\033[H\033[2J\033[3J'
        echo "Exiting StealthVault Environment..."
        break
    fi
done

exec /bin/sh
EOF

            chmod +x init

            cd "$WORKSPACE_PATH/code" || exit

cat << 'EOF' > stealthvault.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>

#include "mbedtls/aes.h"
#include "mbedtls/md.h"
#include "mbedtls/pkcs5.h"

typedef enum { 
    MODE_DECOY_CALC,
    MODE_PASS_AUTH,
    MODE_ACTIVE_VAULT,
    MODE_SECURE_WIPE
} SystemState;

SystemState currentState = MODE_DECOY_CALC;
char derived_key[11];
char* vault_ram_buffer = NULL;
size_t vault_buffer_size = 0;


void remove_spaces(char* s) {
    char* d = s;
    do {
        while (isspace((unsigned char)*s)) {
            s++;
        }
    } while ((*d++ = *s++));
}

void read_password_hidden(char* buffer, int length) {
    struct termios oldt, newt;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;

    newt.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    for (int i = 0; i < length; i++) {
        buffer[i] = getchar();
        printf("*");
        fflush(stdout);
    }
    buffer[length] = '\0';
    
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    printf("\n");
    
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}


void run_camouflage() {
    char input[256];
    char clean_input[256];
    
    printf("\033[H\033[2J\033[3J");
    
    printf("==============================================================================\n");
    printf(" Enter a math problem ( Format: <num1> <operator> <num2> ): \n");
    printf(" Operators: addition: + , subtraction: - , multiplication: * , division: / \n");
    printf("==============================================================================\n");
    printf("(Type 'exit' to quit)\n> ");
    
    if (!fgets(input, sizeof(input), stdin)) return;
    
    strcpy(clean_input, input);
    remove_spaces(clean_input);
    
    if (strncmp(clean_input, "1337/0", 6) == 0) {
        currentState = MODE_PASS_AUTH;
        return;
    }
    
    if (strncmp(clean_input, "exit", 4) == 0) {
        printf("\033[H\033[2J\033[3J");
        exit(99);
    }
    
    double a, b, result;
    char op;
    int math_attempted = 0;
    
    if (sscanf(clean_input, "%lf%c%lf", &a, &op, &b) == 3) {
        math_attempted = 1;
        printf("\n");
        switch (op) {
            case '+':
                result = a + b;
                printf("Result: %.2lf\n", result);
                break;
            case '-':
                result = a - b;
                printf("Result: %.2lf\n", result);
                break;
            case '*':
                result = a * b;
                printf("Result: %.2lf\n", result);
                break;
            case '/':
                if (b == 0.0) {
                    printf("Error: Division by zero\n");
                } else {
                    result = a / b;
                    printf("Result: %.2lf\n", result);
                }
                break;
            default:
                printf("Error: Unknown operator\n");
                break;
        }
    } else if (strlen(clean_input) > 0) {
        math_attempted = 1;
        printf("\nSyntax Error\n");
    }

    if (math_attempted) {
        printf("\npress ENTER to ReRun...");
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
    }
}

void run_auth() {
    printf("\033[H\033[2J\033[3J");
    printf("\n[Authentication Protocol Initiated]\n");
    printf("Enter 10-character Vault Password:\n> ");
    
    read_password_hidden(derived_key, 10);
    
    printf("[+] Authenticating...\n");
    currentState = MODE_ACTIVE_VAULT;
}

void run_vault() {
    FILE *f = fopen("/bin/vault.enc", "rb");
    if (!f) {
        printf("Error: /bin/vault.enc not found. Returning to decoy mode.\n");
        printf("\npress ENTER to continue...");
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
        currentState = MODE_SECURE_WIPE;
        return;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 16) {
        printf("Error: vault.enc is too small or corrupted.\n");
        fclose(f);
        currentState = MODE_SECURE_WIPE;
        return;
    }

    unsigned char *file_buffer = (unsigned char *)malloc(file_size);
    if (!file_buffer) {
        printf("Error: Memory allocation failed.\n");
        fclose(f);
        currentState = MODE_SECURE_WIPE;
        return;
    }
    
    size_t bytes_read = fread(file_buffer, 1, file_size, f);
    
    if (bytes_read != file_size) {
        printf("Error: Failed to read the complete vault.enc file.\n");
        free(file_buffer);
        fclose(f);
        currentState = MODE_SECURE_WIPE;
        return;
    }
    fclose(f);

    printf("[+] Deriving AES Key using mbedTLS PBKDF2(SHA-256)...\n");

    unsigned char derived_aes_key[32];
    const unsigned char salt[] = "StealthVaultSalt";
    mbedtls_md_context_t md_ctx;
    
    mbedtls_md_init(&md_ctx);
    mbedtls_md_setup(&md_ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_pkcs5_pbkdf2_hmac(&md_ctx, (const unsigned char*)derived_key, strlen(derived_key),
                              salt, strlen((char*)salt), 50000, 32, derived_aes_key);
    mbedtls_md_free(&md_ctx);

    printf("[+] Decrypting Vault...\n");

    unsigned char iv[16];
    memcpy(iv, file_buffer, 16);

    size_t ciphertext_len = file_size - 16;
    unsigned char *ciphertext = file_buffer + 16;
    
    vault_buffer_size = ciphertext_len + 1;
    vault_ram_buffer = (char*)malloc(vault_buffer_size);
    if (!vault_ram_buffer) {
        printf("Error: Memory allocation failed.\n");
        memset(derived_aes_key, 0, sizeof(derived_aes_key)); 
        free(file_buffer);
        currentState = MODE_SECURE_WIPE;
        return;
    }

    mbedtls_aes_context aes_ctx;
    mbedtls_aes_init(&aes_ctx);
    mbedtls_aes_setkey_dec(&aes_ctx, derived_aes_key, 256);
    
    mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_DECRYPT, ciphertext_len, iv, ciphertext, (unsigned char*)vault_ram_buffer);
    mbedtls_aes_free(&aes_ctx);

    int pad_len = vault_ram_buffer[ciphertext_len - 1];
    if(pad_len > 0 && pad_len <= 16) {
         vault_ram_buffer[ciphertext_len - pad_len] = '\0';
    } else {
         vault_ram_buffer[ciphertext_len] = '\0';
    }

    memset(derived_aes_key, 0, sizeof(derived_aes_key));
    free(file_buffer);

    printf("\033[H\033[2J\033[3J");
    printf("\n--- SECURE VAULT ---\n");
    printf("%s\n", vault_ram_buffer);
    printf("\nPress 'q' to lock vault and exit: ");

    int cmd;
    while ((cmd = getchar()) != EOF) {
        if (cmd == 'q') {
            currentState = MODE_SECURE_WIPE;
            break;
        }
    }
    
    while ((cmd = getchar()) != '\n' && cmd != EOF);
}

void run_cleanup() {
    if (vault_ram_buffer != NULL) {
        memset(vault_ram_buffer, 0, vault_buffer_size);
        free(vault_ram_buffer);
        vault_ram_buffer = NULL;
        vault_buffer_size = 0;
    }
    
    memset(derived_key, 0, sizeof(derived_key));
    
    printf("\n[!] RAM securely wiped. Restarting system...\n\n");
    
    sleep(3); 
    exit(0);
}

int main() {
    while (1) {
        switch (currentState) {
            case MODE_DECOY_CALC:
                run_camouflage();
                break;
            case MODE_PASS_AUTH:
                run_auth();
                break;
            case MODE_ACTIVE_VAULT:
                run_vault();
                break;
            case MODE_SECURE_WIPE:
                run_cleanup();
                break;
        }
    }
    return 0;
}
EOF

            cat << 'EOF' > mkgen
#!/usr/bin/env bash

SOURCE=$1
FILENAME="${SOURCE%.*}"
MBEDTLS_PATH="${2:-__WORKSPACE_PATH__/C_libs/mbedtls}"

cat << MAKEFILE_EOF > Makefile
CC_ARM = arm-linux-gnueabihf-gcc
CFLAGS = -O2 -Wall
STATIC_FLAG = -static
INCLUDES = -I$MBEDTLS_PATH/include
LDFLAGS = -L$MBEDTLS_PATH/library -lmbedcrypto

.PHONY: build-static-arm clean

build-static-arm:
	\$(CC_ARM) \$(CFLAGS) \$(STATIC_FLAG) \$(INCLUDES) $SOURCE -o ${FILENAME}_static_arm \$(LDFLAGS)

clean:
	rm -f ${FILENAME}_static_arm Makefile
MAKEFILE_EOF
EOF

            sed -i "s|__WORKSPACE_PATH__|$WORKSPACE_PATH|g" mkgen
            chmod +x mkgen
            
            cat << 'EOF' > my_passwords.txt
welcome to lidors passwords file for embedded systems project!

-----------------------------
---- Services & websites ----
-----------------------------

ArielUni 
acc: ploni.almoni@msmail.ariel.ac.il 
pass: 4th_year_is_FUN 

yad2 
acc: ploni_almoni 
pass: almoni_ploni

---------------------
---- Email Boxes ----
---------------------

gmail1 
acc: email1@gmail.com 
pass: 123456 

gmail2 
acc: email2@gmail.com 
pass: qwerty 

EOF

            cat << 'EOF' > py_enc_script.py
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

encrypt_vault('my_passwords.txt', 'vault.enc', '_PASSWORD_')
EOF

            cd "$WORKSPACE_PATH/code" || exit
            ./mkgen stealthvault.c "$WORKSPACE_PATH/C_libs/mbedtls"
            make build-static-arm
            cp stealthvault_static_arm "$WORKSPACE_PATH/initramfs/bin/"
            make clean
            
            update_vault_password
            python3 py_enc_script.py
            mv vault.enc "$WORKSPACE_PATH/initramfs/bin/"

            cd "$WORKSPACE_PATH/initramfs" || exit
            find . | cpio -H newc -ov --owner root:root > ../initramfs.cpio 2>/dev/null
            cd "$WORKSPACE_PATH" || exit
            INIT_PATH=/$(cpio -t < initramfs.cpio 2>/dev/null | grep -E "/init$")
            gzip -f initramfs.cpio
            ;;
            
        4)

            if [ ! -f "$WORKSPACE_PATH/code/py_enc_script.py" ] || [ ! -f "$WORKSPACE_PATH/code/my_passwords.txt" ]; then
                echo "Error: Project source files missing. Please run Option 3 first."
                sleep 2
                continue
            fi

            cd "$WORKSPACE_PATH/code" || exit
            
            read -p "Do you want to edit the passwords file? (y/n): " EDIT_CHOICE
            if [[ "$EDIT_CHOICE" == [yY] || "$EDIT_CHOICE" == [yY][eE][sS] ]]; then
                read -p "Choose editor: 1) nano 2) vim: " EDITOR_CHOICE
                if [ "$EDITOR_CHOICE" = "1" ]; then
                    nano my_passwords.txt
                elif [ "$EDITOR_CHOICE" = "2" ]; then
                    vim my_passwords.txt
                fi
            fi
            
            read -p "Do you want to change the encryption password? (y/n): " PASS_CHOICE
            if [[ "$PASS_CHOICE" == [yY] || "$PASS_CHOICE" == [yY][eE][sS] ]]; then
                update_vault_password
            fi
            
            python3 py_enc_script.py
            make build-static-arm
            
            cp vault.enc "$WORKSPACE_PATH/initramfs/bin/"
            cp stealthvault_static_arm "$WORKSPACE_PATH/initramfs/bin/"
            make clean
            
            cd "$WORKSPACE_PATH/initramfs" || exit
            find . | cpio -H newc -ov --owner root:root > ../initramfs.cpio 2>/dev/null
            cd "$WORKSPACE_PATH" || exit
            INIT_PATH=/$(cpio -t < initramfs.cpio 2>/dev/null | grep -E "/init$")
            gzip -f initramfs.cpio
            ;;
            
        5)

            if [ ! -f "$WORKSPACE_PATH/initramfs.cpio.gz" ]; then
                echo "Error: Compressed initramfs image not found. Please run Option 3 or 4 first."
                sleep 2
                continue
            fi

            killall qemu-system-arm 2>/dev/null
            
            cd "$WORKSPACE_PATH" || exit
            
            qemu-system-arm \
            -M vexpress-a15 \
            -cpu cortex-a15 \
            -m 512 \
            -nographic \
            -kernel kernel/build-arm/arch/arm/boot/zImage \
            -dtb kernel/build-arm/arch/arm/boot/dts/arm/vexpress-v2p-ca15-tc1.dtb \
            -initrd initramfs.cpio.gz \
            -append "console=ttyAMA0 quiet loglevel=3 rdinit=${INIT_PATH}"
            
            stty sane
            ;;
            
        *)
            echo "Invalid option selected."
            sleep 2
            ;;
    esac
done