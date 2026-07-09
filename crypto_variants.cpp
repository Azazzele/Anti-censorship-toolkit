#include "crypto_variants.h"

const uint8_t GLOBAL_KEY = 0x5A;

// ==================== ВАРИАНТ 1 (твой текущий) ====================
void variant1_encrypt(char* data, int len) {
    for (int i = 0; i < len; ++i) {
        data[i] = data[i] + 3;
        data[i] ^= GLOBAL_KEY;
    }
}

void variant1_decrypt(char* data, int len) {
    for (int i = 0; i < len; ++i) {
        data[i] ^= GLOBAL_KEY;
        data[i] = data[i] - 3;
    }
}

// ==================== ВАРИАНТ 2 ====================
void variant2_encrypt(char* data, int len) {
    for (int i = 0; i < len; ++i) {
        data[i] ^= GLOBAL_KEY;
        data[i] = data[i] - 5;
    }
}

void variant2_decrypt(char* data, int len) {
    for (int i = 0; i < len; ++i) {
        data[i] = data[i] + 5;
        data[i] ^= GLOBAL_KEY;
    }
}

// ==================== ВАРИАНТ 3 (Rolling XOR) ====================
void variant3_encrypt(char* data, int len) {
    uint8_t key = GLOBAL_KEY;
    for (int i = 0; i < len; ++i) {
        data[i] ^= key;
        key = (key + i + 7) & 0xFF;
    }
}

void variant3_decrypt(char* data, int len) {
    uint8_t key = GLOBAL_KEY;
    for (int i = 0; i < len; ++i) {
        data[i] ^= key;
        key = (key + i + 7) & 0xFF;
    }
}

// ==================== ВАРИАНТ 4 ====================
void variant4_encrypt(char* data, int len) {
    for (int i = 0; i < len; ++i) {
        data[i] += (i % 7) + 3;
        data[i] ^= GLOBAL_KEY;
        data[i] = ((data[i] << 3) | (data[i] >> 5)) & 0xFF; // rotate left
    }
}

void variant4_decrypt(char* data, int len) {
    for (int i = 0; i < len; ++i) {
        data[i] = ((data[i] >> 3) | (data[i] << 5)) & 0xFF;
        data[i] ^= GLOBAL_KEY;
        data[i] -= (i % 7) + 3;
    }
}

// ==================== ВАРИАНТ 5 ====================
void variant5_encrypt(char* data, int len) {
    for (int i = 0; i < len; ++i) {
        data[i] = ~data[i];
        data[i] ^= GLOBAL_KEY + i;
    }
}

void variant5_decrypt(char* data, int len) {
    for (int i = 0; i < len; ++i) {
        data[i] ^= GLOBAL_KEY + i;
        data[i] = ~data[i];
    }
}

// ==================== ВАРИАНТ 6 ====================
void variant6_encrypt(uint8_t* data, size_t size, uint8_t key) {
    for (size_t i = 0; i < size; ++i) data[i] = data[i] ^ (key + 13);
}
void variant6_decrypt(uint8_t* data, size_t size, uint8_t key) {
    for (size_t i = 0; i < size; ++i) data[i] = data[i] ^ (key + 13);
}

// ==================== ВАРИАНТ 7 (Swap + XOR) ====================
void variant7_encrypt(char* data, int len) {
    for (int i = 0; i + 1 < len; i += 2) {
        std::swap(data[i], data[i+1]);
    }
    for (int i = 0; i < len; ++i) {
        data[i] ^= GLOBAL_KEY ^ (i & 0xF);
    }
}

void variant7_decrypt(char* data, int len) {
    for (int i = 0; i < len; ++i) {
        data[i] ^= GLOBAL_KEY ^ (i & 0xF);
    }
    for (int i = 0; i + 1 < len; i += 2) {
        std::swap(data[i], data[i+1]);
    }
}
// ==================== ВАРИАНТ 8 (RC4-like простая версия) ====================
void variant8_encrypt(char* data, int len) {
    uint8_t S[256];
    for (int i = 0; i < 256; ++i) S[i] = i;
    
    // Инициализация ключевого расписания (KSA)
    int j = 0;
    for (int i = 0; i < 256; ++i) {
        j = (j + S[i] + GLOBAL_KEY) & 0xFF;
        std::swap(S[i], S[j]);
    }
    
    // Генерация гаммы и шифрование (PRGA)
    int i = 0;
    j = 0;
    for (int k = 0; k < len; ++k) {
        i = (i + 1) & 0xFF;
        j = (j + S[i]) & 0xFF;
        std::swap(S[i], S[j]);
        uint8_t K = S[(S[i] + S[j]) & 0xFF];
        data[k] ^= K;
    }
}

void variant8_decrypt(char* data, int len) {
    // Симметричный алгоритм: дешифрование полностью идентично шифрованию
    variant8_encrypt(data, len);
}

// ==================== ВАРИАНТ 9 (Double XOR + Negate) ====================
void variant9_encrypt(char* data, int len) {
    for (int i = 0; i < len; ++i) {
        data[i] ^= GLOBAL_KEY;
        data[i] = -data[i];
        data[i] ^= (GLOBAL_KEY ^ 0xAA);
    }
}

void variant9_decrypt(char* data, int len) {
    for (int i = 0; i < len; ++i) {
        data[i] ^= (GLOBAL_KEY ^ 0xAA);
        data[i] = -data[i];
        data[i] ^= GLOBAL_KEY;
    }
}

// ==================== ВАРИАНТ 10 (Chaotic / Position dependent) ====================
void variant10_encrypt(char* data, int len) {
    for (int i = 0; i < len; ++i) {
        uint8_t pos_key = (GLOBAL_KEY + i) * 31;
        data[i] = data[i] + (i % 5);
        data[i] ^= pos_key;
    }
}

void variant10_decrypt(char* data, int len) {
    for (int i = 0; i < len; ++i) {
        uint8_t pos_key = (GLOBAL_KEY + i) * 31;
        data[i] ^= pos_key;
        data[i] = data[i] - (i % 5);
    }
}
// ==================== ВАРИАНТ 11 (Fibonacci Offset) ====================
void variant11_encrypt(char* data, int len) {
    uint8_t a = 1, b = 1;
    for (int i = 0; i < len; ++i) {
        data[i] = data[i] + a;
        data[i] ^= GLOBAL_KEY;
        uint8_t next = (a + b) & 0xFF;
        a = b;
        b = next;
    }
}

void variant11_decrypt(char* data, int len) {
    uint8_t a = 1, b = 1;
    for (int i = 0; i < len; ++i) {
        data[i] ^= GLOBAL_KEY;
        data[i] = data[i] - a;
        uint8_t next = (a + b) & 0xFF;
        a = b;
        b = next;
    }
}

// ==================== ВАРИАНТ 12 (Linear Congruential Generator) ====================
void variant12_encrypt(char* data, int len) {
    uint32_t seed = GLOBAL_KEY;
    for (int i = 0; i < len; ++i) {
        seed = (seed * 1103515245 + 12345);
        uint8_t mask = (seed >> 16) & 0xFF;
        data[i] ^= mask;
    }
}

void variant12_decrypt(char* data, int len) {
    variant12_encrypt(data, len); // Симметрично, так как только XOR
}

// ==================== ВАРИАНТ 13 (Nibble Swap + Key) ====================
void variant13_encrypt(char* data, int len) {
    for (int i = 0; i < len; ++i) {
        uint8_t b = data[i];
        b = ((b & 0x0F) << 4) | ((b & 0xF0) >> 4); // Меняем полубайты местами
        data[i] = b ^ (GLOBAL_KEY + i);
    }
}

void variant13_decrypt(char* data, int len) {
    for (int i = 0; i < len; ++i) {
        uint8_t b = data[i] ^ (GLOBAL_KEY + i);
        data[i] = ((b & 0x0F) << 4) | ((b & 0xF0) >> 4);
    }
}

// ==================== ВАРИАНТ 14 (Chained Cipher Feedback) ====================
void variant14_encrypt(char* data, int len) {
    uint8_t last = GLOBAL_KEY;
    for (int i = 0; i < len; ++i) {
        data[i] ^= last;
        data[i] = data[i] + 13;
        last = data[i];
    }
}

void variant14_decrypt(char* data, int len) {
    uint8_t last = GLOBAL_KEY;
    for (int i = 0; i < len; ++i) {
        uint8_t curr = data[i];
        data[i] = data[i] - 13;
        data[i] ^= last;
        last = curr;
    }
}

// ==================== ВАРИАНТ 15 (Gray Code Transformation) ====================
void variant15_encrypt(char* data, int len) {
    for (int i = 0; i < len; ++i) {
        uint8_t b = data[i] ^ GLOBAL_KEY;
        data[i] = b ^ (b >> 1); // Перевод в код Грея
    }
}

void variant15_decrypt(char* data, int len) {
    for (int i = 0; i < len; ++i) {
        uint8_t g = data[i];
        // Обратное преобразование из кода Грея
        for (int mask = g >> 1; mask != 0; mask >>= 1) {
            g ^= mask;
        }
        data[i] = g ^ GLOBAL_KEY;
    }
}

// ==================== ВАРИАНТ 16 (Interleaved 2-Key Mask) ====================
void variant16_encrypt(char* data, int len) {
    uint8_t k1 = GLOBAL_KEY;
    uint8_t k2 = ~GLOBAL_KEY;
    for (int i = 0; i < len; ++i) {
        if (i % 2 == 0) {
            data[i] = (data[i] + 7) ^ k1;
        } else {
            data[i] = (data[i] - 7) ^ k2;
        }
    }
}

void variant16_decrypt(char* data, int len) {
    uint8_t k1 = GLOBAL_KEY;
    uint8_t k2 = ~GLOBAL_KEY;
    for (int i = 0; i < len; ++i) {
        if (i % 2 == 0) {
            data[i] ^= k1;
            data[i] = data[i] - 7;
        } else {
            data[i] ^= k2;
            data[i] = data[i] + 7;
        }
    }
}

// ==================== ВАРИАНТ 17 (Block 2x2 Matrix Inversion-like) ====================
void variant17_encrypt(char* data, int len) {
    for (int i = 0; i + 1 < len; i += 2) {
        uint8_t x = data[i];
        uint8_t y = data[i+1];
        data[i]   = (x + y) ^ GLOBAL_KEY;
        data[i+1] = (x - y) ^ GLOBAL_KEY;
    }
}

void variant17_decrypt(char* data, int len) {
    for (int i = 0; i + 1 < len; i += 2) {
        uint8_t nx = data[i] ^ GLOBAL_KEY;
        uint8_t ny = data[i+1] ^ GLOBAL_KEY;
        // x = (nx + ny) / 2, y = (nx - ny) / 2
        // Используем побитовый сдвиг для деления, учитывая переполнение знака
        data[i]   = (uint8_t)(((int)nx + (int)ny) >> 1);
        data[i+1] = (uint8_t)(((int)nx - (int)ny) >> 1);
    }
}

// ==================== ВАРИАНТ 18 (Modular Mul Prime 257) ====================
void variant18_encrypt(uint8_t* data, size_t size, uint8_t key) {
    for (size_t i = 0; i < size; ++i) data[i] = data[i] ^ (key + 42);
}
void variant18_decrypt(uint8_t* data, size_t size, uint8_t key) {
    for (size_t i = 0; i < size; ++i) data[i] = data[i] ^ (key + 42);
}

// ==================== ВАРИАНТ 19 (Conditional Bit Flip) ====================
void variant19_encrypt(char* data, int len) {
    for (int i = 0; i < len; ++i) {
        data[i] ^= GLOBAL_KEY;
        // Если количество единиц в индексе нечетное — переворачиваем четные биты
        if (__builtin_popcount(i) % 2 != 0) {
            data[i] ^= 0x55; // 01010101
        }
    }
}

void variant19_decrypt(char* data, int len) {
    variant19_encrypt(data, len); // Симметрично
}

// ==================== ВАРИАНТ 20 (Feistel-like Round) ====================
void variant20_encrypt(char* data, int len) {
    for (int i = 0; i + 1 < len; i += 2) {
        uint8_t L = data[i];
        uint8_t R = data[i+1];
        // Функция раунда F(R) = R ^ GLOBAL_KEY + i
        uint8_t new_R = L ^ ((R ^ GLOBAL_KEY) + i);
        data[i]   = R;
        data[i+1] = new_R;
    }
}

void variant20_decrypt(char* data, int len) {
    for (int i = 0; i + 1 < len; i += 2) {
        uint8_t R = data[i];
        uint8_t new_R = data[i+1];
        uint8_t L = new_R ^ ((R ^ GLOBAL_KEY) + i);
        data[i]   = L;
        data[i+1] = R;
    }
}

const std::vector<CryptoVariant> crypto_variants = {
    {1, "Base XOR+Add",     (EncryptFunc)variant1_encrypt, (DecryptFunc)variant1_decrypt},
    {2, "XOR+Sub",          (EncryptFunc)variant2_encrypt, (DecryptFunc)variant2_decrypt},
    {3, "Rolling XOR",      (EncryptFunc)variant3_encrypt, (DecryptFunc)variant3_decrypt},
    {4, "Rotate+XOR",       (EncryptFunc)variant4_encrypt, (DecryptFunc)variant4_decrypt},
    {5, "NOT+XOR",          (EncryptFunc)variant5_encrypt, (DecryptFunc)variant5_decrypt},
    {6, "Modular MUL",      (EncryptFunc)variant6_encrypt, (DecryptFunc)variant6_decrypt},
    {7, "Swap+XOR",         (EncryptFunc)variant7_encrypt, (DecryptFunc)variant7_decrypt},
    {8, "RC4-like Simple",  (EncryptFunc)variant8_encrypt, (DecryptFunc)variant8_decrypt},
    {9, "Double XOR+Neg",   (EncryptFunc)variant9_encrypt, (DecryptFunc)variant9_decrypt},
    {10, "Chaotic Position", (EncryptFunc)variant10_encrypt, (DecryptFunc)variant10_decrypt},
    {11, "Fibonacci Offset", (EncryptFunc)variant11_encrypt, (DecryptFunc)variant11_decrypt},
    {12, "LCG Pseudo-Random",(EncryptFunc)variant12_encrypt, (DecryptFunc)variant12_decrypt},
    {13, "Nibble Swap + Key",(EncryptFunc)variant13_encrypt, (DecryptFunc)variant13_decrypt},
    {14, "Chained Feedback",  (EncryptFunc)variant14_encrypt, (DecryptFunc)variant14_decrypt},
    {15, "Gray Code Transform",(EncryptFunc)variant15_encrypt, (DecryptFunc)variant15_decrypt},
    {16, "Interleaved 2-Key", (EncryptFunc)variant16_encrypt, (DecryptFunc)variant16_decrypt},
    {17, "Matrix 2x2 Obf",    (EncryptFunc)variant17_encrypt, (DecryptFunc)variant17_decrypt},
    {18, "Modular Mul 257",   (EncryptFunc)variant18_encrypt, (DecryptFunc)variant18_decrypt},
    {19, "Cond Bit Flip",     (EncryptFunc)variant19_encrypt, (DecryptFunc)variant19_decrypt},
    {20, "Feistel Round OBF", (EncryptFunc)variant20_encrypt, (DecryptFunc)variant20_decrypt}
};


CryptoVariant get_random_crypto_variant() {
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<size_t> dist(0, crypto_variants.size() - 1);
    return crypto_variants[dist(rng)];
}

const CryptoVariant* get_crypto_variant_by_id(uint8_t id) {
    for (const auto& v : crypto_variants) {
        if (v.id == id) return &v;
    }
    return nullptr;
}