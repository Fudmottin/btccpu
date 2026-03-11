// src/mining_job/coinbase.cpp

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iosfwd>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "mining_job/coinbase.hpp"
#include "util/hex.hpp"

namespace cpu_miner {
namespace {

class ByteReader {
 public:
   explicit ByteReader(const std::vector<std::uint8_t>& bytes)
      : bytes_(bytes) {}

   [[nodiscard]] std::size_t remaining() const noexcept {
      return bytes_.size() - pos_;
   }

   [[nodiscard]] std::size_t position() const noexcept { return pos_; }

   std::uint8_t read_u8() {
      require(1U, "unexpected end of transaction while reading byte");
      return bytes_[pos_++];
   }

   std::uint32_t read_u32_le() {
      require(4U, "unexpected end of transaction while reading u32");
      const std::uint32_t b0 = bytes_[pos_ + 0];
      const std::uint32_t b1 = bytes_[pos_ + 1];
      const std::uint32_t b2 = bytes_[pos_ + 2];
      const std::uint32_t b3 = bytes_[pos_ + 3];
      pos_ += 4U;
      return b0 | (b1 << 8U) | (b2 << 16U) | (b3 << 24U);
   }

   std::uint64_t read_u64_le() {
      require(8U, "unexpected end of transaction while reading u64");
      std::uint64_t value = 0;
      for (unsigned i = 0; i < 8U; ++i) {
         value |= (static_cast<std::uint64_t>(bytes_[pos_ + i]) << (8U * i));
      }
      pos_ += 8U;
      return value;
   }

   std::uint64_t read_compact_size() {
      const std::uint8_t first = read_u8();
      if (first < 0xfdU) return first;
      if (first == 0xfdU) {
         require(2U, "unexpected end of transaction while reading compactsize");
         const std::uint64_t b0 = bytes_[pos_ + 0];
         const std::uint64_t b1 = bytes_[pos_ + 1];
         pos_ += 2U;
         return b0 | (b1 << 8U);
      }
      if (first == 0xfeU) {
         return read_u32_le();
      }
      return read_u64_le();
   }

   std::vector<std::uint8_t> read_bytes(std::size_t count) {
      require(count, "unexpected end of transaction while reading bytes");
      std::vector<std::uint8_t> out(
         bytes_.begin() + static_cast<std::ptrdiff_t>(pos_),
         bytes_.begin() + static_cast<std::ptrdiff_t>(pos_ + count));
      pos_ += count;
      return out;
   }

 private:
   void require(std::size_t count, std::string_view message) const {
      if (remaining() < count) {
         throw std::runtime_error(std::string(message));
      }
   }

   const std::vector<std::uint8_t>& bytes_;
   std::size_t pos_{};
};

std::string bytes_to_hex_string(const std::vector<std::uint8_t>& bytes) {
   return bytes_to_hex(bytes);
}

std::string sats_to_btc_string(std::uint64_t sats) {
   const std::uint64_t whole = sats / 100000000ULL;
   const std::uint64_t frac = sats % 100000000ULL;

   std::ostringstream os;
   os << whole << '.' << std::setw(8) << std::setfill('0') << frac;
   return os.str();
}

std::string describe_script_pubkey(const std::vector<std::uint8_t>& script) {
   if (script.size() == 22U && script[0] == 0x00U && script[1] == 0x14U) {
      return "P2WPKH";
   }

   if (script.size() == 34U && script[0] == 0x00U && script[1] == 0x20U) {
      return "P2WSH";
   }

   if (script.size() == 25U && script[0] == 0x76U && script[1] == 0xa9U &&
       script[2] == 0x14U && script[23] == 0x88U && script[24] == 0xacU) {
      return "P2PKH";
   }

   if (!script.empty() && script[0] == 0x6aU) {
      return "OP_RETURN";
   }

   return "unknown";
}

std::string describe_prevout(const std::vector<std::uint8_t>& prevout_hash,
                             std::uint32_t prevout_index) {
   const bool zero_hash = std::all_of(prevout_hash.begin(), prevout_hash.end(),
                                      [](std::uint8_t b) { return b == 0U; });

   if (zero_hash && prevout_index == 0xffffffffU) {
      return "coinbase prevout";
   }

   return "non-coinbase prevout";
}

bool is_printable_ascii(std::uint8_t c) { return c >= 32U && c <= 126U; }

constexpr char kBech32Charset[] = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";

std::uint32_t bech32_polymod(const std::vector<std::uint8_t>& values) {
   std::uint32_t chk = 1U;
   constexpr std::array<std::uint32_t, 5> gen{0x3b6a57b2U, 0x26508e6dU,
                                              0x1ea119faU, 0x3d4233ddU,
                                              0x2a1462b3U};

   for (std::uint8_t v : values) {
      const std::uint32_t top = chk >> 25U;
      chk = ((chk & 0x1ffffffU) << 5U) ^ v;
      for (int i = 0; i < 5; ++i) {
         if (((top >> i) & 1U) != 0U) {
            chk ^= gen[static_cast<std::size_t>(i)];
         }
      }
   }

   return chk;
}

std::vector<std::uint8_t> bech32_hrp_expand(std::string_view hrp) {
   std::vector<std::uint8_t> out;
   out.reserve(hrp.size() * 2U + 1U);

   for (char c : hrp) {
      out.push_back(
         static_cast<std::uint8_t>(static_cast<unsigned char>(c) >> 5U));
   }
   out.push_back(0U);
   for (char c : hrp) {
      out.push_back(
         static_cast<std::uint8_t>(static_cast<unsigned char>(c) & 31U));
   }

   return out;
}

std::vector<std::uint8_t>
convert_bits_8_to_5(std::span<const std::uint8_t> bytes) {
   std::vector<std::uint8_t> out;
   int acc = 0;
   int bits = 0;

   for (std::uint8_t b : bytes) {
      acc = (acc << 8) | b;
      bits += 8;
      while (bits >= 5) {
         bits -= 5;
         out.push_back(static_cast<std::uint8_t>((acc >> bits) & 31));
      }
   }

   if (bits > 0) {
      out.push_back(static_cast<std::uint8_t>((acc << (5 - bits)) & 31));
   }

   return out;
}

std::string bech32_encode(std::string_view hrp,
                          const std::vector<std::uint8_t>& data) {
   std::vector<std::uint8_t> values = bech32_hrp_expand(hrp);
   values.insert(values.end(), data.begin(), data.end());
   values.insert(values.end(), 6U, 0U);

   const std::uint32_t polymod = bech32_polymod(values) ^ 1U;

   std::vector<std::uint8_t> checksum(6U);
   for (int i = 0; i < 6; ++i) {
      checksum[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(
         (polymod >> (5U * (5U - static_cast<unsigned>(i)))) & 31U);
   }

   std::string out(hrp);
   out.push_back('1');

   for (std::uint8_t v : data) {
      out.push_back(kBech32Charset[v]);
   }
   for (std::uint8_t v : checksum) {
      out.push_back(kBech32Charset[v]);
   }

   return out;
}

std::vector<std::string>
extract_ascii_runs(const std::vector<std::uint8_t>& bytes,
                   std::size_t min_run = 4U) {
   std::vector<std::string> runs;
   std::size_t i = 0;

   while (i < bytes.size()) {
      while (i < bytes.size() && !is_printable_ascii(bytes[i])) {
         ++i;
      }

      const std::size_t start = i;

      while (i < bytes.size() && is_printable_ascii(bytes[i])) {
         ++i;
      }

      const std::size_t len = i - start;
      if (len >= min_run) {
         runs.emplace_back(bytes.begin() + static_cast<std::ptrdiff_t>(start),
                           bytes.begin() + static_cast<std::ptrdiff_t>(i));
      }
   }

   return runs;
}

} // namespace

std::string extranonce2_from_counter(std::uint64_t value,
                                     std::size_t size_bytes) {
   return hex_from_u64_be(value, size_bytes);
}

std::string make_coinbase_hex(const MiningJob& job,
                              const SubscriptionContext& sub,
                              const std::string& extranonce2_hex) {
   if (!is_lower_hex_string(job.coinb1)) {
      throw std::invalid_argument("coinb1 is not valid lowercase hex");
   }
   if (!is_lower_hex_string(job.coinb2)) {
      throw std::invalid_argument("coinb2 is not valid lowercase hex");
   }
   if (!is_lower_hex_string(sub.extranonce1)) {
      throw std::invalid_argument("extranonce1 is not valid lowercase hex");
   }
   if (!is_lower_hex_string(extranonce2_hex)) {
      throw std::invalid_argument("extranonce2 is not valid lowercase hex");
   }

   const std::size_t expected_extranonce2_hex_chars = sub.extranonce2_size * 2U;
   if (extranonce2_hex.size() != expected_extranonce2_hex_chars) {
      throw std::invalid_argument("extranonce2 has wrong hex length");
   }

   std::string coinbase;
   coinbase.reserve(job.coinb1.size() + sub.extranonce1.size() +
                    extranonce2_hex.size() + job.coinb2.size());

   coinbase += job.coinb1;
   coinbase += sub.extranonce1;
   coinbase += extranonce2_hex;
   coinbase += job.coinb2;

   return coinbase;
}

CoinbaseBuild build_coinbase(const MiningJob& job,
                             const SubscriptionContext& sub,
                             const std::string& extranonce2_hex) {
   CoinbaseBuild out;
   out.extranonce2_hex = extranonce2_hex;
   out.coinbase_hex = make_coinbase_hex(job, sub, extranonce2_hex);
   out.coinbase_bytes = hex_to_bytes(out.coinbase_hex);

   const auto hash_words = sha256::dbl_sha256_words(out.coinbase_bytes);
   out.coinbase_hash = sha256::digest_words_to_bytes_be(hash_words);

   return out;
}

ScriptSigSummary
summarize_script_sig(const std::vector<std::uint8_t>& script_sig) {
   ScriptSigSummary out;

   if (!script_sig.empty()) {
      const std::uint8_t first = script_sig[0];
      if (first >= 1U && first <= 4U && script_sig.size() >= 1U + first) {
         out.has_block_height = true;
         out.block_height = 0U;
         for (std::size_t i = 0; i < first; ++i) {
            out.block_height |= static_cast<std::uint32_t>(script_sig[1U + i])
                                << (8U * i);
         }
      }
   }

   out.ascii_runs = extract_ascii_runs(script_sig);
   return out;
}

std::string
decode_p2wpkh_address(const std::vector<std::uint8_t>& script_pubkey) {
   if (script_pubkey.size() != 22U || script_pubkey[0] != 0x00U ||
       script_pubkey[1] != 0x14U) {
      return {};
   }

   std::vector<std::uint8_t> witness_program(script_pubkey.begin() + 2,
                                             script_pubkey.end());

   std::vector<std::uint8_t> data;
   data.push_back(0U);
   const auto prog5 = convert_bits_8_to_5(witness_program);
   data.insert(data.end(), prog5.begin(), prog5.end());

   return bech32_encode("bc", data);
}

std::string
decode_p2wsh_address(const std::vector<std::uint8_t>& script_pubkey) {
   if (script_pubkey.size() != 34U || script_pubkey[0] != 0x00U ||
       script_pubkey[1] != 0x20U) {
      return {};
   }

   std::vector<std::uint8_t> witness_program(script_pubkey.begin() + 2,
                                             script_pubkey.end());

   std::vector<std::uint8_t> data;
   data.push_back(0U);
   const auto prog5 = convert_bits_8_to_5(witness_program);
   data.insert(data.end(), prog5.begin(), prog5.end());

   return bech32_encode("bc", data);
}

CoinbaseDecoded decode_coinbase(const std::vector<std::uint8_t>& tx_bytes) {
   ByteReader r(tx_bytes);
   CoinbaseDecoded out;

   out.version = r.read_u32_le();

   if (r.remaining() >= 2U && tx_bytes[r.position()] == 0x00U &&
       tx_bytes[r.position() + 1U] == 0x01U) {
      out.has_witness_marker = true;
      (void)r.read_u8();
      (void)r.read_u8();
   }

   const std::uint64_t input_count = r.read_compact_size();
   if (input_count != 1U) {
      throw std::runtime_error("expected exactly one coinbase input");
   }

   out.prevout_hash = r.read_bytes(32U);
   out.prevout_index = r.read_u32_le();

   const std::uint64_t script_sig_size = r.read_compact_size();
   out.script_sig = r.read_bytes(static_cast<std::size_t>(script_sig_size));
   out.sequence = r.read_u32_le();

   const std::uint64_t output_count = r.read_compact_size();
   out.outputs.reserve(static_cast<std::size_t>(output_count));

   for (std::uint64_t i = 0; i < output_count; ++i) {
      CoinbaseOutputDecoded output;
      output.value_sats = r.read_u64_le();

      const std::uint64_t script_size = r.read_compact_size();
      output.script_pubkey =
         r.read_bytes(static_cast<std::size_t>(script_size));
      output.script_type = describe_script_pubkey(output.script_pubkey);

      if (output.script_type == "P2WPKH") {
         output.address = decode_p2wpkh_address(output.script_pubkey);
      } else if (output.script_type == "P2WSH") {
         output.address = decode_p2wsh_address(output.script_pubkey);
      }

      out.outputs.push_back(std::move(output));
   }

   if (out.has_witness_marker) {
      for (std::size_t input_index = 0; input_index < 1U; ++input_index) {
         const std::uint64_t item_count = r.read_compact_size();
         for (std::uint64_t item = 0; item < item_count; ++item) {
            const std::uint64_t item_size = r.read_compact_size();
            (void)r.read_bytes(static_cast<std::size_t>(item_size));
         }
      }
   }

   out.locktime = r.read_u32_le();

   if (r.remaining() != 0U) {
      throw std::runtime_error("extra trailing bytes after coinbase decode");
   }

   return out;
}

void print_coinbase_decoded(const CoinbaseDecoded& decoded, std::ostream& os) {
   os << "  decoded coinbase:\n";
   os << "    version: " << decoded.version << '\n';
   os << "    witness marker/flag: "
      << (decoded.has_witness_marker ? "present" : "absent") << '\n';
   os << "    prevout: "
      << describe_prevout(decoded.prevout_hash, decoded.prevout_index) << '\n';
   os << "    prevout hash: " << bytes_to_hex_string(decoded.prevout_hash)
      << '\n';
   os << "    prevout index: " << decoded.prevout_index << '\n';
   os << "    scriptSig bytes: " << decoded.script_sig.size() << '\n';
   os << "    scriptSig hex: " << bytes_to_hex_string(decoded.script_sig)
      << '\n';

   const auto summary = summarize_script_sig(decoded.script_sig);

   if (summary.has_block_height) {
      os << "    block height: " << summary.block_height << '\n';
   }

   os << "    scriptSig ascii runs: " << summary.ascii_runs.size() << '\n';
   for (std::size_t i = 0; i < summary.ascii_runs.size(); ++i) {
      os << "      ascii[" << i << "]: " << summary.ascii_runs[i] << '\n';
   }

   os << "    sequence: 0x" << std::hex << std::setw(8) << std::setfill('0')
      << decoded.sequence << std::dec << std::setfill(' ') << '\n';
   os << "    outputs: " << decoded.outputs.size() << '\n';

   for (std::size_t i = 0; i < decoded.outputs.size(); ++i) {
      const auto& output = decoded.outputs[i];
      os << "      [" << i << "] value_sats: " << output.value_sats << '\n';
      os << "      [" << i
         << "] value_btc: " << sats_to_btc_string(output.value_sats) << '\n';
      os << "      [" << i << "] script type: " << output.script_type << '\n';
      os << "      [" << i
         << "] script hex: " << bytes_to_hex_string(output.script_pubkey)
         << '\n';

      if (!output.address.empty()) {
         os << "      [" << i << "] address: " << output.address << '\n';
      }
   }

   os << "    locktime: " << decoded.locktime << '\n';
}

} // namespace cpu_miner

