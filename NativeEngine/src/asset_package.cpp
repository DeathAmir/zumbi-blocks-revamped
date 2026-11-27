#include "asset_package.hpp"
#include <openssl/evp.h>
#include <zlib.h>
#include <array>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <unordered_map>
#include <sstream>
#include <vector>

namespace zumbi {
namespace {
constexpr char manifestMagic[] = "ZBR-ASSET-MANIFEST-1\n";
constexpr char objectMagic[] = "ZBRZLIB1";
using Bytes = std::vector<unsigned char>;

bool decodeHex(const std::string& text, Bytes& out, std::size_t expected) {
    if (text.size() != expected * 2) return false;
    out.clear();
    out.reserve(expected);
    auto digit=[](char c)->int {
        if(c>='0'&&c<='9') return c-'0';
        if(c>='a'&&c<='f') return c-'a'+10;
        if(c>='A'&&c<='F') return c-'A'+10;
        return -1;
    };
    for(std::size_t i=0;i<text.size();i+=2) {
        const int a=digit(text[i]), b=digit(text[i+1]);
        if(a<0||b<0) return false;
        out.push_back(static_cast<unsigned char>((a<<4)|b));
    }
    return true;
}
std::string encodeHex(const unsigned char* data,std::size_t size) {
    std::ostringstream out;
    out<<std::hex<<std::setfill('0');
    for(std::size_t i=0;i<size;++i) out<<std::setw(2)<<static_cast<unsigned int>(data[i]);
    return out.str();
}
bool readFile(const std::filesystem::path& path,Bytes& data) {
    std::ifstream in(path,std::ios::binary);
    if(!in) return false;
    in.seekg(0,std::ios::end);
    const auto n=in.tellg();
    if(n<0||static_cast<std::uint64_t>(n)>512ull*1024*1024) return false;
    data.resize(static_cast<std::size_t>(n));
    in.seekg(0);
    return data.empty()||static_cast<bool>(in.read(reinterpret_cast<char*>(data.data()),static_cast<std::streamsize>(data.size())));
}
bool writeFile(const std::filesystem::path& path,const Bytes& data) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    if(!out) return false;
    if(!data.empty()) out.write(reinterpret_cast<const char*>(data.data()),static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(out);
}
std::string hashBytes(const Bytes& data) {
    std::array<unsigned char,32> digest{};
    unsigned int length=0;
    static unsigned char empty=0;
    const auto* source=data.empty()?&empty:data.data();
    if(EVP_Digest(source,data.size(),digest.data(),&length,EVP_sha256(),nullptr)!=1||length!=digest.size()) return {};
    return encodeHex(digest.data(),digest.size());
}
std::string hexPath(const std::string& path) {
    return encodeHex(reinterpret_cast<const unsigned char*>(path.data()),path.size());
}
bool unhexPath(const std::string& text,std::string& path) {
    if(text.size()%2) return false;
    Bytes bytes;
    if(!decodeHex(text,bytes,text.size()/2)) return false;
    path.assign(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    return true;
}
void writeU64(std::ostream& out,std::uint64_t value) {
    for(unsigned i=0;i<8;++i) out.put(static_cast<char>((value>>(i*8))&0xff));
}
bool readU64(std::istream& in,std::uint64_t& value) {
    value=0;
    for(unsigned i=0;i<8;++i) {
        const int c=in.get();
        if(c==EOF) return false;
        value|=static_cast<std::uint64_t>(static_cast<unsigned char>(c))<<(i*8);
    }
    return true;
}
bool makeKey(const std::string& privateHex,EVP_PKEY** key) {
    Bytes seed;
    if(!decodeHex(privateHex,seed,32)) return false;
    *key=EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519,nullptr,seed.data(),seed.size());
    return *key!=nullptr;
}
bool signManifest(const std::string& manifest,const std::string& privateHex,Bytes& signature,std::string& pubHex) {
    EVP_PKEY* key=nullptr;
    if(!makeKey(privateHex,&key)) return false;
    std::array<unsigned char,32> pub{};
    std::size_t pubSize=pub.size();
    const bool gotPub=EVP_PKEY_get_raw_public_key(key,pub.data(),&pubSize)==1&&pubSize==pub.size();
    EVP_MD_CTX* ctx=EVP_MD_CTX_new();
    std::size_t sigSize=64;
    signature.resize(sigSize);
    const bool signedOk=ctx&&EVP_DigestSignInit(ctx,nullptr,nullptr,nullptr,key)==1&&
        EVP_DigestSign(ctx,signature.data(),&sigSize,
            reinterpret_cast<const unsigned char*>(manifest.data()),manifest.size())==1;
    if(ctx) EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(key);
    if(!gotPub||!signedOk||sigSize!=64) return false;
    signature.resize(sigSize);
    pubHex=encodeHex(pub.data(),pub.size());
    return true;
}
bool verifyManifest(const std::string& manifest,const Bytes& signature,const std::string& publicHex) {
    Bytes pub;
    if(signature.size()!=64||!decodeHex(publicHex,pub,32)) return false;
    EVP_PKEY* key=EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519,nullptr,pub.data(),pub.size());
    if(!key) return false;
    EVP_MD_CTX* ctx=EVP_MD_CTX_new();
    const bool ok=ctx&&EVP_DigestVerifyInit(ctx,nullptr,nullptr,nullptr,key)==1&&
        EVP_DigestVerify(ctx,signature.data(),signature.size(),
            reinterpret_cast<const unsigned char*>(manifest.data()),manifest.size())==1;
    if(ctx) EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(key);
    return ok;
}
bool compressAsset(const Bytes& input,Bytes& output) {
    uLongf bound=compressBound(static_cast<uLong>(input.size()));
    output.resize(static_cast<std::size_t>(bound));
    static unsigned char empty=0;
    const auto* source=input.empty()?&empty:input.data();
    const int result=compress2(output.data(),&bound,source,static_cast<uLong>(input.size()),Z_BEST_COMPRESSION);
    if(result!=Z_OK) return false;
    output.resize(static_cast<std::size_t>(bound));
    return true;
}
bool decompressAsset(const Bytes& input,std::uint64_t expected,Bytes& output) {
    if(expected>512ull*1024*1024||expected>std::numeric_limits<uLongf>::max()) return false;
    output.resize(static_cast<std::size_t>(expected));
    if(expected==0) {
        Bytes empty;
        Bytes check;
        return compressAsset(empty,check) && check==input;
    }
    uLongf actual=static_cast<uLongf>(expected);
    const int result=uncompress(output.data(),&actual,input.data(),static_cast<uLong>(input.size()));
    return result==Z_OK&&actual==expected;
}
bool writeObject(const std::filesystem::path& path,const Bytes& original) {
    Bytes compressed;
    if(!compressAsset(original,compressed)) return false;
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    if(!out) return false;
    out.write(objectMagic,sizeof(objectMagic)-1);
    writeU64(out,original.size());
    writeU64(out,compressed.size());
    if(!compressed.empty()) out.write(reinterpret_cast<const char*>(compressed.data()),static_cast<std::streamsize>(compressed.size()));
    return static_cast<bool>(out);
}
bool readObject(const std::filesystem::path& path,std::uint64_t expected,Bytes& original) {
    std::ifstream in(path,std::ios::binary);
    std::array<char,8> magic{};
    std::uint64_t raw=0,packed=0;
    if(!in||!in.read(magic.data(),magic.size())||magic!=std::array<char,8>{'Z','B','R','Z','L','I','B','1'}||
       !readU64(in,raw)||!readU64(in,packed)||raw!=expected||raw>512ull*1024*1024||packed>512ull*1024*1024) return false;
    Bytes bytes(static_cast<std::size_t>(packed));
    if(packed&&!in.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(packed))) return false;
    if(in.peek()!=EOF) return false;
    return decompressAsset(bytes,raw,original);
}
}
bool AssetPackage::derivePublicKey(const std::string& privateHex,std::string& publicHex,std::string& error) {
    EVP_PKEY* key=nullptr;
    if(!makeKey(privateHex,&key)) { error="Private key must be a 32-byte Ed25519 seed encoded as 64 hexadecimal characters."; return false; }
    std::array<unsigned char,32> pub{};
    std::size_t size=pub.size();
    const bool ok=EVP_PKEY_get_raw_public_key(key,pub.data(),&size)==1&&size==pub.size();
    EVP_PKEY_free(key);
    if(!ok) { error="Could not derive Ed25519 public key."; return false; }
    publicHex=encodeHex(pub.data(),pub.size());
    return true;
}
bool AssetPackage::pack(const std::filesystem::path& source,const std::filesystem::path& output,
                        const std::string& privateKeyHex,std::string& error) {
    if(!std::filesystem::is_directory(source)) { error="Asset source directory does not exist."; return false; }
    std::string manifest=manifestMagic;
    const auto content=output/"Game"/"ZBR"/"Content";
    const auto objects=content/"Objects";
    const auto absoluteOutput=std::filesystem::absolute(output).lexically_normal();
    std::vector<std::filesystem::path> files;
    for(const auto& entry:std::filesystem::recursive_directory_iterator(source)) {
        if(entry.is_symlink()) continue;
        if(!entry.is_regular_file()) continue;
        const auto path=entry.path();
        if(path.extension()==".cs"||path.extension()==".pdb") continue;
        if(path.string().find(absoluteOutput.string())==0) continue;
        files.push_back(path);
    }
    std::sort(files.begin(),files.end());
    if(files.empty()) { error="No asset files found."; return false; }
    std::unordered_map<std::string,bool> written;
    for(const auto& path:files) {
        Bytes bytes;
        if(!readFile(path,bytes)) { error="Cannot read asset: "+path.string(); return false; }
        const auto hash=hashBytes(bytes);
        if(hash.empty()) { error="SHA-256 calculation failed."; return false; }
        if(!written.contains(hash)) {
            if(!writeObject(objects/(hash+".zba"),bytes)) { error="Cannot compress asset: "+path.string(); return false; }
            written.emplace(hash,true);
        }
        const auto relative=std::filesystem::relative(path,source).generic_string();
        manifest+=hash+" "+std::to_string(bytes.size())+" "+hexPath(relative)+"\n";
    }
    Bytes signature;
    std::string publicHex;
    if(!signManifest(manifest,privateKeyHex,signature,publicHex)) { error="Ed25519 signing failed; check ZBR_ASSET_SIGNING_PRIVATE_KEY."; return false; }
    std::filesystem::create_directories(content);
    Bytes manifestBytes(manifest.begin(),manifest.end());
    if(!writeFile(content/"manifest.zbm",manifestBytes)||!writeFile(content/"manifest.sig",signature)) {
        error="Could not write signed asset manifest."; return false;
    }
    Bytes pubBytes(publicHex.begin(),publicHex.end());
    if(!writeFile(output/"Game"/"ZBR"/"Keys"/"asset_public_key.hex",pubBytes)) {
        error="Could not write package public key."; return false;
    }
    for(const auto& folder:{"Bin","Saves","Config","Logs","UserData"}) std::filesystem::create_directories(output/"Game"/folder);
    std::filesystem::create_directories(output/"Game"/"ZBR"/"Scripts");
    std::filesystem::create_directories(output/"Game"/"ZBR"/"Shaders");
    std::filesystem::create_directories(output/"Game"/"ZBR"/"Cache");
    std::ofstream readme(output/"Game"/"README.txt",std::ios::trunc);
    readme<<"Zumbi Blocks Revamped native layout\n"
          <<"Bin/       Native game executables and runtime libraries\n"
          <<"ZBR/Content/ SHA-256-addressed, Zlib-compressed assets and signed manifest\n"
          <<"ZBR/Scripts/ Compiled Zumbi Scripts modules (.zsc)\n"
          <<"ZBR/Shaders/ Renderer shader programs\n"
          <<"ZBR/Keys/   Public verification key (private signing key is never shipped)\n"
          <<"Saves/      Player and world saves\nConfig/      Runtime configuration\nLogs/        Diagnostic logs\nUserData/    User-generated data\n";
    if(!readme) { error="Could not write game layout README."; return false; }
    return true;
}
bool AssetPackage::verify(const std::filesystem::path& root,const std::string& trustedPublicKeyHex,std::string& error) {
    const auto content=root/"Game"/"ZBR"/"Content";
    Bytes manifestBytes,signatureBytes,pubBytes;
    if(!readFile(content/"manifest.zbm",manifestBytes)||!readFile(content/"manifest.sig",signatureBytes)||
       !readFile(root/"Game"/"ZBR"/"Keys"/"asset_public_key.hex",pubBytes)) {
        error="Signed asset manifest or public key is missing."; return false;
    }
    const std::string manifest(manifestBytes.begin(),manifestBytes.end());
    const std::string packagePub(pubBytes.begin(),pubBytes.end());
    if(packagePub!=trustedPublicKeyHex) { error="Package public key does not match the trusted key."; return false; }
    if(manifest.rfind(manifestMagic,0)!=0||!verifyManifest(manifest,signatureBytes,trustedPublicKeyHex)) {
        error="Asset manifest signature is invalid."; return false;
    }
    std::istringstream lines(manifest.substr(sizeof(manifestMagic)-1));
    std::string hash,pathHex;
    std::uint64_t size=0;
    std::size_t count=0;
    while(lines>>hash>>size>>pathHex) {
        Bytes decodedHash;
        if(!decodeHex(hash,decodedHash,32)||size>512ull*1024*1024) { error="Invalid manifest entry."; return false; }
        std::string relative;
        if(!unhexPath(pathHex,relative)||relative.empty()) { error="Invalid asset path encoding."; return false; }
        const std::filesystem::path rel(relative);
        if(rel.is_absolute()) { error="Absolute paths are forbidden in the manifest."; return false; }
        for(const auto& part:rel) if(part==".."||part==".") { error="Path traversal is forbidden in the manifest."; return false; }
        Bytes bytes;
        if(!readObject(content/"Objects"/(hash+".zba"),size,bytes)||hashBytes(bytes)!=hash) {
            error="Asset integrity check failed: "+relative; return false;
        }
        const auto mirror=root/"Game"/rel;
        if(std::filesystem::exists(mirror)) {
            Bytes mirrored;
            if(!std::filesystem::is_regular_file(mirror)||!readFile(mirror,mirrored)||hashBytes(mirrored)!=hash) {
                error="Mirrored game file does not match its signed asset: "+relative; return false;
            }
        }
        ++count;
    }
    if(!count) { error="Manifest contains no assets."; return false; }
    return true;
}

bool AssetPackage::selfTest(std::string& error) {
    const auto base=std::filesystem::temp_directory_path()/"zbr_asset_package_selftest";
    const auto source=base/"source";
    const auto output=base/"output";
    std::error_code ec;
    std::filesystem::remove_all(base,ec);
    std::filesystem::create_directories(source/"nested folder");
    {
        std::ofstream file(source/"nested folder"/"asset.bin",std::ios::binary);
        const std::string payload="Zumbi Blocks signed asset package test";
        file.write(payload.data(),static_cast<std::streamsize>(payload.size()));
        if(!file) { error="Could not create asset self-test fixture."; return false; }
    }
    {
        std::ofstream file(source/"empty.dat",std::ios::binary);
        if(!file) { error="Could not create empty asset fixture."; return false; }
    }
    const std::string privateKey="000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
    std::string publicKey;
    if(!derivePublicKey(privateKey,publicKey,error)||!pack(source,output,privateKey,error)||
       !verify(output,publicKey,error)) {
        std::filesystem::remove_all(base,ec);
        return false;
    }
    {
        const auto mirror=output/"Game"/"nested folder"/"asset.bin";
        std::filesystem::create_directories(mirror.parent_path());
        std::filesystem::copy_file(source/"nested folder"/"asset.bin",mirror);
        if(!verify(output,publicKey,error)) {
            std::filesystem::remove_all(base,ec);
            return false;
        }
        std::ofstream tampered(mirror,std::ios::binary|std::ios::trunc);
        tampered<<"tampered mirror";
        tampered.close();
        std::string mirrorError;
        if(verify(output,publicKey,mirrorError)) {
            std::filesystem::remove_all(base,ec);
            error="Tampered mirrored game file was accepted.";
            return false;
        }
    }
    {
        std::ofstream signature(output/"Game"/"ZBR"/"Content"/"manifest.sig",std::ios::binary|std::ios::app);
        signature.put('x');
    }
    std::string tamperError;
    const bool tamperAccepted=verify(output,publicKey,tamperError);
    std::filesystem::remove_all(base,ec);
    if(tamperAccepted) { error="Tampered manifest was accepted."; return false; }
    return true;
}
}
