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
    
    printf("============================================================\n");
    printf(" Type in simple math problem and Enter to get the answer \n");
    printf("============================================================\n");
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
