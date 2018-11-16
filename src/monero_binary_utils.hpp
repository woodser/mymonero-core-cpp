#ifndef monero_binary_utils_hpp
#define monero_binary_utils_hpp

#include "cryptonote_basic.h"
#include "serialization/keyvalue_serialization.h"	// TODO: consolidate with other binary deps?
#include "storages/portable_storage.h"

/**
 * Collection of utilities for working with Monero's binary portable storage format.
 */
namespace binary_utils
{
  using namespace std;
  using namespace boost;
  using namespace cryptonote;

  /**
   * TODO.
   */
  void json_to_binary(const std::string &buff_json, std::string &buff_bin);

  /**
   * TODO.
   */
  void binary_to_json(const std::string &buff_bin, std::string &buff_json);

  /**
   * TODO.
   */
  void binary_blocks_to_json(const std::string &buff_bin, std::string &buff_json);
}
#endif /* monero_binary_utils_hpp */
