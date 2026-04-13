/**
 * @file crypto_dsa.h
 * @brief Digital Signature Algorithm (DSA) cryptographic API.
 *
 * This module provides functions to generate key pairs,
 * sign messages, and verify digital signatures using DSA.
 */

#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define CRYPTO_DSA_PRIVATE_KEY_LENGTH 64		// Length of a DSA private key in bytes
#define CRYPTO_DSA_PUBLIC_KEY_LENGTH 32			// Length of a DSA public key in bytes
#define CRYPTO_DSA_SIGNATURE_LENGTH 64			// Length of a DSA signature in bytes


/**
 * @brief Type representing a DSA public key.
 *
 * The public key is stored as a fixed-size byte array.
 */
typedef uint8_t crypto_dsa_public_key_t[CRYPTO_DSA_PUBLIC_KEY_LENGTH];

/**
 * @brief Type representing a DSA private key.
 *
 * The private key is stored as a fixed-size byte array.
 */
typedef uint8_t crypto_dsa_private_key_t[CRYPTO_DSA_PRIVATE_KEY_LENGTH];

/**
 * @brief Type representing a DSA signature.
 *
 * The signature is stored as a fixed-size byte array.
 */
typedef uint8_t crypto_dsa_signature_t[CRYPTO_DSA_SIGNATURE_LENGTH];


/**
 * @brief Generate a DSA key pair.
 *
 * This function generates a new private key and the corresponding
 * public key.
 *
 * @param[out] private_key Buffer that will contain the generated private key.
 * @param[out] public_key  Buffer that will contain the generated public key.
 */
void crypto_dsa_generate_keys(crypto_dsa_private_key_t private_key,
                              crypto_dsa_public_key_t public_key);


/**
 * @brief Sign a message using a DSA private key.
 *
 * Generates a digital signature for the provided message.
 *
 * @param[in] private_key   Private key used for signing.
 * @param[in] data          Pointer to the message data.
 * @param[in] message_size  Size of the message in bytes.
 * @param[out] signature    Buffer where the generated signature will be stored.
 */
void crypto_dsa_sign(crypto_dsa_private_key_t private_key,
                     const uint8_t *data,
                     size_t message_size,
                     crypto_dsa_signature_t signature);


/**
 * @brief Verify a DSA signature.
 *
 * Checks whether a signature is valid for the given message
 * and public key.
 *
 * @param[in] public_key    Public key used for verification.
 * @param[in] data          Pointer to the message data.
 * @param[in] message_size  Size of the message in bytes.
 * @param[in] signature     Signature to verify.
 *
 * @return true  If the signature is valid.
 * @return false If the signature is invalid.
 */
bool crypto_dsa_verify(crypto_dsa_public_key_t public_key,
                       const uint8_t *data,
                       size_t message_size,
                       crypto_dsa_signature_t signature);
