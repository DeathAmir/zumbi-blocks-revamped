#include "asset_archive.hpp"
#include <array>
#include <algorithm>
#include <climits>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#else
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

namespace zumbi {
namespace {
constexpr std::array<char, 8> magic{'Z','B','R','P','A','K','1',static_cast<char>(0)};
constexpr std::size_t key_size = 32, nonce_size = 12, tag_size = 16;

bool decode_key(const std::string& hex, std::array<unsigned char,key_size>& key) {
    if (hex.size() != 64) return false;
    for (std::size_t i = 0; i < key_size; ++i) {
        unsigned int value = 0;
        try {
            std::size_t used = 0;
            value = static_cast<unsigned int>(std::stoul(hex.substr(i * 2, 2), &used, 16));
            if (used != 2 || value > 255) return false;
        } catch (...) { return false; }
        key[i] = static_cast<unsigned char>(value);
    }
    return true;
}

bool read_all(const std::filesystem::path& path, std::vector<unsigned char>& data) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    file.seekg(0, std::ios::end);
    const auto length = file.tellg();
    if (length < 0 || static_cast<std::uint64_t>(length) > static_cast<std::uint64_t>(INT_MAX)) return false;
    data.resize(static_cast<std::size_t>(length));
    file.seekg(0);
    return data.empty() || static_cast<bool>(file.read(reinterpret_cast<char*>(data.data()), length));
}

#ifdef _WIN32
bool random_bytes(unsigned char* output, std::size_t size) {
    return size <= ULONG_MAX &&
        BCryptGenRandom(nullptr, output, static_cast<ULONG>(size), BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
}

bool aes_gcm_encrypt(const std::array<unsigned char,key_size>& key,
                     const std::array<unsigned char,nonce_size>& nonce,
                     const std::vector<unsigned char>& plain,
                     std::vector<unsigned char>& cipher,
                     std::array<unsigned char,tag_size>& tag) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_KEY_HANDLE cryptoKey = nullptr;
    std::vector<unsigned char> keyObject;
    bool success = false;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0) return false;
    if (BCryptSetProperty(algorithm, BCRYPT_CHAINING_MODE,
        reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),
        sizeof(BCRYPT_CHAIN_MODE_GCM), 0) != 0) goto cleanup;
    {
        DWORD objectLength = 0, returned = 0;
        if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&objectLength), sizeof(objectLength), &returned, 0) != 0) goto cleanup;
        keyObject.resize(objectLength);
    }
    if (BCryptGenerateSymmetricKey(algorithm, &cryptoKey, keyObject.data(),
        static_cast<ULONG>(keyObject.size()), const_cast<PUCHAR>(key.data()),
        static_cast<ULONG>(key.size()), 0) != 0) goto cleanup;
    {
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth;
        BCRYPT_INIT_AUTH_MODE_INFO(auth);
        auth.pbNonce = const_cast<PUCHAR>(nonce.data());
        auth.cbNonce = static_cast<ULONG>(nonce.size());
        auth.pbTag = tag.data();
        auth.cbTag = static_cast<ULONG>(tag.size());
        cipher.resize(plain.size() + 16);
        ULONG written = 0;
        static unsigned char empty = 0;
        const auto* input = plain.empty() ? &empty : plain.data();
        if (BCryptEncrypt(cryptoKey, const_cast<PUCHAR>(input),
            static_cast<ULONG>(plain.size()), &auth, nullptr, 0,
            cipher.data(), static_cast<ULONG>(cipher.size()), &written, 0) != 0) goto cleanup;
        cipher.resize(written);
        success = true;
    }
cleanup:
    if (cryptoKey) BCryptDestroyKey(cryptoKey);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    return success;
}

bool aes_gcm_decrypt(const std::array<unsigned char,key_size>& key,
                     const std::array<unsigned char,nonce_size>& nonce,
                     const std::array<unsigned char,tag_size>& tag,
                     const unsigned char* cipher, std::size_t cipherSize,
                     std::vector<unsigned char>& plain) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_KEY_HANDLE cryptoKey = nullptr;
    std::vector<unsigned char> keyObject;
    bool success = false;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0) return false;
    if (BCryptSetProperty(algorithm, BCRYPT_CHAINING_MODE,
        reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),
        sizeof(BCRYPT_CHAIN_MODE_GCM), 0) != 0) goto cleanup;
    {
        DWORD objectLength = 0, returned = 0;
        if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&objectLength), sizeof(objectLength), &returned, 0) != 0) goto cleanup;
        keyObject.resize(objectLength);
    }
    if (BCryptGenerateSymmetricKey(algorithm, &cryptoKey, keyObject.data(),
        static_cast<ULONG>(keyObject.size()), const_cast<PUCHAR>(key.data()),
        static_cast<ULONG>(key.size()), 0) != 0) goto cleanup;
    {
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth;
        BCRYPT_INIT_AUTH_MODE_INFO(auth);
        auth.pbNonce = const_cast<PUCHAR>(nonce.data());
        auth.cbNonce = static_cast<ULONG>(nonce.size());
        auth.pbTag = const_cast<PUCHAR>(tag.data());
        auth.cbTag = static_cast<ULONG>(tag.size());
        plain.resize(cipherSize + 16);
        ULONG written = 0;
        static unsigned char empty = 0;
        if (BCryptDecrypt(cryptoKey, const_cast<PUCHAR>(cipher ? cipher : &empty),
            static_cast<ULONG>(cipherSize), &auth, nullptr, 0,
            plain.data(), static_cast<ULONG>(plain.size()), &written, 0) != 0) goto cleanup;
        plain.resize(written);
        success = true;
    }
cleanup:
    if (cryptoKey) BCryptDestroyKey(cryptoKey);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    return success;
}
#else
bool random_bytes(unsigned char* output, std::size_t size) {
    return size <= INT_MAX && RAND_bytes(output, static_cast<int>(size)) == 1;
}

bool aes_gcm_encrypt(const std::array<unsigned char,key_size>& key,
                     const std::array<unsigned char,nonce_size>& nonce,
                     const std::vector<unsigned char>& plain,
                     std::vector<unsigned char>& cipher,
                     std::array<unsigned char,tag_size>& tag) {
    EVP_CIPHER_CTX* context = EVP_CIPHER_CTX_new();
    if (!context) return false;
    cipher.resize(plain.size() + 16);
    int first = 0, final = 0;
    static unsigned char empty = 0;
    const auto* input = plain.empty() ? &empty : plain.data();
    const bool ok =
        EVP_EncryptInit_ex(context, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
        EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(nonce.size()), nullptr) == 1 &&
        EVP_EncryptInit_ex(context, nullptr, nullptr, key.data(), nonce.data()) == 1 &&
        EVP_EncryptUpdate(context, cipher.data(), &first, input, static_cast<int>(plain.size())) == 1 &&
        EVP_EncryptFinal_ex(context, cipher.data() + first, &final) == 1 &&
        EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_GET_TAG, static_cast<int>(tag.size()), tag.data()) == 1;
    EVP_CIPHER_CTX_free(context);
    if (!ok) return false;
    cipher.resize(static_cast<std::size_t>(first + final));
    return true;
}

bool aes_gcm_decrypt(const std::array<unsigned char,key_size>& key,
                     const std::array<unsigned char,nonce_size>& nonce,
                     const std::array<unsigned char,tag_size>& tag,
                     const unsigned char* cipher, std::size_t cipherSize,
                     std::vector<unsigned char>& plain) {
    EVP_CIPHER_CTX* context = EVP_CIPHER_CTX_new();
    if (!context) return false;
    plain.resize(cipherSize + 16);
    int first = 0, final = 0;
    static unsigned char empty = 0;
    const auto* input = cipher ? cipher : &empty;
    const bool ok =
        EVP_DecryptInit_ex(context, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
        EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(nonce.size()), nullptr) == 1 &&
        EVP_DecryptInit_ex(context, nullptr, nullptr, key.data(), nonce.data()) == 1 &&
        EVP_DecryptUpdate(context, plain.data(), &first, input, static_cast<int>(cipherSize)) == 1 &&
        EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_SET_TAG, static_cast<int>(tag.size()), const_cast<unsigned char*>(tag.data())) == 1 &&
        EVP_DecryptFinal_ex(context, plain.data() + first, &final) == 1;
    EVP_CIPHER_CTX_free(context);
    if (!ok) return false;
    plain.resize(static_cast<std::size_t>(first + final));
    return true;
}
#endif
}

bool AssetArchive::pack(const std::filesystem::path& input, const std::filesystem::path& output,
                        const std::string& keyHex, std::string& error) {
    std::array<unsigned char,key_size> key{};
    std::vector<unsigned char> plain, cipher;
    if (!decode_key(keyHex, key)) { error = "Key must be 64 hexadecimal characters."; return false; }
    if (!read_all(input, plain)) { error = "Cannot read input file or file exceeds the supported size."; return false; }
    std::array<unsigned char,nonce_size> nonce{};
    std::array<unsigned char,tag_size> tag{};
    if (!random_bytes(nonce.data(), nonce.size())) { error = "Secure random generation failed."; return false; }
    if (!aes_gcm_encrypt(key, nonce, plain, cipher, tag)) { error = "AES-256-GCM encryption failed."; return false; }
    std::ofstream file(output, std::ios::binary);
    if (!file) { error = "Cannot create archive."; return false; }
    const std::uint64_t size = cipher.size();
    file.write(magic.data(), static_cast<std::streamsize>(magic.size()));
    file.write(reinterpret_cast<const char*>(nonce.data()), static_cast<std::streamsize>(nonce.size()));
    file.write(reinterpret_cast<const char*>(&size), sizeof(size));
    file.write(reinterpret_cast<const char*>(tag.data()), static_cast<std::streamsize>(tag.size()));
    if (!cipher.empty()) file.write(reinterpret_cast<const char*>(cipher.data()), static_cast<std::streamsize>(cipher.size()));
    if (!file) { error = "Archive write failed."; return false; }
    return true;
}

bool AssetArchive::unpack(const std::filesystem::path& input, const std::filesystem::path& output,
                          const std::string& keyHex, std::string& error) {
    std::array<unsigned char,key_size> key{};
    std::vector<unsigned char> bytes, plain;
    if (!decode_key(keyHex, key)) { error = "Key must be 64 hexadecimal characters."; return false; }
    if (!read_all(input, bytes) || bytes.size() < magic.size() + nonce_size + sizeof(std::uint64_t) + tag_size) {
        error = "Invalid or truncated archive."; return false;
    }
    std::size_t position = 0;
    if (!std::equal(magic.begin(), magic.end(), bytes.begin())) { error = "Invalid archive signature."; return false; }
    position += magic.size();
    std::array<unsigned char,nonce_size> nonce{};
    std::copy_n(bytes.data() + position, nonce.size(), nonce.data());
    position += nonce.size();
    std::uint64_t size = 0;
    std::memcpy(&size, bytes.data() + position, sizeof(size));
    position += sizeof(size);
    std::array<unsigned char,tag_size> tag{};
    std::copy_n(bytes.data() + position, tag.size(), tag.data());
    position += tag.size();
    if (size != bytes.size() - position || size > static_cast<std::uint64_t>(INT_MAX)) {
        error = "Archive length mismatch or too large."; return false;
    }
    if (!aes_gcm_decrypt(key, nonce, tag, bytes.data() + position, static_cast<std::size_t>(size), plain)) {
        error = "Authentication failed: wrong key or modified archive."; return false;
    }
    std::ofstream file(output, std::ios::binary);
    if (!file) { error = "Cannot create output file."; return false; }
    if (!plain.empty()) file.write(reinterpret_cast<const char*>(plain.data()), static_cast<std::streamsize>(plain.size()));
    if (!file) { error = "Output write failed."; return false; }
    return true;
}
}
