#ifndef _HPP_NETASSETS
#define _HPP_NETASSETS

#include <string>

std::string NetSha1(const std::string &data);
std::string NetGetBuildHash();
std::string NetGetDataVersion();
std::string NetGetDataHash();
std::string NetGetAnimationHash();

#endif
