#include "netassets.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/uuid/detail/sha1.hpp>

std::string NetSha1(const std::string &data) {
  boost::uuids::detail::sha1 sha;
  sha.process_bytes(data.data(), data.size());
  boost::uuids::detail::sha1::digest_type digest;
  sha.get_digest(digest);

  std::ostringstream oss;
  for (int i = 0; i < 20; i++) {
    oss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
  }
  return oss.str();
}

std::string NetGetBuildHash() {
#ifdef GAMEPLAYFOOTBALL_BUILD_HASH
  return GAMEPLAYFOOTBALL_BUILD_HASH;
#else
  return "unknown";
#endif
}

static std::string JsonStringField(const std::string &json, const std::string &key) {
  std::string needle = "\"" + key + "\"";
  size_t keyPos = json.find(needle);
  if (keyPos == std::string::npos) return "";
  size_t colonPos = json.find(':', keyPos + needle.size());
  if (colonPos == std::string::npos) return "";
  size_t firstQuote = json.find('"', colonPos + 1);
  if (firstQuote == std::string::npos) return "";
  size_t secondQuote = json.find('"', firstQuote + 1);
  if (secondQuote == std::string::npos) return "";
  return json.substr(firstQuote + 1, secondQuote - firstQuote - 1);
}

static std::string ReadManifestField(const std::string &key) {
  std::ifstream manifestFile("databases/default/manifest.json");
  if (!manifestFile.is_open()) return "";
  std::stringstream manifestStream;
  manifestStream << manifestFile.rdbuf();
  return JsonStringField(manifestStream.str(), key);
}

std::string NetGetDataVersion() {
  return ReadManifestField("data_version");
}

std::string NetGetDataHash() {
  return ReadManifestField("sha256");
}

std::string NetGetAnimationHash() {
  std::string listing;
  boost::system::error_code ec;
  boost::filesystem::path root("media/animations");

  if (boost::filesystem::exists(root, ec)) {
    std::vector<std::string> names;
    for (boost::filesystem::recursive_directory_iterator it(root, ec), end; it != end; it.increment(ec)) {
      if (ec) break;
      if (boost::filesystem::is_regular_file(it->path(), ec)) {
        names.push_back(it->path().generic_string());
      }
    }
    std::sort(names.begin(), names.end());
    for (unsigned int i = 0; i < names.size(); i++) {
      listing += names.at(i);
      listing += '\n';
    }
  }

  return NetSha1(listing);
}
