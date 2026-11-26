#include "asset_archive.hpp"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <array>
#include <algorithm>
#include <climits>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <vector>

namespace zumbi {
namespace {
constexpr std::array<char, 8> magic{'Z','B','R','P','A','K','1','\0'};
constexpr std::size_t key_size = 32, nonce_size = 12, tag_size = 16;
bool decode_key(const std::string& hex, std::array<unsigned char,key_size>& key) {
    if (hex.size()!=64) return false;
    for (std::size_t i=0;i<key_size;i++) {
        unsigned int v=0;
        try { v=static_cast<unsigned int>(std::stoul(hex.substr(i*2,2),nullptr,16)); }
        catch (...) { return false; }
        key[i]=static_cast<unsigned char>(v);
    }
    return true;
}
bool read_all(const std::filesystem::path& p,std::vector<unsigned char>& data) {
    std::ifstream f(p,std::ios::binary);
    if(!f) return false;
    f.seekg(0,std::ios::end); auto n=f.tellg();
    if(n<0) return false;
    data.resize(static_cast<std::size_t>(n)); f.seekg(0);
    return data.empty() || static_cast<bool>(f.read(reinterpret_cast<char*>(data.data()),n));
}
}
bool AssetArchive::pack(const std::filesystem::path& input,const std::filesystem::path& output,const std::string& key_hex,std::string& error) {
    std::array<unsigned char,key_size> key{}; std::vector<unsigned char> plain;
    if(!decode_key(key_hex,key)){error="Key must be 64 hexadecimal characters.";return false;}
    if(!read_all(input,plain)){error="Cannot read input file.";return false;}
    std::array<unsigned char,nonce_size> nonce{};
    std::array<unsigned char,tag_size> tag{};
    if(RAND_bytes(nonce.data(),static_cast<int>(nonce.size()))!=1){error="Secure random generation failed.";return false;}
    EVP_CIPHER_CTX* ctx=EVP_CIPHER_CTX_new();
    if(!ctx){error="Cipher context allocation failed.";return false;}
    std::vector<unsigned char> cipher(plain.size()+16);
    int out1=0,out2=0;
    bool ok=EVP_EncryptInit_ex(ctx,EVP_aes_256_gcm(),nullptr,nullptr,nullptr)==1 &&
        EVP_CIPHER_CTX_ctrl(ctx,EVP_CTRL_GCM_SET_IVLEN,static_cast<int>(nonce.size()),nullptr)==1 &&
        EVP_EncryptInit_ex(ctx,nullptr,nullptr,key.data(),nonce.data())==1 &&
        EVP_EncryptUpdate(ctx,cipher.data(),&out1,plain.data(),static_cast<int>(plain.size()))==1 &&
        EVP_EncryptFinal_ex(ctx,cipher.data()+out1,&out2)==1 &&
        EVP_CIPHER_CTX_ctrl(ctx,EVP_CTRL_GCM_GET_TAG,static_cast<int>(tag.size()),tag.data())==1;
    EVP_CIPHER_CTX_free(ctx);
    if(!ok){error="AES-256-GCM encryption failed.";return false;}
    cipher.resize(static_cast<std::size_t>(out1+out2));
    std::ofstream f(output,std::ios::binary);
    if(!f){error="Cannot create archive.";return false;}
    const std::uint64_t size=cipher.size();
    f.write(magic.data(),magic.size()); f.write(reinterpret_cast<const char*>(nonce.data()),nonce.size());
    f.write(reinterpret_cast<const char*>(&size),sizeof(size)); f.write(reinterpret_cast<const char*>(tag.data()),tag.size());
    f.write(reinterpret_cast<const char*>(cipher.data()),static_cast<std::streamsize>(cipher.size()));
    if(!f){error="Archive write failed.";return false;}
    return true;
}
bool AssetArchive::unpack(const std::filesystem::path& input,const std::filesystem::path& output,const std::string& key_hex,std::string& error) {
    std::array<unsigned char,key_size> key{}; std::vector<unsigned char> bytes;
    if(!decode_key(key_hex,key)){error="Key must be 64 hexadecimal characters.";return false;}
    if(!read_all(input,bytes) || bytes.size()<magic.size()+nonce_size+sizeof(std::uint64_t)+tag_size){error="Invalid or truncated archive.";return false;}
    std::size_t pos=0;
    if(!std::equal(magic.begin(),magic.end(),bytes.begin())){error="Invalid archive signature.";return false;}
    pos+=magic.size();
    std::array<unsigned char,nonce_size> nonce{}; std::copy_n(bytes.data()+pos,nonce.size(),nonce.data()); pos+=nonce.size();
    std::uint64_t size=0; std::memcpy(&size,bytes.data()+pos,sizeof(size)); pos+=sizeof(size);
    std::array<unsigned char,tag_size> tag{}; std::copy_n(bytes.data()+pos,tag.size(),tag.data()); pos+=tag.size();
    if(size!=bytes.size()-pos || size>static_cast<std::uint64_t>(INT_MAX)){error="Archive length mismatch or too large.";return false;}
    std::vector<unsigned char> plain(static_cast<std::size_t>(size)+16);
    EVP_CIPHER_CTX* ctx=EVP_CIPHER_CTX_new();
    if(!ctx){error="Cipher context allocation failed.";return false;}
    int out1=0,out2=0;
    bool ok=EVP_DecryptInit_ex(ctx,EVP_aes_256_gcm(),nullptr,nullptr,nullptr)==1 &&
        EVP_CIPHER_CTX_ctrl(ctx,EVP_CTRL_GCM_SET_IVLEN,static_cast<int>(nonce.size()),nullptr)==1 &&
        EVP_DecryptInit_ex(ctx,nullptr,nullptr,key.data(),nonce.data())==1 &&
        EVP_DecryptUpdate(ctx,plain.data(),&out1,bytes.data()+pos,static_cast<int>(size))==1 &&
        EVP_CIPHER_CTX_ctrl(ctx,EVP_CTRL_GCM_SET_TAG,static_cast<int>(tag.size()),tag.data())==1 &&
        EVP_DecryptFinal_ex(ctx,plain.data()+out1,&out2)==1;
    EVP_CIPHER_CTX_free(ctx);
    if(!ok){error="Authentication failed: wrong key or modified archive.";return false;}
    plain.resize(static_cast<std::size_t>(out1+out2));
    std::ofstream f(output,std::ios::binary);
    if(!f){error="Cannot create output file.";return false;}
    f.write(reinterpret_cast<const char*>(plain.data()),static_cast<std::streamsize>(plain.size()));
    if(!f){error="Output write failed.";return false;}
    return true;
}
}
