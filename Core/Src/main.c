#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "crypto_dsa.h"

/**
 * @def FILE_NAME_MAX_LENGTH
 * @brief Maximum length for input file names.
 */
#define FILE_NAME_MAX_LENGTH		64

/**
 * @def SIGNED_FILE_NAME_MAX_LENGTH
 * @brief Maximum length for signed file names (includes ".sign" suffix).
 */
#define SIGNED_FILE_NAME_MAX_LENGTH	(FILE_NAME_MAX_LENGTH + 6)

/**
 * @def SIGNED_FILE_MAGIC_NUMBER
 * @brief Magic number used to identify a signed file footer ("BOOT").
 */
#define SIGNED_FILE_MAGIC_NUMBER	0x424F4F54

/**
 * @struct signed_file_footer_t
 * @brief Footer appended to signed binary files.
 *
 * This structure is appended at the end of a signed binary file.
 * It contains metadata and the cryptographic signature.
 */
typedef struct {
	/** Magic number to identify the footer */
	uint32_t magic;

	/** Original file length (without footer) */
	uint32_t file_length;

	/** Digital signature of the file */
	crypto_dsa_signature_t signature;
} __attribute__((packed)) signed_file_footer_t;


/** Global input file name buffer */
char input_file_name[FILE_NAME_MAX_LENGTH];

/** Global public key used for signature verification */
crypto_dsa_public_key_t public_key;

/** Global private key used for signing */
crypto_dsa_private_key_t private_key;


/**
 * @brief Sign a binary file and append a footer containing the signature.
 *
 * This function reads a binary file, computes its DSA signature,
 * and appends a footer containing:
 * - Magic number
 * - Original file size
 * - Signature
 *
 * @param filename Path to the input binary file.
 * @param private_key Private key used for signing.
 * @param signed_file_length Output parameter storing total size of signed file.
 *
 * @return Pointer to allocated buffer containing signed file data,
 *         or NULL on failure. Caller must free the buffer.
 */
static unsigned char *_sign_binary_file(const char *filename,
                                        crypto_dsa_private_key_t private_key,
                                        size_t *signed_file_length)
{
	FILE *file;
	size_t file_size;
	size_t read_length;
	signed_file_footer_t footer;

    file = fopen(filename, "rb");
    if (!file) {
        printf("Failed to read the binary file to sign\n");
        return NULL;
    }

    fseek(file, 0, SEEK_END);      /* seek to end to measure file size */
    file_size = ftell(file);
    rewind(file);

    /* allocate space for file content + appended footer */
    unsigned char *signed_file_buffer = malloc(file_size + sizeof(signed_file_footer_t));
    if (!signed_file_buffer) {
        fclose(file);
        return NULL;
    }

    read_length = fread(signed_file_buffer, 1, file_size, file);
    fclose(file);

    if (read_length != file_size) {
        printf("Failed to read the binary file\n");
        free(signed_file_buffer);
        return NULL;
    }

    crypto_dsa_sign(private_key, signed_file_buffer, file_size, footer.signature);
    footer.magic = SIGNED_FILE_MAGIC_NUMBER;
    footer.file_length = file_size;

    memcpy(signed_file_buffer + file_size, &footer, sizeof(signed_file_footer_t));

    *signed_file_length = file_size + sizeof(signed_file_footer_t);
    return signed_file_buffer;
}

/**
 * @brief Write a signed binary buffer to disk.
 *
 * @param filename Output file path.
 * @param buffer Pointer to data to write.
 * @param size Size of the buffer in bytes.
 */
static void _generate_signed_binary_file(const char *filename,
                                         unsigned char *buffer,
                                         size_t size)
{
    FILE *file;
    size_t written;

    file = fopen(filename, "wb");
    if (!file) {
        printf("Failed to create output file\n");
        return;
    }

    written = fwrite(buffer, 1, size, file);
    fclose(file);

    if (written != size) {
        printf("Write error\n");
    }
}

/**
 * @brief Verify the signature of a signed binary file.
 *
 * This function reads a signed binary file, extracts the footer,
 * validates the magic number, and verifies the signature using
 * the provided public key.
 *
 * @param filename Path to the signed binary file.
 * @param public_key Public key used for verification.
 *
 * @return true if the file was read successfully (result is printed),
 *         false on file or memory error.
 */
static bool _verify_signature(const char *filename,
                              crypto_dsa_public_key_t public_key)
{
	FILE *file;
	size_t file_size;
	size_t read_length;
	signed_file_footer_t footer;
	bool valid_signature;

    file = fopen(filename, "rb");
    if (!file) {
        printf("Failed to read the signed binary file\n");
        return false;
    }

    fseek(file, 0, SEEK_END);      /* seek to end to measure file size */
    file_size = ftell(file);
    rewind(file);

    unsigned char *signed_file_buffer = malloc(file_size);
    if (!signed_file_buffer) {
        fclose(file);
        return false;
    }

    read_length = fread(signed_file_buffer, 1, file_size, file);
    fclose(file);

    if (read_length != file_size) {
        printf("Failed to read the binary file\n");
        free(signed_file_buffer);
        return false;
    }

    /* extract footer from the end of the file */
    memcpy(&footer,
           signed_file_buffer + file_size - sizeof(footer),
           sizeof(footer));

    if (footer.magic != SIGNED_FILE_MAGIC_NUMBER) {
        printf("Invalid file: footer magic number not found\n");
        free(signed_file_buffer);
        return false;
    }

    /* verify only the original content, not the footer itself */
    valid_signature = crypto_dsa_verify(public_key,
                                        signed_file_buffer,
                                        footer.file_length,
                                        footer.signature);

    printf("%s\n", valid_signature ? "Signature is valid" : "Signature not valid");

    free(signed_file_buffer);
    return true;
}

/**
 * @brief Main entry point.
 *
 * Usage:
 *   signing_tool sign   <binary_file>  -- Sign a binary and write <binary_file>.sign
 *   signing_tool verify <binary_file>  -- Verify the signature of a signed file
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 *
 * @return 0 on success, -1 on invalid arguments.
 */
int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <sign|verify> <binary_file>\n", argv[0]);
        return -1;
    }

    const char *mode = argv[1];
    if (strcmp(mode, "sign") != 0 && strcmp(mode, "verify") != 0) {
        printf("Error: unknown mode '%s'. Use 'sign' or 'verify'.\n", mode);
        return -1;
    }

    strncpy(input_file_name, argv[2], FILE_NAME_MAX_LENGTH - 1);
    input_file_name[FILE_NAME_MAX_LENGTH - 1] = '\0';  /* ensure null-termination if truncated */
    printf("binary file: %s\n", input_file_name);

    crypto_dsa_generate_keys(private_key, public_key);

    if (strcmp(mode, "sign") == 0) {
        /* Signing flow */
        unsigned char *signed_file;
        size_t signed_file_length;
        char signed_file_name[SIGNED_FILE_NAME_MAX_LENGTH];

        signed_file = _sign_binary_file(input_file_name,
                                        private_key,
                                        &signed_file_length);

        if (signed_file) {
            printf("Generating signed binary file\n");
            snprintf(signed_file_name,
                     SIGNED_FILE_NAME_MAX_LENGTH,
                     "%s.sign",
                     input_file_name);

            _generate_signed_binary_file(signed_file_name,
                                         signed_file,
                                         signed_file_length);
            free(signed_file);
        }
    } else {
        /* Verification flow */
        _verify_signature(input_file_name, public_key);
    }

    return 0;
}
