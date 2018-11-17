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

  // load binary rpc response to struct
  cryptonote::COMMAND_RPC_GET_BLOCKS_BY_HEIGHT::response resp_struct;
  epee::serialization::load_t_from_binary(resp_struct, buff_bin);

  // build property tree from deserialized blocks and transactions
  boost::property_tree::ptree root;
  boost::property_tree::ptree blocksNode;	// array of block strings
  boost::property_tree::ptree txsNodes;		// array of txs per block (array of array)
  for (int blockIdx = 0; blockIdx < resp_struct.blocks.size(); blockIdx++) {

      // parse and validate block
      cryptonote::block block;
      if (cryptonote::parse_and_validate_block_from_blob(resp_struct.blocks[blockIdx].block, block)) {

	  // add block node to blocks node
	  boost::property_tree::ptree blockNode;
	  blockNode.put("", cryptonote::obj_to_json_str(block));	// TODO: no pretty print
	  blocksNode.push_back(std::make_pair("", blockNode));
      } else {
	  throw std::runtime_error("failed to parse block blob at index " + std::to_string(blockIdx));
      }

      // parse and validate txs
      boost::property_tree::ptree txsNode;
      for (int txIdx = 0; txIdx < resp_struct.blocks[blockIdx].txs.size(); txIdx++) {
	  cryptonote::transaction tx;
	  if (cryptonote::parse_and_validate_tx_from_blob(resp_struct.blocks[blockIdx].txs[txIdx], tx)) {

	      // add tx node to txs node
	      boost::property_tree::ptree txNode;
	      txNode.put("", cryptonote::obj_to_json_str(tx));	// TODO: no pretty print
	      txsNode.push_back(std::make_pair("", txNode));
	  } else {
	      throw std::runtime_error("failed to parse tx blob at index " + std::to_string(txIdx));
	  }
      }
      txsNodes.push_back(std::make_pair("", txsNode));	// array of array of transactions, one array per block
  }
  root.add_child("blocks", blocksNode);
  root.add_child("txs", txsNodes);
  root.put("status", resp_struct.status);
  root.put("untrusted", resp_struct.untrusted);	// TODO: loss of ints and bools

  // convert root to string // TODO: common utility with serial_bridge
  std::stringstream ss;
  boost::property_tree::write_json(ss, root, false/*pretty*/);
  buff_json = ss.str();
}
