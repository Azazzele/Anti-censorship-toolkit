#ifndef CRYPTO_VARIANTS_H
#define CRYPTO_VARIANTS_H


#pragma once
#include <cstdint>
#include <vector>
#include <random>
#include <functional>

// Типы функций шифрования/дешифрования
typedef void (*EncryptFunc)(uint8_t* data, size_t size, uint8_t key);
typedef void (*DecryptFunc)(uint8_t* data, size_t size, uint8_t key);

// Структура одного варианта обфускации
struct CryptoVariant {
    uint8_t id;
    const char* name;
    EncryptFunc encrypt;
    DecryptFunc decrypt;
};

// ====================== 10 ВАРИАНТОВ ОБФУСКАЦИИ ======================

void variant1_encrypt(char* data, int len);  // Текущий (XOR + add)
void variant1_decrypt(char* data, int len);

void variant2_encrypt(char* data, int len);  // XOR + subtract
void variant2_decrypt(char* data, int len);

void variant3_encrypt(char* data, int len);  // Rolling XOR
void variant3_decrypt(char* data, int len);

void variant4_encrypt(char* data, int len);  // Add + XOR + rotate
void variant4_decrypt(char* data, int len);

void variant5_encrypt(char* data, int len);  // Bit flip + XOR
void variant5_decrypt(char* data, int len);

void variant6_encrypt(char* data, int len);  // MUL + XOR (mod 251)
void variant6_decrypt(char* data, int len);

void variant7_encrypt(char* data, int len);  // Swap bytes + XOR
void variant7_decrypt(char* data, int len);

void variant8_encrypt(char* data, int len);  // RC4-like (простая версия)
void variant8_decrypt(char* data, int len);

void variant9_encrypt(char* data, int len);  // Double XOR + negate
void variant9_decrypt(char* data, int len);

void variant10_encrypt(char* data, int len); // Chaotic (position dependent)
void variant10_decrypt(char* data, int len);

void variant11_encrypt(char* data, int len);
void variant11_decrypt(char* data, int len);

void variant12_encrypt(char* data, int len);
void variant12_decrypt(char* data, int len);

void variant13_encrypt(char* data, int len);
void variant13_decrypt(char* data, int len);

void variant14_encrypt(char* data, int len);
void variant14_decrypt(char* data, int len);

void variant15_encrypt(char* data, int len);
void variant15_decrypt(char* data, int len);

void variant16_encrypt(char* data, int len);
void variant16_decrypt(char* data, int len);

void variant17_encrypt(char* data, int len);
void variant17_decrypt(char* data, int len);

void variant18_encrypt(char* data, int len);
void variant18_decrypt(char* data, int len);

void variant19_encrypt(char* data, int len);
void variant19_decrypt(char* data, int len);

void variant20_encrypt(char* data, int len);
void variant20_decrypt(char* data, int len);



// Массив всех вариантов
extern const std::vector<CryptoVariant> crypto_variants;


CryptoVariant get_random_crypto_variant();
const CryptoVariant* get_crypto_variant_by_id(uint8_t id);


#endif