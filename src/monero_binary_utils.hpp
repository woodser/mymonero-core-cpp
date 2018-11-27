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

  /**
   * Modified from core_rpc_server.cpp to return a string.
   */
  static std::string get_pruned_tx_json(cryptonote::transaction &tx)
  {
    std::stringstream ss;
    json_archive<true> ar(ss);
    bool r = tx.serialize_base(ar);
    CHECK_AND_ASSERT_MES(r, std::string(), "Failed to serialize rct signatures base");
    return ss.str();
  }
}
#endif /* monero_binary_utils_hpp */
