/**
 * @file crypto_dsa.c
 * @brief Implementation of the DSA cryptographic API.
 *
 * This module implements key generation, message signing,
 * and signature verification using the Monocypher EdDSA
 * primitives. Keys are stored in a local file and reused
 * if already generated.
 */

#include <stdio.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <sys/random.h>
#include <string.h>

#include "monocypher.h"
#include "crypto_dsa.h"

static const char file_name[] = "../keys/keys.txt";

#define CRYPTO_SEED_LENGTH     32
#define FILE_LINE_MAX_LENGTH   (CRYPTO_DSA_PRIVATE_KEY_LENGTH * 2 + 2)  /* hex chars + newline + NUL */


/*
 * **********************************************************************************
 * Local APIs
 * **********************************************************************************
 */

/*
 * Uses the POSIX stat() function to determine whether the
 * file specified by the given filename exists.
 */
static bool _file_exists(const char *file_name)
{
    struct stat buffer;
    return stat(file_name, &buffer) == 0 ? true : false;
}

/*
 * A random seed is generated using the system random
 * generator and used to derive the private and public keys.
 */
static void _generate_dsa_keys(crypto_dsa_private_key_t private_key,
                               crypto_dsa_public_key_t public_key)
{
	uint8_t seed[CRYPTO_SEED_LENGTH];

	getrandom(seed, CRYPTO_SEED_LENGTH, 0);
	crypto_eddsa_key_pair(private_key, public_key, seed);
}

/*
 * Each byte is converted to a two-character hexadecimal
 * representation.
 */
static void _convert_hex_to_str(uint8_t *key, size_t length, char *str)
{
	if (!key || !str) {
		return;
	}

	for (int i = 0; i < (int)length; i++) {
		sprintf(str + i * 2, "%02x", key[i]);
	}

	str[length * 2] = '\0';
}

/*
 * Parses a hexadecimal string and converts it into a binary
 * buffer representation.
 */
static void _convert_str_to_hex(char *str, uint8_t *key, size_t length)
{
	if (!key || !str) {
		return;
	}

	for (int i = 0; i < (int)length; i++) {
        sscanf(str + 2 * i, "%2hhx", &key[i]);
    }
}

/*
 * The private and public keys are converted to hexadecimal
 * strings and stored in two separate lines in the file.
 */
static void _save_keys_to_file(crypto_dsa_private_key_t private_key,
                               crypto_dsa_public_key_t public_key,
                               FILE *file)
{
	char file_line[FILE_LINE_MAX_LENGTH];

	if (!file) {
		return;
	}

	_convert_hex_to_str(private_key, CRYPTO_DSA_PRIVATE_KEY_LENGTH, file_line);
	fprintf(file, "%s\n", file_line);

	_convert_hex_to_str(public_key, CRYPTO_DSA_PUBLIC_KEY_LENGTH, file_line);
	fprintf(file, "%s\n", file_line);
}

/*
 * Creates the key file, generates a new key pair,
 * and saves the keys to persistent storage.
 */
static void _generate_new_keys(crypto_dsa_private_key_t private_key,
                               crypto_dsa_public_key_t public_key)
{
	FILE *file;

	file = fopen(file_name, "w");
	if (!file) {
		printf("Failed to generate keys\n");
		return;
	}

	_generate_dsa_keys(private_key, public_key);
	_save_keys_to_file(private_key, public_key, file);
	fclose(file);
}

/*
 * Reads the stored private and public keys from the key file
 * and converts them from hexadecimal string format to binary.
 */
static void _load_keys_from_file(crypto_dsa_private_key_t private_key,
                                 crypto_dsa_public_key_t public_key)
{
	FILE *file;
	char file_line[FILE_LINE_MAX_LENGTH];

	file = fopen(file_name, "r");
	if (!file) {
		printf("Failed to open keys file\n");
		return;
	}

	fgets(file_line, FILE_LINE_MAX_LENGTH, file);
	file_line[strcspn(file_line, "\n")] = '\0';
	_convert_str_to_hex(file_line, private_key, CRYPTO_DSA_PRIVATE_KEY_LENGTH);

	fgets(file_line, FILE_LINE_MAX_LENGTH, file);
	file_line[strcspn(file_line, "\n")] = '\0';
	_convert_str_to_hex(file_line, public_key, CRYPTO_DSA_PUBLIC_KEY_LENGTH);

	fclose(file);
}

/*
 * **********************************************************************************
 * External APIs
 * **********************************************************************************
 */

/**
 * @brief Generate or load a DSA key pair.
 *
 * If a key file already exists, the keys are loaded from the file.
 * Otherwise, a new key pair is generated and stored.
 *
 * @param[out] private_key Private key buffer.
 * @param[out] public_key  Public key buffer.
 */
void crypto_dsa_generate_keys(crypto_dsa_private_key_t private_key,
                              crypto_dsa_public_key_t public_key)
{
	if (_file_exists(file_name)) {
		printf("Keys file found, read keys...\n");
		_load_keys_from_file(private_key, public_key);
	} else {
		printf("Keys file not found, generate new keys...\n");
		_generate_new_keys(private_key, public_key);
	}
}

/**
 * @brief Sign a message using a private key.
 *
 * Generates a digital signature using the EdDSA algorithm.
 *
 * @param[in]  private_key   Private key used for signing.
 * @param[in]  data          Pointer to the message buffer.
 * @param[in]  message_size  Size of the message in bytes.
 * @param[out] signature     Generated signature.
 */
void crypto_dsa_sign(crypto_dsa_private_key_t private_key,
                     const uint8_t *data,
                     size_t message_size,
                     crypto_dsa_signature_t signature)
{
	if (!data || !private_key || !signature) {
		return;
	}

	crypto_eddsa_sign(signature, private_key, data, message_size);
}

/**
 * @brief Verify a digital signature.
 *
 * Checks whether the provided signature is valid for
 * the given message and public key.
 *
 * @param[in] public_key     Public key used for verification.
 * @param[in] data           Pointer to the message buffer.
 * @param[in] message_size   Size of the message in bytes.
 * @param[in] signature      Signature to verify.
 *
 * @return true  If the signature is valid.
 * @return false If the signature is invalid.
 */
bool crypto_dsa_verify(crypto_dsa_public_key_t public_key,
                       const uint8_t *data,
                       size_t message_size,
                       crypto_dsa_signature_t signature)
{
	if (!data || !public_key || !signature) {
		return false;
	}

	return crypto_eddsa_check(signature, public_key, data, message_size) == 0;
}
