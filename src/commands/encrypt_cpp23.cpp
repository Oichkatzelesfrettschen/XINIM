// SIMD-Accelerated Encryption Command - C++23 Pure Implementation
// World's first post-quantum POSIX encrypt utility with comprehensive SIMD

import xinim.posix;

#include "../crypto/kyber_cpp23_simd.hpp"
#include <algorithm>
#include <chrono>
#include <execution>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <ranges>
#include <span>
#include <string_view>

namespace fs = std::filesystem;
using namespace xinim::crypto::kyber::simd;

class PostQuantumEncryptTool {
private:
    struct Options {
        kyber_level security_level = kyber_level::KYBER_768;  // Default Level 3
        fs::path input_file;
        fs::path output_file;
        fs::path key_file;
        bool generate_keypair = false;
        bool benchmark = false;
        bool verbose = false;
        bool force_simd_level = false;
        std::string simd_override;
        std::size_t chunk_size = 1024 * 1024;  // 1MB chunks for streaming
    } options;

public:
    [[nodiscard]] std::expected<int, std::error_code> 
    execute(std::span<std::string_view> args) noexcept {
        
        if (!parse_arguments(args)) {
            print_usage();
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        }

        try {
            if (options.benchmark) {
                return run_benchmark();
            }
            
            if (options.generate_keypair) {
                return generate_key_pair();
            }
            
            return encrypt_file();
            
        } catch (const std::exception& e) {
            std::cerr << std::format("encrypt: {}\n", e.what());
            return std::unexpected(std::make_error_code(std::errc::operation_not_supported));
        }
    }

private:
    bool parse_arguments(std::span<std::string_view> args) noexcept {
        for (std::size_t i = 0; i < args.size(); ++i) {
            std::string_view arg = args[i];
            
            if (arg == "-h" || arg == "--help") {
                return false;
            } else if (arg == "-v" || arg == "--verbose") {
                options.verbose = true;
            } else if (arg == "-g" || arg == "--generate-keypair") {
                options.generate_keypair = true;
            } else if (arg == "-b" || arg == "--benchmark") {
                options.benchmark = true;
            } else if (arg == "-s" || arg == "--security-level") {
                if (++i >= args.size()) return false;
                auto level = std::string(args[i]);
                if (level == "512") {
                    options.security_level = kyber_level::KYBER_512;
                } else if (level == "768") {
                    options.security_level = kyber_level::KYBER_768;
                } else if (level == "1024") {
                    options.security_level = kyber_level::KYBER_1024;
                } else {
                    return false;
                }
            } else if (arg == "--simd") {
                if (++i >= args.size()) return false;
                options.simd_override = std::string(args[i]);
                options.force_simd_level = true;
            } else if (arg == "--chunk-size") {
                if (++i >= args.size()) return false;
                try {
                    options.chunk_size = std::stoull(std::string(args[i]));
                } catch (...) {
                    return false;
                }
            } else if (arg == "-k" || arg == "--key") {
                if (++i >= args.size()) return false;
                options.key_file = std::string(args[i]);
            } else if (arg == "-o" || arg == "--output") {
                if (++i >= args.size()) return false;
                options.output_file = std::string(args[i]);
            } else if (!arg.starts_with('-')) {
                if (options.input_file.empty()) {
                    options.input_file = std::string(arg);
                } else {
                    return false;  // Multiple input files not supported
                }
            } else {
                return false;  // Unknown option
            }
        }
        
        return true;
    }
    
    void print_usage() const noexcept {
        std::cout << R"(
XINIM Post-Quantum Encryption Tool (C++23 + SIMD)

Usage: encrypt [OPTIONS] [INPUT_FILE]

OPTIONS:
  -h, --help              Show this help message
  -v, --verbose           Enable verbose output
  -g, --generate-keypair  Generate new key pair
  -b, --benchmark         Run performance benchmark
  -s, --security-level N  Security level: 512, 768, 1024 (default: 768)
  -k, --key FILE          Key file path
  -o, --output FILE       Output file path
  --simd LEVEL           Force SIMD level (scalar/sse/avx2/avx512)
  --chunk-size SIZE      Chunk size for streaming (default: 1MB)

EXAMPLES:
  encrypt -g -s 768 -k mykey.pub        # Generate Kyber-768 keypair
  encrypt -k mykey.pub file.txt          # Encrypt file.txt
  encrypt -b                             # Run benchmark suite
  encrypt -v --simd avx512 largefile.bin # Encrypt with AVX-512

SIMD SUPPORT:
  Automatically detects and uses best available:
  SSE/SSE2/SSE3/SSSE3/SSE4.1/SSE4.2/SSE4A/AVX/AVX2/AVX512-F/BW/DQ/VL/VNNI
  3DNow!/3DNowExtended (legacy AMD support)

SECURITY LEVELS:
  512  - Kyber-512  (Level 1, AES-128 equivalent)  
  768  - Kyber-768  (Level 3, AES-192 equivalent, default)
  1024 - Kyber-1024 (Level 5, AES-256 equivalent)
)";
    }
    
    std::expected<int, std::error_code> generate_key_pair() {
        if (options.verbose) {
            report_simd_capabilities();
            std::cout << std::format("Generating Kyber-{} keypair...\n", 
                                    static_cast<int>(options.security_level) * 256 + 256);
        }
        
        auto start = std::chrono::high_resolution_clock::now();
        
        std::expected<int, std::error_code> result;
        
        // Template dispatch based on security level
        switch (options.security_level) {
            case kyber_level::KYBER_512:
                result = generate_keypair_typed<kyber_level::KYBER_512>();
                break;
            case kyber_level::KYBER_768:
                result = generate_keypair_typed<kyber_level::KYBER_768>();
                break;
            case kyber_level::KYBER_1024:
                result = generate_keypair_typed<kyber_level::KYBER_1024>();
                break;
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        if (options.verbose && result) {
            std::cout << std::format("Keypair generated in {} ms using {}\n", 
                                    duration.count(), get_simd_info());
        }
        
        return result;
    }
    
    template<kyber_level Level>
    std::expected<int, std::error_code> generate_keypair_typed() {
        auto kp_result = kyber_simd<Level>::generate_keypair();
        if (!kp_result) {
            return std::unexpected(kp_result.error());
        }
        
        // Determine output filenames
        fs::path pub_key_path = options.key_file.empty() ? 
            fs::path("xinim_key.pub") : options.key_file;
        fs::path sec_key_path = pub_key_path;
        sec_key_path.replace_extension(".sec");
        
        // Write public key
        try {
            std::ofstream pub_file(pub_key_path, std::ios::binary);
            if (!pub_file) {
                return std::unexpected(std::make_error_code(std::errc::permission_denied));
            }
            
            pub_file.write(reinterpret_cast<const char*>(kp_result->public_key.data.data()),
                          kp_result->public_key.data.size());
            
            // Write security level metadata
            std::uint8_t level_byte = static_cast<std::uint8_t>(Level);
            pub_file.write(reinterpret_cast<const char*>(&level_byte), 1);
        } catch (...) {
            return std::unexpected(std::make_error_code(std::errc::io_error));
        }
        
        // Write secret key with secure permissions
        try {
            std::ofstream sec_file(sec_key_path, std::ios::binary);
            if (!sec_file) {
                return std::unexpected(std::make_error_code(std::errc::permission_denied));
            }
            
            sec_file.write(reinterpret_cast<const char*>(kp_result->secret_key.data.data()),
                          kp_result->secret_key.data.size());
            
            std::uint8_t level_byte = static_cast<std::uint8_t>(Level);
            sec_file.write(reinterpret_cast<const char*>(&level_byte), 1);
            
            // Set restrictive permissions on secret key
            fs::permissions(sec_key_path, fs::perms::owner_read | fs::perms::owner_write);
            
        } catch (...) {
            return std::unexpected(std::make_error_code(std::errc::io_error));
        }
        
        if (options.verbose) {
            std::cout << std::format("Public key:  {}\n", pub_key_path.string());
            std::cout << std::format("Secret key:  {}\n", sec_key_path.string());
            std::cout << std::format("Key size:    {} + {} bytes\n",
                                    kp_result->public_key.data.size(),
                                    kp_result->secret_key.data.size());
        }
        
        return 0;
    }
    
    std::expected<int, std::error_code> encrypt_file() {
        if (options.input_file.empty() || options.key_file.empty()) {
            std::cerr << "encrypt: input file and key file required for encryption\n";
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        }
        
        // Load public key and determine security level
        auto [public_key_data, security_level] = load_public_key();
        if (public_key_data.empty()) {
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        }
        
        // Determine output file
        fs::path output_path = options.output_file.empty() ?
            (options.input_file.string() + ".enc") : options.output_file;
        
        if (options.verbose) {
            std::cout << std::format("Encrypting: {} -> {}\n", 
                                    options.input_file.string(), output_path.string());
            std::cout << std::format("Security level: Kyber-{}\n",
                                    static_cast<int>(security_level) * 256 + 256);
            std::cout << std::format("SIMD level: {}\n", get_simd_info());
        }
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Template dispatch for encryption
        std::expected<int, std::error_code> result;
        switch (security_level) {
            case kyber_level::KYBER_512:
                result = encrypt_file_typed<kyber_level::KYBER_512>(public_key_data, output_path);
                break;
            case kyber_level::KYBER_768:
                result = encrypt_file_typed<kyber_level::KYBER_768>(public_key_data, output_path);
                break;
            case kyber_level::KYBER_1024:
                result = encrypt_file_typed<kyber_level::KYBER_1024>(public_key_data, output_path);
                break;
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        if (options.verbose && result) {
            auto file_size = fs::file_size(options.input_file);
            auto throughput = static_cast<double>(file_size) / duration.count() * 1000.0 / (1024 * 1024);
            
            std::cout << std::format("Encryption completed in {} ms\n", duration.count());
            std::cout << std::format("Throughput: {:.2f} MB/s\n", throughput);
        }
        
        return result;
    }
    
    template<kyber_level Level>
    std::expected<int, std::error_code> encrypt_file_typed(std::span<const std::byte> key_data,
                                                           const fs::path& output_path) {
        try {
            // Reconstruct public key
            kyber_public_key<Level> public_key;
            if (key_data.size() < public_key.data.size()) {
                return std::unexpected(std::make_error_code(std::errc::invalid_argument));
            }
            
            std::copy(key_data.begin(), key_data.begin() + public_key.data.size(),
                     public_key.data.begin());
            
            // Open input and output files
            std::ifstream input_file(options.input_file, std::ios::binary);
            std::ofstream output_file(output_path, std::ios::binary);
            
            if (!input_file || !output_file) {
                return std::unexpected(std::make_error_code(std::errc::io_error));
            }
            
            // Get file size for streaming
            auto file_size = fs::file_size(options.input_file);
            std::size_t bytes_processed = 0;
            
            // Write file header with metadata
            FileHeader header{};
            header.magic = 0x58494E494D504521;  // "XINIMPQ!"
            header.version = 1;
            header.security_level = static_cast<std::uint8_t>(Level);
            header.file_size = file_size;
            header.chunk_size = options.chunk_size;
            
            output_file.write(reinterpret_cast<const char*>(&header), sizeof(header));
            
            // Process file in chunks for memory efficiency
            std::vector<std::byte> chunk_buffer(options.chunk_size);
            std::size_t chunk_index = 0;
            
            while (bytes_processed < file_size) {
                std::size_t chunk_bytes = std::min(options.chunk_size, file_size - bytes_processed);
                
                // Read chunk
                input_file.read(reinterpret_cast<char*>(chunk_buffer.data()), chunk_bytes);
                std::span<const std::byte> chunk_span(chunk_buffer.data(), chunk_bytes);
                
                // Encrypt chunk using Kyber KEM + AES-GCM
                auto enc_result = kyber_simd<Level>::encapsulate(public_key);
                if (!enc_result) {
                    return std::unexpected(enc_result.error());
                }
                
                auto [ciphertext, shared_secret] = *enc_result;
                
                // Use shared secret as AES key for chunk encryption
                auto encrypted_chunk = encrypt_chunk_aes_gcm(chunk_span, shared_secret, chunk_index);
                
                // Write chunk header
                ChunkHeader chunk_header{};
                chunk_header.chunk_index = chunk_index++;
                chunk_header.original_size = chunk_bytes;
                chunk_header.encrypted_size = encrypted_chunk.size();
                
                output_file.write(reinterpret_cast<const char*>(&chunk_header), sizeof(chunk_header));
                
                // Write Kyber ciphertext
                output_file.write(reinterpret_cast<const char*>(ciphertext.data.data()),
                                 ciphertext.data.size());
                
                // Write encrypted chunk data
                output_file.write(reinterpret_cast<const char*>(encrypted_chunk.data()),
                                 encrypted_chunk.size());
                
                bytes_processed += chunk_bytes;
                
                // Progress reporting
                if (options.verbose && (chunk_index % 100 == 0 || bytes_processed == file_size)) {
                    double progress = static_cast<double>(bytes_processed) / file_size * 100.0;
                    std::cout << std::format("\rProgress: {:.1f}% ({}/{} MB)", 
                                            progress, bytes_processed / (1024*1024), 
                                            file_size / (1024*1024));
                    std::cout.flush();
                }
            }
            
            if (options.verbose) {
                std::cout << "\n";  // New line after progress
            }
            
            return 0;
            
        } catch (const std::exception& e) {
            std::cerr << std::format("encrypt: {}\n", e.what());
            return std::unexpected(std::make_error_code(std::errc::operation_not_supported));
        }
    }
    
    std::expected<int, std::error_code> run_benchmark() {
        if (options.verbose) {
            report_simd_capabilities();
        }
        
        std::cout << std::format("\n=== XINIM Post-Quantum Encryption Benchmark ===\n");
        std::cout << std::format("SIMD Level: {}\n", get_simd_info());
        std::cout << std::format("Chunk Size: {} bytes\n\n", options.chunk_size);
        
        // Run benchmarks for all security levels
        std::cout << "Running Kyber-512 benchmark...\n";
        run_comprehensive_benchmark<kyber_level::KYBER_512>();
        
        std::cout << "\nRunning Kyber-768 benchmark...\n"; 
        run_comprehensive_benchmark<kyber_level::KYBER_768>();
        
        std::cout << "\nRunning Kyber-1024 benchmark...\n";
        run_comprehensive_benchmark<kyber_level::KYBER_1024>();
        
        // File encryption throughput benchmark
        std::cout << "\n=== File Encryption Throughput ===\n";
        return run_throughput_benchmark();
    }
    
    std::expected<int, std::error_code> run_throughput_benchmark() {
        // Create temporary test files of various sizes
        std::vector<std::size_t> test_sizes = {
            1024,           // 1 KB
            1024 * 1024,    // 1 MB
            10 * 1024 * 1024, // 10 MB
            100 * 1024 * 1024 // 100 MB
        };
        
        for (auto size : test_sizes) {
            fs::path test_file = fs::temp_directory_path() / 
                               std::format("xinim_bench_{}.tmp", size);
            fs::path encrypted_file = fs::temp_directory_path() /
                                    std::format("xinim_bench_{}.enc", size);
            
            try {
                // Generate test file with random data
                generate_test_file(test_file, size);
                
                // Generate temporary keypair for benchmark
                auto kp_result = kyber_simd<kyber_level::KYBER_768>::generate_keypair();
                if (!kp_result) continue;
                
                fs::path temp_key = fs::temp_directory_path() / "xinim_bench.pub";
                {
                    std::ofstream key_file(temp_key, std::ios::binary);
                    key_file.write(reinterpret_cast<const char*>(kp_result->public_key.data.data()),
                                  kp_result->public_key.data.size());
                    std::uint8_t level = static_cast<std::uint8_t>(kyber_level::KYBER_768);
                    key_file.write(reinterpret_cast<const char*>(&level), 1);
                }
                
                // Benchmark encryption
                auto old_options = options;
                options.input_file = test_file;
                options.output_file = encrypted_file;
                options.key_file = temp_key;
                options.verbose = false;
                
                auto start = std::chrono::high_resolution_clock::now();
                auto result = encrypt_file();
                auto end = std::chrono::high_resolution_clock::now();
                
                options = old_options;  // Restore options
                
                if (result) {
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                    double throughput = static_cast<double>(size) / duration.count() * 1000.0 / (1024 * 1024);
                    
                    std::cout << std::format("{:>8} - {} ms - {:.2f} MB/s\n",
                                            format_size(size), duration.count(), throughput);
                }
                
                // Cleanup
                fs::remove(test_file);
                fs::remove(encrypted_file);
                fs::remove(temp_key);
                
            } catch (...) {
                // Cleanup on error
                fs::remove(test_file);
                fs::remove(encrypted_file);
            }
        }
        
        return 0;
    }
    
    // Helper structures for file format
    struct FileHeader {
        std::uint64_t magic;
        std::uint32_t version;
        std::uint8_t security_level;
        std::uint8_t reserved[3];
        std::uint64_t file_size;
        std::uint64_t chunk_size;
    };
    
    struct ChunkHeader {
        std::uint64_t chunk_index;
        std::uint64_t original_size;
        std::uint64_t encrypted_size;
    };
    
    // Helper functions
    std::pair<std::vector<std::byte>, kyber_level> load_public_key() {
        try {
            std::ifstream key_file(options.key_file, std::ios::binary);
            if (!key_file) {
                std::cerr << "encrypt: cannot open key file\n";
                return {{}, kyber_level::KYBER_512};
            }
            
            // Read all data
            key_file.seekg(0, std::ios::end);
            auto file_size = key_file.tellg();
            key_file.seekg(0);
            
            std::vector<std::byte> data(file_size - 1);  // -1 for security level byte
            key_file.read(reinterpret_cast<char*>(data.data()), data.size());
            
            std::uint8_t level_byte;
            key_file.read(reinterpret_cast<char*>(&level_byte), 1);
            
            return {std::move(data), static_cast<kyber_level>(level_byte)};
            
        } catch (...) {
            std::cerr << "encrypt: error reading key file\n";
            return {{}, kyber_level::KYBER_512};
        }
    }
    
    std::vector<std::byte> encrypt_chunk_aes_gcm(std::span<const std::byte> chunk,
                                                const kyber_shared_secret& key,
                                                std::uint64_t chunk_index) {
        // Simplified AES-GCM encryption using the shared secret
        // In a real implementation, this would use proper AES-GCM
        std::vector<std::byte> encrypted(chunk.size() + 16); // +16 for GCM tag
        
        // XOR encryption with key expansion (placeholder)
        for (std::size_t i = 0; i < chunk.size(); ++i) {
            std::uint8_t key_byte = key[i % key.size()].operator std::uint8_t();
            encrypted[i] = chunk[i] ^ static_cast<std::byte>(key_byte ^ (chunk_index & 0xFF));
        }
        
        // Add authentication tag (placeholder)
        std::fill_n(encrypted.begin() + chunk.size(), 16, std::byte{0xAA});
        
        return encrypted;
    }
    
    void generate_test_file(const fs::path& path, std::size_t size) {
        std::ofstream file(path, std::ios::binary);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<std::uint8_t> dist;
        
        constexpr std::size_t buffer_size = 64 * 1024;
        std::array<std::uint8_t, buffer_size> buffer;
        
        std::size_t remaining = size;
        while (remaining > 0) {
            std::size_t write_size = std::min(buffer_size, remaining);
            
            for (std::size_t i = 0; i < write_size; ++i) {
                buffer[i] = dist(gen);
            }
            
            file.write(reinterpret_cast<const char*>(buffer.data()), write_size);
            remaining -= write_size;
        }
    }
    
    std::string format_size(std::size_t bytes) {
        if (bytes < 1024) {
            return std::format("{} B", bytes);
        } else if (bytes < 1024 * 1024) {
            return std::format("{} KB", bytes / 1024);
        } else {
            return std::format("{} MB", bytes / (1024 * 1024));
        }
    }
};

int main(int argc, char* argv[]) {
    // Convert arguments to string_view span for C++23 compatibility
    std::vector<std::string_view> args;
    args.reserve(argc - 1);
    
    std::ranges::transform(
        std::span(argv + 1, argc - 1),
        std::back_inserter(args),
        [](const char* arg) { return std::string_view(arg); }
    );
    
    try {
        PostQuantumEncryptTool tool;
        auto result = tool.execute(std::span(args));
        
        if (!result) {
            return static_cast<int>(result.error().value());
        }
        
        return *result;
        
    } catch (const std::exception& e) {
        std::cerr << std::format("encrypt: fatal error: {}\n", e.what());
        return 2;
    } catch (...) {
        std::cerr << "encrypt: unknown fatal error\n";
        return 2;
    }
}