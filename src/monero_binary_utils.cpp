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

  std::cout << "Converted binary to struct, status:\n" << resp_struct.status << "\n";
  std::cout << "Number of blocks:\n" << resp_struct.blocks.size () << "\n";

  // collect blocks and txs
  std::vector < cryptonote::block > blocks;
  for (int blockIdx = 0; blockIdx < resp_struct.blocks.size (); blockIdx++) {

    // parse and validate block
    cryptonote::block block;
    if (cryptonote::parse_and_validate_block_from_blob(resp_struct.blocks[blockIdx].block, block)) {
      blocks.push_back (block);
      std::cout << "Serialized block: " << cryptonote::obj_to_json_str (block) << "\n";
    } else {
      throw std::runtime_error("failed to parse block blob at index " + std::to_string (blockIdx));
    }

    // parse and validate txs
    for (int txIdx = 0; txIdx < block.tx_hashes.size (); txIdx++) {
      cryptonote::transaction tx;
      if (cryptonote::parse_and_validate_tx_from_blob(resp_struct.blocks[blockIdx].txs[txIdx], tx)) {
	std::cout << "Serialized tx: " << cryptonote::obj_to_json_str (tx) << "\n";
      } else {
	throw std::runtime_error("failed to parse tx blob at index " + std::to_string (txIdx));
      }
    }
  }

  // build property tree with response values
  boost::property_tree::ptree root;
  root.put ("blocks", std::string ("hi there!"));

  // convert root to string // TODO: common utility with serial_bridge
  std::stringstream ss;
  boost::property_tree::write_json (ss, root, false/*pretty*/);
  buff_json = ss.str ();
  std::cout << buff_json << "\n";
}
