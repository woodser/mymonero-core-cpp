#include "monero_binary_utils.hpp"
#include "rpc/core_rpc_server_commands_defs.h"
#include "storages/portable_storage_template_helper.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

using namespace std;
using namespace boost;
using namespace cryptonote;
using namespace binary_utils;

void binary_utils::json_to_binary(const std::string &buff_json, std::string &buff_bin) {
  epee::serialization::portable_storage ps;
  ps.load_from_json(buff_json);
  ps.store_to_binary(buff_bin);
}

void binary_utils::binary_to_json(const std::string &buff_bin, std::string &buff_json) {
  epee::serialization::portable_storage ps;
  ps.load_from_binary(buff_bin);
  ps.dump_as_json(buff_json);
}

void binary_utils::binary_blocks_to_json(const std::string &buff_bin, std::string &buff_json) {

  // convert binary to struct with binary blocks and transactions
  cryptonote::COMMAND_RPC_GET_BLOCKS_BY_HEIGHT::response bin_blocks;
  epee::serialization::load_t_from_binary(bin_blocks, buff_bin);

  // build response by parsing and validating binary blocks and transactions
  binary_utils::blocks_resp resp;
  for (int blockIdx = 0; blockIdx < bin_blocks.blocks.size(); blockIdx++) {

      // parse and validate block
      cryptonote::block block;
      if (cryptonote::parse_and_validate_block_from_blob(bin_blocks.blocks[blockIdx].block, block)) {
	  resp.blocks.push_back(block);
      } else {
	throw std::runtime_error("failed to parse block blob at index " + std::to_string(blockIdx));
      }

      // parse and validate txs
      resp.txs.push_back(std::vector<cryptonote::transaction>());
      for (int txIdx = 0; txIdx < bin_blocks.blocks[blockIdx].txs.size(); txIdx++) {
	cryptonote::transaction tx;
	if (cryptonote::parse_and_validate_tx_from_blob(bin_blocks.blocks[blockIdx].txs[txIdx], tx)) {
	    resp.txs[blockIdx].push_back(tx);
	} else {
	    throw std::runtime_error("failed to parse tx blob at index " + std::to_string(txIdx));
	}
      }
  }

  // convert response to json
  buff_json = cryptonote::obj_to_json_str(resp);
}
