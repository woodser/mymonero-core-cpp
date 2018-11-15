//
//  serial_bridge_index.cpp
//  Copyright (c) 2014-2018, MyMonero.com
//
//  All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without modification, are
//  permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice, this list of
//	conditions and the following disclaimer.
//
//  2. Redistributions in binary form must reproduce the above copyright notice, this list
//	of conditions and the following disclaimer in the documentation and/or other
//	materials provided with the distribution.
//
//  3. Neither the name of the copyright holder nor the names of its contributors may be
//	used to endorse or promote products derived from this software without specific
//	prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
//  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
//  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
//  THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
//  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
//  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
//  THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//
//
#include "serial_bridge_index.hpp"
//
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
//
#include "monero_fork_rules.hpp"
#include "monero_transfer_utils.hpp"
#include "monero_address_utils.hpp" // TODO: split this/these out into a different namespaces or file so this file can scale (leave file for shared utils)
#include "monero_paymentID_utils.hpp"
#include "monero_wallet_utils.hpp"
#include "monero_key_image_utils.hpp"
#include "wallet_errors.h"
#include "string_tools.h"
#include "ringct/rctSigs.h"
//
//
using namespace std;
using namespace boost;
using namespace cryptonote;
using namespace monero_transfer_utils;
using namespace monero_fork_rules;
//
using namespace serial_bridge;
//
network_type serial_bridge::nettype_from_string(const string &nettype_string)
{ // TODO: possibly move this to network_type declaration
	if (nettype_string == "MAINNET") {
		return MAINNET;
	} else if (nettype_string == "TESTNET") {
		return TESTNET;
	} else if (nettype_string == "STAGENET") {
		return STAGENET;
	} else if (nettype_string == "FAKECHAIN") {
		return FAKECHAIN;
	} else if (nettype_string == "UNDEFINED") {
		return UNDEFINED;
	}
	THROW_WALLET_EXCEPTION_IF(false, error::wallet_internal_error, "Unrecognized nettype_string")
	return UNDEFINED;
}
string serial_bridge::string_from_nettype(network_type nettype)
{
	switch (nettype) {
		case MAINNET:
			return "MAINNET";
		case TESTNET:
			return "TESTNET";
		case STAGENET:
			return "STAGENET";
		case FAKECHAIN:
			return "FAKECHAIN";
		case UNDEFINED:
			return "UNDEFINED";
		default:
			THROW_WALLET_EXCEPTION_IF(false, error::wallet_internal_error, "Unrecognized nettype for string conversion")
			return "UNDEFINED";
	}
}
struct RetVals_Transforms
{
	static string str_from(uint64_t v)
	{
		std::ostringstream o;
		o << v;
		return o.str();
	}
	static string str_from(uint32_t v)
	{
		std::ostringstream o;
		o << v;
		return o.str();
	}
};
//
// Shared - Parsing - Args
bool parsed_json_root(const string &args_string, boost::property_tree::ptree &json_root)
{
//	cout << "args_string: " << args_string << endl;
	
	std::stringstream ss;
	ss << args_string;
	try {
		boost::property_tree::read_json(ss, json_root);
	} catch (std::exception const& e) {
		THROW_WALLET_EXCEPTION_IF(false, error::wallet_internal_error, "Invalid JSON");
		return false;
	}
	return true;
}
//
// Shared - Factories - Return values
string ret_json_from_root(const boost::property_tree::ptree &root)
{
	stringstream ret_ss;
	boost::property_tree::write_json(ret_ss, root, false/*pretty*/);
	//
	return ret_ss.str();
}
string error_ret_json_from_message(string err_msg)
{
	boost::property_tree::ptree root;
	root.put(ret_json_key__any__err_msg(), err_msg);
	//
	return ret_json_from_root(root);
}
string error_ret_json_from_code(int code, optional<string> err_msg)
{
	boost::property_tree::ptree root;
	root.put(ret_json_key__any__err_code(), code);
	if (err_msg != none) {
		root.put(ret_json_key__any__err_msg(), *err_msg);
	}
	//
	return ret_json_from_root(root);
}
//
//
// Bridge Function Implementations
//
string serial_bridge::decode_address(const string &args_string)
{
	boost::property_tree::ptree json_root;
	if (!parsed_json_root(args_string, json_root)) {
		// it will already have thrown an exception
		return error_ret_json_from_message("Invalid JSON");
	}
	auto retVals = monero::address_utils::decodedAddress(json_root.get<string>("address"), nettype_from_string(json_root.get<string>("nettype_string")));
	if (retVals.did_error) {
		return error_ret_json_from_message(*(retVals.err_string));
	}
	boost::property_tree::ptree root;
	root.put(ret_json_key__isSubaddress(), retVals.isSubaddress);
	root.put(ret_json_key__pub_viewKey_string(), std::move(*(retVals.pub_viewKey_string)));
	root.put(ret_json_key__pub_spendKey_string(), std::move(*(retVals.pub_spendKey_string)));
	if (retVals.paymentID_string != none) {
		root.put(ret_json_key__paymentID_string(), std::move(*(retVals.paymentID_string)));
	}
	//
	return ret_json_from_root(root);
}
string serial_bridge::is_subaddress(const string &args_string)
{
	boost::property_tree::ptree json_root;
	if (!parsed_json_root(args_string, json_root)) {
		// it will already have thrown an exception
		return error_ret_json_from_message("Invalid JSON");
	}
	bool retVal = monero::address_utils::isSubAddress(json_root.get<string>("address"), nettype_from_string(json_root.get<string>("nettype_string")));
	boost::property_tree::ptree root;
	root.put(ret_json_key__generic_retVal(), retVal);
	//
	return ret_json_from_root(root);
}
string serial_bridge::is_integrated_address(const string &args_string)
{
	boost::property_tree::ptree json_root;
	if (!parsed_json_root(args_string, json_root)) {
		// it will already have thrown an exception
		return error_ret_json_from_message("Invalid JSON");
	}
	bool retVal = monero::address_utils::isIntegratedAddress(json_root.get<string>("address"), nettype_from_string(json_root.get<string>("nettype_string")));
	boost::property_tree::ptree root;
	root.put(ret_json_key__generic_retVal(), retVal);
	//
	return ret_json_from_root(root);
}
string serial_bridge::new_integrated_address(const string &args_string)
{
	boost::property_tree::ptree json_root;
	if (!parsed_json_root(args_string, json_root)) {
		// it will already have thrown an exception
		return error_ret_json_from_message("Invalid JSON");
	}
	optional<string> retVal = monero::address_utils::new_integratedAddrFromStdAddr(json_root.get<string>("address"), json_root.get<string>("short_pid"), nettype_from_string(json_root.get<string>("nettype_string")));
	boost::property_tree::ptree root;
	if (retVal != none) {
		root.put(ret_json_key__generic_retVal(), std::move(*retVal));
	}
	//
	return ret_json_from_root(root);
}
string serial_bridge::new_payment_id(const string &args_string)
{
	boost::property_tree::ptree json_root;
	if (!parsed_json_root(args_string, json_root)) {
		// it will already have thrown an exception
		return error_ret_json_from_message("Invalid JSON");
	}
	optional<string> retVal = monero_paymentID_utils::new_short_plain_paymentID_string();
	boost::property_tree::ptree root;
	if (retVal != none) {
		root.put(ret_json_key__generic_retVal(), std::move(*retVal));
	}
	//
	return ret_json_from_root(root);
}
//
string serial_bridge::newly_created_wallet(const string &args_string)
{
	boost::property_tree::ptree json_root;
	if (!parsed_json_root(args_string, json_root)) {
		// it will already have thrown an exception
		return error_ret_json_from_message("Invalid JSON");
	}
	monero_wallet_utils::WalletDescriptionRetVals retVals;
	bool r = monero_wallet_utils::convenience__new_wallet_with_language_code(
		json_root.get<string>("locale_language_code"),
		retVals,
		nettype_from_string(json_root.get<string>("nettype_string"))
	);
	bool did_error = retVals.did_error;
	if (!r) {
		return error_ret_json_from_message(*(retVals.err_string));
	}
	THROW_WALLET_EXCEPTION_IF(did_error, error::wallet_internal_error, "Illegal success flag but did_error");
	//
	boost::property_tree::ptree root;
	root.put(
		ret_json_key__mnemonic_string(),
		std::string((*(retVals.optl__desc)).mnemonic_string.data(), (*(retVals.optl__desc)).mnemonic_string.size())
	);
	root.put(ret_json_key__mnemonic_language(), (*(retVals.optl__desc)).mnemonic_language);
	root.put(ret_json_key__sec_seed_string(), (*(retVals.optl__desc)).sec_seed_string);
	root.put(ret_json_key__address_string(), (*(retVals.optl__desc)).address_string);
	root.put(ret_json_key__pub_viewKey_string(), epee::string_tools::pod_to_hex((*(retVals.optl__desc)).pub_viewKey));
	root.put(ret_json_key__sec_viewKey_string(), epee::string_tools::pod_to_hex((*(retVals.optl__desc)).sec_viewKey));
	root.put(ret_json_key__pub_spendKey_string(), epee::string_tools::pod_to_hex((*(retVals.optl__desc)).pub_spendKey));
	root.put(ret_json_key__sec_spendKey_string(), epee::string_tools::pod_to_hex((*(retVals.optl__desc)).sec_spendKey));
	//
	return ret_json_from_root(root);
}
string serial_bridge::are_equal_mnemonics(const string &args_string)
{
	boost::property_tree::ptree json_root;
	if (!parsed_json_root(args_string, json_root)) {
		// it will already have thrown an exception
		return error_ret_json_from_message("Invalid JSON");
	}
	bool equal;
	try {
		equal = monero_wallet_utils::are_equal_mnemonics(
			json_root.get<string>("a"),
			json_root.get<string>("b")
		);
	} catch (std::exception const& e) {
		return error_ret_json_from_message(e.what());
	}
	boost::property_tree::ptree root;
	root.put(ret_json_key__generic_retVal(), equal);
	//
	return ret_json_from_root(root);
}
string serial_bridge::address_and_keys_from_seed(const string &args_string)
{
	boost::property_tree::ptree json_root;
	if (!parsed_json_root(args_string, json_root)) {
		// it will already have thrown an exception
		return error_ret_json_from_message("Invalid JSON");
	}
	monero_wallet_utils::ComponentsFromSeed_RetVals retVals;
	bool r = monero_wallet_utils::address_and_keys_from_seed(
		json_root.get<string>("seed_string"),
		nettype_from_string(json_root.get<string>("nettype_string")),
		retVals
	);
	bool did_error = retVals.did_error;
	if (!r) {
		return error_ret_json_from_message(*(retVals.err_string));
	}
	THROW_WALLET_EXCEPTION_IF(did_error, error::wallet_internal_error, "Illegal success flag but did_error");
	//
	boost::property_tree::ptree root;
	root.put(ret_json_key__address_string(), (*(retVals.optl__val)).address_string);
	root.put(ret_json_key__pub_viewKey_string(), epee::string_tools::pod_to_hex((*(retVals.optl__val)).pub_viewKey));
	root.put(ret_json_key__sec_viewKey_string(), epee::string_tools::pod_to_hex((*(retVals.optl__val)).sec_viewKey));
	root.put(ret_json_key__pub_spendKey_string(), epee::string_tools::pod_to_hex((*(retVals.optl__val)).pub_spendKey));
	root.put(ret_json_key__sec_spendKey_string(), epee::string_tools::pod_to_hex((*(retVals.optl__val)).sec_spendKey));
	//
	return ret_json_from_root(root);
}
string serial_bridge::mnemonic_from_seed(const string &args_string)
{
	boost::property_tree::ptree json_root;
	if (!parsed_json_root(args_string, json_root)) {
		// it will already have thrown an exception
		return error_ret_json_from_message("Invalid JSON");
	}
	monero_wallet_utils::SeedDecodedMnemonic_RetVals retVals = monero_wallet_utils::mnemonic_string_from_seed_hex_string(
		json_root.get<string>("seed_string"),
		json_root.get<string>("wordset_name")
	);
	boost::property_tree::ptree root;
	if (retVals.err_string != none) {
		return error_ret_json_from_message(*(retVals.err_string));
	}
	root.put(
		ret_json_key__generic_retVal(),
		std::string((*(retVals.mnemonic_string)).data(), (*(retVals.mnemonic_string)).size())
	);
	//
	return ret_json_from_root(root);
}
string serial_bridge::seed_and_keys_from_mnemonic(const string &args_string)
{
	boost::property_tree::ptree json_root;
	if (!parsed_json_root(args_string, json_root)) {
		// it will already have thrown an exception
		return error_ret_json_from_message("Invalid JSON");
	}
	monero_wallet_utils::WalletDescriptionRetVals retVals;
	bool r = monero_wallet_utils::wallet_with(
		json_root.get<string>("mnemonic_string"),
		retVals,
		nettype_from_string(json_root.get<string>("nettype_string"))
	);
	bool did_error = retVals.did_error;
	if (!r) {
		return error_ret_json_from_message(*retVals.err_string);
	}
	monero_wallet_utils::WalletDescription walletDescription = *(retVals.optl__desc);
	THROW_WALLET_EXCEPTION_IF(did_error, error::wallet_internal_error, "Illegal success flag but did_error");
	//
	boost::property_tree::ptree root;
	root.put(ret_json_key__sec_seed_string(), (*(retVals.optl__desc)).sec_seed_string);
	root.put(ret_json_key__mnemonic_language(), (*(retVals.optl__desc)).mnemonic_language);
	root.put(ret_json_key__address_string(), (*(retVals.optl__desc)).address_string);
	root.put(ret_json_key__pub_viewKey_string(), epee::string_tools::pod_to_hex((*(retVals.optl__desc)).pub_viewKey));
	root.put(ret_json_key__sec_viewKey_string(), epee::string_tools::pod_to_hex((*(retVals.optl__desc)).sec_viewKey));
	root.put(ret_json_key__pub_spendKey_string(), epee::string_tools::pod_to_hex((*(retVals.optl__desc)).pub_spendKey));
	root.put(ret_json_key__sec_spendKey_string(), epee::string_tools::pod_to_hex((*(retVals.optl__desc)).sec_spendKey));
	//
	return ret_json_from_root(root);
}
string serial_bridge::validate_components_for_login(const string &args_string)
{
	boost::property_tree::ptree json_root;
	if (!parsed_json_root(args_string, json_root)) {
		// it will already have thrown an exception
		return error_ret_json_from_message("Invalid JSON");
	}
	monero_wallet_utils::WalletComponentsValidationResults retVals;
	bool r = monero_wallet_utils::validate_wallet_components_with( // returns !did_error
		json_root.get<string>("address_string"),
		json_root.get<string>("sec_viewKey_string"),
		json_root.get_optional<string>("sec_spendKey_string"),
		json_root.get_optional<string>("seed_string"),
		nettype_from_string(json_root.get<string>("nettype_string")),
		retVals
	);
	bool did_error = retVals.did_error;
	if (!r) {
		return error_ret_json_from_message(*retVals.err_string);
	}
	THROW_WALLET_EXCEPTION_IF(did_error, error::wallet_internal_error, "Illegal success flag but did_error");
	//
	boost::property_tree::ptree root;
	root.put(ret_json_key__isValid(), retVals.isValid);
	root.put(ret_json_key__isInViewOnlyMode(), retVals.isInViewOnlyMode);
	root.put(ret_json_key__pub_viewKey_string(), std::move(retVals.pub_viewKey_string));
	root.put(ret_json_key__pub_spendKey_string(), std::move(retVals.pub_spendKey_string));
	//
	return ret_json_from_root(root);
}
string serial_bridge::estimated_tx_network_fee(const string &args_string)
{
	boost::property_tree::ptree json_root;
	if (!parsed_json_root(args_string, json_root)) {
		// it will already have thrown an exception
		return error_ret_json_from_message("Invalid JSON");
	}
	uint64_t fee = monero_fee_utils::estimated_tx_network_fee(
		stoull(json_root.get<string>("fee_per_b")),
		stoul(json_root.get<string>("priority")),
		[] (uint8_t version, int64_t early_blocks) -> bool
		{
			return lightwallet_hardcoded__use_fork_rules(version, early_blocks);
		}
	);
	std::ostringstream o;
	o << fee;
	//
	boost::property_tree::ptree root;
	root.put(ret_json_key__generic_retVal(), o.str());
	//
	return ret_json_from_root(root);
}
//
string serial_bridge::generate_key_image(const string &args_string)
{
	boost::property_tree::ptree json_root;
	if (!parsed_json_root(args_string, json_root)) {
		// it will already have thrown an exception
		return error_ret_json_from_message("Invalid JSON");
	}
	crypto::secret_key sec_viewKey{};
	crypto::secret_key sec_spendKey{};
	crypto::public_key pub_spendKey{};
	crypto::public_key tx_pub_key{};
	{
		bool r = false;
		r = epee::string_tools::hex_to_pod(std::string(json_root.get<string>("sec_viewKey_string")), sec_viewKey);
		THROW_WALLET_EXCEPTION_IF(!r, error::wallet_internal_error, "Invalid secret view key");
		r = epee::string_tools::hex_to_pod(std::string(json_root.get<string>("sec_spendKey_string")), sec_spendKey);
		THROW_WALLET_EXCEPTION_IF(!r, error::wallet_internal_error, "Invalid secret spend key");
		r = epee::string_tools::hex_to_pod(std::string(json_root.get<string>("pub_spendKey_string")), pub_spendKey);
		THROW_WALLET_EXCEPTION_IF(!r, error::wallet_internal_error, "Invalid public spend key");
		r = epee::string_tools::hex_to_pod(std::string(json_root.get<string>("tx_pub_key")), tx_pub_key);
		THROW_WALLET_EXCEPTION_IF(!r, error::wallet_internal_error, "Invalid tx pub key");
	}
	monero_key_image_utils::KeyImageRetVals retVals;
	bool r = monero_key_image_utils::new__key_image(
		pub_spendKey, sec_spendKey, sec_viewKey, tx_pub_key,
		stoull(json_root.get<string>("out_index")),
		retVals
	);
	if (!r) {
		return error_ret_json_from_message("Unable to generate key image"); // TODO: return error string? (unwrap optional)
	}
	boost::property_tree::ptree root;
	root.put(ret_json_key__generic_retVal(), epee::string_tools::pod_to_hex(retVals.calculated_key_image));
	//
	return ret_json_from_root(root);
}
//
//
// TODO: possibly take tx sec key as a arg so we don't have to worry about randomness there
string serial_bridge::send_step1__prepare_params_for_get_decoys(const string &args_string)
{
	boost::property_tree::ptree json_root;
	if (!parsed_json_root(args_string, json_root)) {
		// it will already have thrown an exception
		return error_ret_json_from_message("Invalid JSON");
	}
	//
	vector<SpendableOutput> unspent_outs;
	BOOST_FOREACH(boost::property_tree::ptree::value_type &output_desc, json_root.get_child("unspent_outs"))
	{
		assert(output_desc.first.empty()); // array elements have no names
		SpendableOutput out{};
		out.amount = stoull(output_desc.second.get<string>("amount"));
		out.public_key = output_desc.second.get<string>("public_key");
		out.rct = output_desc.second.get_optional<string>("rct");
		out.global_index = stoull(output_desc.second.get<string>("global_index"));
		out.index = stoull(output_desc.second.get<string>("index"));
		out.tx_pub_key = output_desc.second.get<string>("tx_pub_key");
		//
		unspent_outs.push_back(out);
	}
	optional<string> optl__passedIn_attemptAt_fee_string = json_root.get_optional<string>("passedIn_attemptAt_fee");
	optional<uint64_t> optl__passedIn_attemptAt_fee = none;
	if (optl__passedIn_attemptAt_fee_string != none) {
		optl__passedIn_attemptAt_fee = stoull(*optl__passedIn_attemptAt_fee_string);
	}
	Send_Step1_RetVals retVals;
	monero_transfer_utils::send_step1__prepare_params_for_get_decoys(
		retVals,
		//
		json_root.get_optional<string>("payment_id_string"),
		stoull(json_root.get<string>("sending_amount")),
		json_root.get<bool>("is_sweeping"),
		stoul(json_root.get<string>("priority")),
		[] (uint8_t version, int64_t early_blocks) -> bool
		{
			return lightwallet_hardcoded__use_fork_rules(version, early_blocks);
		},
		unspent_outs,
		stoull(json_root.get<string>("fee_per_b")), // per v8
		//
		optl__passedIn_attemptAt_fee // use this for passing step2 "must-reconstruct" return values back in, i.e. re-entry; when nil, defaults to attempt at network min
	);
	boost::property_tree::ptree root;
	if (retVals.errCode != noError) {
		root.put(ret_json_key__any__err_code(), retVals.errCode);
		root.put(ret_json_key__any__err_msg(), err_msg_from_err_code__create_transaction(retVals.errCode));
		//
		// The following will be set if errCode==needMoreMoneyThanFound - and i'm depending on them being 0 otherwise
		root.put(ret_json_key__send__spendable_balance(), std::move(RetVals_Transforms::str_from(retVals.spendable_balance)));
		root.put(ret_json_key__send__required_balance(), std::move(RetVals_Transforms::str_from(retVals.required_balance)));
	} else {
		root.put(ret_json_key__send__mixin(), std::move(RetVals_Transforms::str_from(retVals.mixin)));
		root.put(ret_json_key__send__using_fee(), std::move(RetVals_Transforms::str_from(retVals.using_fee)));
		root.put(ret_json_key__send__final_total_wo_fee(), std::move(RetVals_Transforms::str_from(retVals.final_total_wo_fee)));
		root.put(ret_json_key__send__change_amount(), std::move(RetVals_Transforms::str_from(retVals.change_amount)));
		{
			boost::property_tree::ptree using_outs_ptree;
			BOOST_FOREACH(SpendableOutput &out, retVals.using_outs)
			{ // PROBABLY don't need to shuttle these back (could send only public_key) but consumers might like the feature of being able to send this JSON structure directly back to step2 without reconstructing it for themselves
				boost::property_tree::ptree out_ptree;
				out_ptree.put("amount", std::move(RetVals_Transforms::str_from(out.amount)));
				out_ptree.put("public_key", out.public_key); // FIXME: no std::move correct?
				if (out.rct != none) {
					out_ptree.put("rct", *out.rct); // copy vs move ?
				}
				out_ptree.put("global_index", std::move(RetVals_Transforms::str_from(out.global_index)));
				out_ptree.put("index", std::move(RetVals_Transforms::str_from(out.index)));
				out_ptree.put("tx_pub_key", out.tx_pub_key);
				using_outs_ptree.push_back(std::make_pair("", out_ptree));
			}
			root.add_child(ret_json_key__send__using_outs(), using_outs_ptree);
		}
	}
	return ret_json_from_root(root);
}
string serial_bridge::send_step2__try_create_transaction(const string &args_string)
{
	boost::property_tree::ptree json_root;
	if (!parsed_json_root(args_string, json_root)) {
		// it will already have thrown an exception
		return error_ret_json_from_message("Invalid JSON");
	}
	//
	vector<SpendableOutput> using_outs;
	BOOST_FOREACH(boost::property_tree::ptree::value_type &output_desc, json_root.get_child("using_outs"))
	{
		assert(output_desc.first.empty()); // array elements have no names
		SpendableOutput out{};
		out.amount = stoull(output_desc.second.get<string>("amount"));
		out.public_key = output_desc.second.get<string>("public_key");
		out.rct = output_desc.second.get_optional<string>("rct");
		out.global_index = stoull(output_desc.second.get<string>("global_index"));
		out.index = stoull(output_desc.second.get<string>("index"));
		out.tx_pub_key = output_desc.second.get<string>("tx_pub_key");
		//
		using_outs.push_back(out);
	}
	vector<RandomAmountOutputs> mix_outs;
	BOOST_FOREACH(boost::property_tree::ptree::value_type &mix_out_desc, json_root.get_child("mix_outs"))
	{
		assert(mix_out_desc.first.empty()); // array elements have no names
		auto amountAndOuts = RandomAmountOutputs{};
		amountAndOuts.amount = stoull(mix_out_desc.second.get<string>("amount"));
		BOOST_FOREACH(boost::property_tree::ptree::value_type &mix_out_output_desc, mix_out_desc.second.get_child("outputs"))
		{
			assert(mix_out_output_desc.first.empty()); // array elements have no names
			auto amountOutput = RandomAmountOutput{};
			amountOutput.global_index = stoull(mix_out_output_desc.second.get<string>("global_index")); // this is, I believe, presently supplied as a string by the API, probably to avoid overflow
			amountOutput.public_key = mix_out_output_desc.second.get<string>("public_key");
			amountOutput.rct = mix_out_output_desc.second.get_optional<string>("rct");
			amountAndOuts.outputs.push_back(amountOutput);
		}
		mix_outs.push_back(amountAndOuts);
	}
	optional<string> optl__passedIn_attemptAt_fee_string = json_root.get_optional<string>("passedIn_attemptAt_fee");
	optional<uint64_t> optl__passedIn_attemptAt_fee = none;
	if (optl__passedIn_attemptAt_fee_string != none) {
		optl__passedIn_attemptAt_fee = stoull(*optl__passedIn_attemptAt_fee_string);
	}
	Send_Step2_RetVals retVals;
	monero_transfer_utils::send_step2__try_create_transaction(
		retVals,
		//
		json_root.get<string>("from_address_string"),
		json_root.get<string>("sec_viewKey_string"),
		json_root.get<string>("sec_spendKey_string"),
		json_root.get<string>("to_address_string"),
		json_root.get_optional<string>("payment_id_string"),
		stoull(json_root.get<string>("final_total_wo_fee")),
		stoull(json_root.get<string>("change_amount")),
		stoull(json_root.get<string>("fee_amount")),
		stoul(json_root.get<string>("priority")),
		using_outs,
		stoull(json_root.get<string>("fee_per_b")),
		mix_outs,
		[] (uint8_t version, int64_t early_blocks) -> bool
		{
		   return lightwallet_hardcoded__use_fork_rules(version, early_blocks);
		},
		stoull(json_root.get<string>("unlock_time")),
		nettype_from_string(json_root.get<string>("nettype_string"))
	);
	boost::property_tree::ptree root;
	if (retVals.errCode != noError) {
		root.put(ret_json_key__any__err_code(), retVals.errCode);
		root.put(ret_json_key__any__err_msg(), err_msg_from_err_code__create_transaction(retVals.errCode));
	} else {
		if (retVals.tx_must_be_reconstructed) {
			root.put(ret_json_key__send__tx_must_be_reconstructed(), true);
			root.put(ret_json_key__send__fee_actually_needed(), std::move(RetVals_Transforms::str_from(retVals.fee_actually_needed))); // must be passed back
		} else {
			root.put(ret_json_key__send__tx_must_be_reconstructed(), false); // so consumers have it available
			root.put(ret_json_key__send__serialized_signed_tx(), std::move(*(retVals.signed_serialized_tx_string)));
			root.put(ret_json_key__send__tx_hash(), std::move(*(retVals.tx_hash_string)));
			root.put(ret_json_key__send__tx_key(), std::move(*(retVals.tx_key_string)));
			root.put(ret_json_key__send__tx_pub_key(), std::move(*(retVals.tx_pub_key_string)));
		}
	}
	return ret_json_from_root(root);
}
//
string serial_bridge::decodeRct(const string &args_string)
{
	boost::property_tree::ptree json_root;
	if (!parsed_json_root(args_string, json_root)) {
		// it will already have thrown an exception
		return error_ret_json_from_message("Invalid JSON");
	}
	rct::key sk;
	if (!epee::string_tools::hex_to_pod(json_root.get<string>("sk"), sk)) {
		return error_ret_json_from_message("Invalid 'sk'");
	}
	unsigned int i = stoul(json_root.get<string>("i"));
	// NOTE: this rv structure parsing could be factored but it presently does not implement a number of sub-components of rv, such as .pseudoOuts
	auto rv_desc = json_root.get_child("rv");
	rct::rctSig rv = AUTO_VAL_INIT(rv);
	unsigned int rv_type_int = stoul(rv_desc.get<string>("type"));
	// got to be a better way to do this
	if (rv_type_int == rct::RCTTypeNull) {
		rv.type = rct::RCTTypeNull;
	} else if (rv_type_int == rct::RCTTypeSimple) {
		rv.type = rct::RCTTypeSimple;
	} else if (rv_type_int == rct::RCTTypeFull) {
		rv.type = rct::RCTTypeFull;
	} else if (rv_type_int == rct::RCTTypeBulletproof) {
		rv.type = rct::RCTTypeBulletproof;
	} else {
		return error_ret_json_from_message("Invalid 'rv.type'");
	}
	BOOST_FOREACH(boost::property_tree::ptree::value_type &ecdh_info_desc, rv_desc.get_child("ecdhInfo"))
	{
		assert(ecdh_info_desc.first.empty()); // array elements have no names
		auto ecdh_info = rct::ecdhTuple{};
		if (!epee::string_tools::hex_to_pod(ecdh_info_desc.second.get<string>("mask"), ecdh_info.mask)) {
			return error_ret_json_from_message("Invalid rv.ecdhInfo[].mask");
		}
		if (!epee::string_tools::hex_to_pod(ecdh_info_desc.second.get<string>("amount"), ecdh_info.amount)) {
			return error_ret_json_from_message("Invalid rv.ecdhInfo[].amount");
		}
		rv.ecdhInfo.push_back(ecdh_info);
	}
	BOOST_FOREACH(boost::property_tree::ptree::value_type &outPk_desc, rv_desc.get_child("outPk"))
	{
		assert(outPk_desc.first.empty()); // array elements have no names
		auto outPk = rct::ctkey{};
		if (!epee::string_tools::hex_to_pod(outPk_desc.second.get<string>("mask"), outPk.mask)) {
			return error_ret_json_from_message("Invalid rv.outPk[].mask");
		}
		// FIXME: does dest need to be placed on the key?
		rv.outPk.push_back(outPk);
	}
	//
	rct::key mask;
	rct::xmr_amount/*uint64_t*/ decoded_amount;
	try {
		decoded_amount = rct::decodeRct(
			rv, sk, i, mask,
			hw::get_device("default") // presently this uses the default device but we could let a string be passed to switch the type
		);
	} catch (std::exception const& e) {
		return error_ret_json_from_message(e.what());
	}
	stringstream decoded_amount_ss;
	decoded_amount_ss << decoded_amount;
	//
	boost::property_tree::ptree root;
	root.put(ret_json_key__decodeRct_mask(), epee::string_tools::pod_to_hex(mask));
	root.put(ret_json_key__decodeRct_amount(), decoded_amount_ss.str());
	//
	return ret_json_from_root(root);	
}
string serial_bridge::generate_key_derivation(const string &args_string)
{
	boost::property_tree::ptree json_root;
	if (!parsed_json_root(args_string, json_root)) {
		// it will already have thrown an exception
		return error_ret_json_from_message("Invalid JSON");
	}
	public_key pub_key;
	if (!epee::string_tools::hex_to_pod(json_root.get<string>("pub"), pub_key)) {
		return error_ret_json_from_message("Invalid 'pub'");
	}
	secret_key sec_key;
	if (!epee::string_tools::hex_to_pod(json_root.get<string>("sec"), sec_key)) {
		return error_ret_json_from_message("Invalid 'sec'");
	}
	crypto::key_derivation derivation = AUTO_VAL_INIT(derivation);
	if (!crypto::generate_key_derivation(pub_key, sec_key, derivation)) {
		return error_ret_json_from_message("Unable to generate key derivation");
	}
	boost::property_tree::ptree root;
	root.put(ret_json_key__generic_retVal(), epee::string_tools::pod_to_hex(derivation));
	//
	return ret_json_from_root(root);
}
string serial_bridge::derive_public_key(const string &args_string)
{
	boost::property_tree::ptree json_root;
	if (!parsed_json_root(args_string, json_root)) {
		// it will already have thrown an exception
		return error_ret_json_from_message("Invalid JSON");
	}
	crypto::key_derivation derivation;
	if (!epee::string_tools::hex_to_pod(json_root.get<string>("derivation"), derivation)) {
		return error_ret_json_from_message("Invalid 'derivation'");
	}
	std::size_t output_index = stoul(json_root.get<string>("out_index"));
	crypto::public_key base;
	if (!epee::string_tools::hex_to_pod(json_root.get<string>("pub"), base)) {
		return error_ret_json_from_message("Invalid 'pub'");
	}
	crypto::public_key derived_key = AUTO_VAL_INIT(derived_key);
	if (!crypto::derive_public_key(derivation, output_index, base, derived_key)) {
		return error_ret_json_from_message("Unable to derive public key");
	}
	boost::property_tree::ptree root;
	root.put(ret_json_key__generic_retVal(), epee::string_tools::pod_to_hex(derived_key));
	//
	return ret_json_from_root(root);
}

string serial_bridge::json_to_binary(const std::string &buff_json) {
	//std::string buff_bin;
	string buff_bin;
	crypto::json_to_binary(buff_json, buff_bin);
	cout << "Buff bin length: " << buff_bin.length() << "\n";

	// copy binary string into the heap and keep pointer
	std::string* myString = new std::string(buff_bin.c_str(), buff_bin.length());

	//buff_bin = *new string("Hello there!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1189");	// mess up if greater than 47 characters
	//buff_bin = *new string("Hello there!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1189");	// mess up unless one '!' removed
	//buff_bin = *new string("Hello there! Hello there! Hello there! Hello the");	// mess up unless one '!' removed

	//buff_bin = *new string("Hello there!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1189");	// mess up
	//buff_bin = *new string("Hello there my good man can we please exceed a limit so as to mess\n\rup this process!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1189");	// mess up
	//buff_bin = *new string(buff_bin.c_str(), buff_bin.length());
	std::cout << "serial_bridge::json_to_binary(): " << myString[0] << "\n";
	boost::property_tree::ptree root;
	root.put("ptr", reinterpret_cast<intptr_t>(myString->c_str()));
	//root.put("ptr", (int) buff_bin.c_str());
	root.put("length", myString->length());

	cout << "Address info: " << ret_json_from_root(root) << "\n";

	cout << "Lets iterate character by character...\n";
	for (int i = 0; i < myString->length(); i++) {
		cout << myString->at(i);
	}
	cout << "\n";

	return ret_json_from_root(root);
}

string serial_bridge::binary_to_json(const std::string &bin_mem_info_str) {
	std::cout << "serial_bridge::binary_to_json(): " << bin_mem_info_str << "\n";
	boost::property_tree::ptree json_root;
	if (!parsed_json_root(bin_mem_info_str, json_root)) {
		// it will already have thrown an exception
		return error_ret_json_from_message("Invalid JSON");
	}

	char* ptr = (char*) json_root.get<int>("ptr");
	int length = json_root.get<int>("length");

	//cout << "Serial bridge binary pointer: " << &ptr[0] << "\n";
	cout << "Serial bridge binary length: " << length << "\n";



//	cout << address[0] << "\n";

//	cout << "Lets print this character by character\n";
//	for (int i = 0; i < length; i++) {
//		cout << address[i];
//	}
//	cout << "\n";

//	std::string ret(buffer, bufflen);
//
//	    return ret;


	std::string buff_bin(ptr, length);
	cout << "Retrieved binary: \n";
	cout << buff_bin << "\n";

	cout << "Lets iterate character by character...\n";
	for (int i = 0; i < length; i++) {
		cout << buff_bin.at(i);
	}
	cout << "\n";



	std::string buff_json;
	crypto::binary_to_json(buff_bin, buff_json);
	cout << "Dumped JSON :" << buff_json << "\n";
	return buff_json;

//	char* c = (char*) json_root.get<int>("address");
//	for (int i = 0; i < length; i++) {
//		cout << c[i];
//	}
//	cout << "\n";


//	std::string buff_json;
//	crypto::binary_to_json(buff_bin, buff_json);
//	return "Not implemented";
//	boost::property_tree::ptree root;
//	root.put(ret_json_key__generic_retVal(), buff_json);
//	//
//	return ret_json_from_root(root);
}

string serial_bridge::derive_subaddress_public_key(const string &args_string)
{
	boost::property_tree::ptree json_root;
	if (!parsed_json_root(args_string, json_root)) {
		// it will already have thrown an exception
		return error_ret_json_from_message("Invalid JSON");
	}
	crypto::key_derivation derivation;
	if (!epee::string_tools::hex_to_pod(json_root.get<string>("derivation"), derivation)) {
		return error_ret_json_from_message("Invalid 'derivation'");
	}
	std::size_t output_index = stoul(json_root.get<string>("out_index"));
	crypto::public_key out_key;
	if (!epee::string_tools::hex_to_pod(json_root.get<string>("output_key"), out_key)) {
		return error_ret_json_from_message("Invalid 'output_key'");
	}
	crypto::public_key derived_key = AUTO_VAL_INIT(derived_key);
	if (!crypto::derive_subaddress_public_key(out_key, derivation, output_index, derived_key)) {
		return error_ret_json_from_message("Unable to derive public key");
	}
	boost::property_tree::ptree root;
	root.put(ret_json_key__generic_retVal(), epee::string_tools::pod_to_hex(derived_key));
	//
	return ret_json_from_root(root);
}
