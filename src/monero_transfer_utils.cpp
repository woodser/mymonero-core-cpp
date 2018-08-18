//
//  monero_transfer_utils.cpp
//  Copyright © 2018 MyMonero. All rights reserved.
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
//
#include "monero_transfer_utils.hpp"
#include "wallet_errors.h"
#include "string_tools.h"
#include "monero_paymentID_utils.hpp"
//
using namespace std;
using namespace crypto;
using namespace std;
using namespace boost;
using namespace epee;
using namespace cryptonote;
using namespace tools; // for error::
using namespace monero_transfer_utils;
using namespace monero_fork_rules;

//
// Protocol / Defaults
uint32_t monero_transfer_utils::fixed_ringsize()
{
	return 7; // best practice is to conform to fixed default ring size
}
uint32_t monero_transfer_utils::fixed_mixinsize()
{
	return monero_transfer_utils::fixed_ringsize() - 1;
}
uint32_t monero_transfer_utils::default_priority()
{
	return 1; // lowest (TODO: declare centrally)
}
//
// Fee estimation
uint64_t monero_transfer_utils::estimated_tx_network_fee(
	uint64_t fee_per_kb,
	uint32_t priority,
	network_type nettype,
	use_fork_rules_fn_type use_fork_rules_fn
) {
	bool bulletproof = use_fork_rules_fn(get_bulletproof_fork(), 0);
	uint64_t fee_multiplier = get_fee_multiplier(priority, default_priority(), get_fee_algorithm(use_fork_rules_fn), use_fork_rules_fn);
	std::vector<uint8_t> extra; // blank extra
	size_t est_tx_size = estimate_rct_tx_size(2, fixed_mixinsize(), 2, extra.size(), bulletproof); // typically ~14kb post-rct, pre-bulletproofs
	uint64_t estimated_fee = calculate_fee(fee_per_kb, est_tx_size, fee_multiplier);
	//
	return estimated_fee;
}
uint64_t monero_transfer_utils::get_upper_transaction_size_limit(
	uint64_t upper_transaction_size_limit__or_0_for_default,
	use_fork_rules_fn_type use_fork_rules_fn
) {
	if (upper_transaction_size_limit__or_0_for_default > 0)
		return upper_transaction_size_limit__or_0_for_default;
	uint64_t full_reward_zone = use_fork_rules_fn(5, 10) ? CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5 : use_fork_rules_fn(2, 10) ? CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2 : CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1;
	return full_reward_zone - CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE;
}
uint64_t monero_transfer_utils::get_fee_multiplier(
	uint32_t priority,
	uint32_t default_priority,
	int fee_algorithm,
	use_fork_rules_fn_type use_fork_rules_fn
) {
	static const uint64_t old_multipliers[3] = {1, 2, 3};
	static const uint64_t new_multipliers[3] = {1, 20, 166};
	static const uint64_t newer_multipliers[4] = {1, 4, 20, 166};

	if (fee_algorithm == -1)
		fee_algorithm = get_fee_algorithm(use_fork_rules_fn);

	// 0 -> default (here, x1 till fee algorithm 2, x4 from it)
	if (priority == 0)
		priority = default_priority;
	if (priority == 0)
	{
		if (fee_algorithm >= 2)
			priority = 2;
		else
			priority = 1;
	}

	// 1 to 3/4 are allowed as priorities
	uint32_t max_priority = (fee_algorithm >= 2) ? 4 : 3;
	if (priority >= 1 && priority <= max_priority)
	{
		switch (fee_algorithm)
		{
			case 0: return old_multipliers[priority-1];
			case 1: return new_multipliers[priority-1];
			case 2: return newer_multipliers[priority-1];
			default: THROW_WALLET_EXCEPTION_IF (true, error::invalid_priority);
		}
	}

	THROW_WALLET_EXCEPTION_IF (false, error::invalid_priority);
	return 1;
}
int monero_transfer_utils::get_fee_algorithm(use_fork_rules_fn_type use_fork_rules_fn)
{
	// changes at v3 and v5
	if (use_fork_rules_fn(5, 0))
		return 2;
	if (use_fork_rules_fn(3, -720 * 14))
		return 1;
	return 0;
}
size_t monero_transfer_utils::estimate_rct_tx_size(int n_inputs, int mixin, int n_outputs, size_t extra_size, bool bulletproof)
{
	size_t size = 0;

	// tx prefix

	// first few bytes
	size += 1 + 6;

	// vin
	size += n_inputs * (1+6+(mixin+1)*2+32);

	// vout
	size += n_outputs * (6+32);

	// extra
	size += extra_size;

	// rct signatures

	// type
	size += 1;

	// rangeSigs
	if (bulletproof)
		size += ((2*6 + 4 + 5)*32 + 3) * n_outputs;
	else
		size += (2*64*32+32+64*32) * n_outputs;

	// MGs
	size += n_inputs * (64 * (mixin+1) + 32);

	// mixRing - not serialized, can be reconstructed
	/* size += 2 * 32 * (mixin+1) * n_inputs; */

	// pseudoOuts
	size += 32 * n_inputs;
	// ecdhInfo
	size += 2 * 32 * n_outputs;
	// outPk - only commitment is saved
	size += 32 * n_outputs;
	// txnFee
	size += 4;

	LOG_PRINT_L2("estimated rct tx size for " << n_inputs << " with ring size " << (mixin+1) << " and " << n_outputs << ": " << size << " (" << ((32 * n_inputs/*+1*/) + 2 * 32 * (mixin+1) * n_inputs + 32 * n_outputs) << " saved)");
	return size;
}
size_t monero_transfer_utils::estimate_tx_size(bool use_rct, int n_inputs, int mixin, int n_outputs, size_t extra_size, bool bulletproof)
{
	if (use_rct)
		return estimate_rct_tx_size(n_inputs, mixin, n_outputs + 1, extra_size, bulletproof);
	else
		return n_inputs * (mixin+1) * APPROXIMATE_INPUT_BYTES + extra_size;
}
//
uint64_t monero_transfer_utils::calculate_fee(uint64_t fee_per_kb, size_t bytes, uint64_t fee_multiplier)
{
	uint64_t kB = (bytes + 1023) / 1024;
	return kB * fee_per_kb * fee_multiplier;
}
uint64_t monero_transfer_utils::calculate_fee(uint64_t fee_per_kb, const cryptonote::blobdata &blob, uint64_t fee_multiplier)
{
	return calculate_fee(fee_per_kb, blob.size(), fee_multiplier);
}
//
// Transfer parsing/derived properties
bool monero_transfer_utils::is_transfer_unlocked(
	uint64_t unlock_time,
	uint64_t block_height,
	uint64_t blockchain_size, /* extracting wallet2->m_blockchain.size() / m_local_bc_height */
	network_type nettype
) {
	if(!is_tx_spendtime_unlocked(unlock_time, block_height, blockchain_size, nettype))
		return false;

	if(block_height + CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE > blockchain_size)
		return false;

	return true;
}
bool monero_transfer_utils::is_tx_spendtime_unlocked(
	uint64_t unlock_time,
	uint64_t block_height,
	uint64_t blockchain_size,
	network_type nettype
) {
	if(unlock_time < CRYPTONOTE_MAX_BLOCK_NUMBER)
	{
		//interpret as block index
		if(blockchain_size-1 + CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS >= unlock_time)
			return true;
		else
			return false;
	}else
	{
		//interpret as time
		uint64_t current_time = static_cast<uint64_t>(time(NULL));
		// XXX: this needs to be fast, so we'd need to get the starting heights
		// from the daemon to be correct once voting kicks in
		uint64_t v2height = nettype == TESTNET ? 624634 : nettype == STAGENET ? (uint64_t)-1/*TODO*/ : 1009827;
		uint64_t leeway = block_height < v2height ? CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_SECONDS_V1 : CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_SECONDS_V2;
		if(current_time + leeway >= unlock_time)
			return true;
		else
			return false;
	}
	return false;
}
//
// Constructing transactions
std::string monero_transfer_utils::new_dummy_address_string_for_rct_tx(network_type nettype)
{
	cryptonote::account_base account;
	account.generate();
	//
	return account.get_public_address_str(nettype);
}
bool _rct_hex_to_rct_commit(
	const std::string &rct_string,
	rct::key &rct_commit
) {
	// rct string is empty if output is non RCT
	if (rct_string.empty()) {
		return false;
	}
	// rct_string is a string with length 64+64+64 (<rct commit> + <encrypted mask> + <rct amount>)
	std::string rct_commit_str = rct_string.substr(0,64);
	THROW_WALLET_EXCEPTION_IF(!string_tools::validate_hex(64, rct_commit_str), error::wallet_internal_error, "Invalid rct commit hash: " + rct_commit_str);
	string_tools::hex_to_pod(rct_commit_str, rct_commit);
	return true;
}
bool _rct_hex_to_decrypted_mask(
	const std::string &rct_string,
	const crypto::secret_key &view_secret_key,
	const crypto::public_key& tx_pub_key,
	uint64_t internal_output_index,
	rct::key &decrypted_mask
) {
	// rct string is empty if output is non RCT
	if (rct_string.empty()) {
		return false;
	}
	// rct_string is a string with length 64+64+64 (<rct commit> + <encrypted mask> + <rct amount>)
	rct::key encrypted_mask;
	std::string encrypted_mask_str = rct_string.substr(64,64);
	THROW_WALLET_EXCEPTION_IF(!string_tools::validate_hex(64, encrypted_mask_str), error::wallet_internal_error, "Invalid rct mask: " + encrypted_mask_str);
	string_tools::hex_to_pod(encrypted_mask_str, encrypted_mask);
	//
	// Decrypt the mask
	crypto::key_derivation derivation;
	bool r = generate_key_derivation(tx_pub_key, view_secret_key, derivation);
	THROW_WALLET_EXCEPTION_IF(!r, error::wallet_internal_error, "Failed to generate key derivation");
	crypto::secret_key scalar;
	crypto::derivation_to_scalar(derivation, internal_output_index, scalar);
	sc_sub(decrypted_mask.bytes,encrypted_mask.bytes,rct::hash_to_scalar(rct::sk2rct(scalar)).bytes);
	
	return true;
}
bool _verify_sec_key(const crypto::secret_key &secret_key, const crypto::public_key &public_key)
{ // borrowed from device_default.cpp
	crypto::public_key calculated_pub;
	bool r = crypto::secret_key_to_public_key(secret_key, calculated_pub);
	return r && public_key == calculated_pub;
}
//
void monero_transfer_utils::create_transaction(
	TransactionConstruction_RetVals &retVals,
	const account_keys& sender_account_keys, // this will reference a particular hw::device
	const uint32_t subaddr_account_idx,
	const std::unordered_map<crypto::public_key, cryptonote::subaddress_index> &subaddresses,
	const vector<tx_destination_entry> &dsts,
	vector<SpendableOutput> &outputs,
	vector<RandomAmountOutputs> &mix_outs,
	uint64_t fee_amount,
	std::vector<uint8_t> &extra,
	use_fork_rules_fn_type use_fork_rules_fn,
	uint64_t unlock_time, // or 0
	bool rct,
	cryptonote::network_type nettype
) {
	retVals.errCode = noError;
	//
	// TODO: do we need to sort destinations by amount, here, according to 'decompose_destinations'?
	//
	uint32_t fake_outputs_count = fixed_mixinsize();
	bool bulletproof = use_fork_rules_fn(get_bulletproof_fork(), 0);
	//
	if (dsts.size() == 0) {
		retVals.errCode = noDestinations;
		return;
	}
	if (mix_outs.size() != outputs.size() && fake_outputs_count != 0) {
		retVals.errCode = wrongNumberOfMixOutsProvided;
		return;
	}
	for (size_t i = 0; i < mix_outs.size(); i++) {
		if (mix_outs[i].outputs.size() < fake_outputs_count) {
			retVals.errCode = notEnoughOutputsForMixing;
			return;
		}
	}
	if (!sender_account_keys.get_device().verify_keys(sender_account_keys.m_spend_secret_key, sender_account_keys.m_account_address.m_spend_public_key)
		|| !sender_account_keys.get_device().verify_keys(sender_account_keys.m_view_secret_key, sender_account_keys.m_account_address.m_view_public_key)) {
		retVals.errCode = invalidSecretKeys;
		return;
	}
	uint64_t needed_money = 0;
	for (size_t i = 0; i < dsts.size(); i++) {
		needed_money += dsts[i].amount;
		if (needed_money > UINT64_MAX) {
			retVals.errCode = outputAmountOverflow;
			return;
		}
	}
	uint64_t found_money = 0;
	std::vector<tx_source_entry> sources;
	// TODO: log: "Selected transfers: " << outputs
	for (size_t out_index = 0; out_index < outputs.size(); out_index++) {
		found_money += outputs[out_index].amount;
		if (found_money > UINT64_MAX) {
			retVals.errCode = inputAmountOverflow;
		}
		auto src = tx_source_entry{};
		src.amount = outputs[out_index].amount;
		src.rct = outputs[out_index].rct != none;
		//
		typedef cryptonote::tx_source_entry::output_entry tx_output_entry;
		if (mix_outs.size() != 0) {
			// Sort fake outputs by global index
			std::sort(mix_outs[out_index].outputs.begin(), mix_outs[out_index].outputs.end(), [] (
				RandomAmountOutput const& a,
				RandomAmountOutput const& b
			) {
				return a.global_index < b.global_index;
			});
			for (
				size_t j = 0;
				src.outputs.size() < fake_outputs_count && j < mix_outs[out_index].outputs.size();
				j++
			) {
				auto mix_out__output = mix_outs[out_index].outputs[j];
				if (mix_out__output.global_index == outputs[out_index].global_index) {
					LOG_PRINT_L2("got mixin the same as output, skipping");
					continue;
				}
				auto oe = tx_output_entry{};
				oe.first = mix_out__output.global_index;
				//
				crypto::public_key public_key = AUTO_VAL_INIT(public_key);
				if(!string_tools::validate_hex(64, mix_out__output.public_key)) {
					retVals.errCode = givenAnInvalidPubKey;
					return;
				}
				string_tools::hex_to_pod(mix_out__output.public_key, public_key);
				oe.second.dest = rct::pk2rct(public_key);
				//
				if (mix_out__output.rct != boost::none) {
					rct::key commit;
					_rct_hex_to_rct_commit(*mix_out__output.rct, commit);
					oe.second.mask = commit;
				} else {
					if (outputs[out_index].rct != boost::none) {
						retVals.errCode = mixRCTOutsMissingCommit;
						return;
					}
					oe.second.mask = rct::zeroCommit(src.amount); //create identity-masked commitment for non-rct mix input
				}
				src.outputs.push_back(oe);
			}
		}
		auto real_oe = tx_output_entry{};
		real_oe.first = outputs[out_index].global_index;
		//
		crypto::public_key public_key = AUTO_VAL_INIT(public_key);
		if(!string_tools::validate_hex(64, outputs[out_index].public_key)) {
			retVals.errCode = givenAnInvalidPubKey;
			return;
		}
		if (!string_tools::hex_to_pod(outputs[out_index].public_key, public_key)) {
			retVals.errCode = givenAnInvalidPubKey;
			return;
		}
		real_oe.second.dest = rct::pk2rct(public_key);
		//
		if (outputs[out_index].rct != none) {
			rct::key commit;
			_rct_hex_to_rct_commit(*(outputs[out_index].rct), commit);
			real_oe.second.mask = commit; //add commitment for real input
		} else {
			real_oe.second.mask = rct::zeroCommit(src.amount/*aka outputs[out_index].amount*/); //create identity-masked commitment for non-rct input
		}
		//
		// Add real_oe to outputs
		uint64_t real_output_index = src.outputs.size();
		for (size_t j = 0; j < src.outputs.size(); j++) {
			if (real_oe.first < src.outputs[j].first) {
				real_output_index = j;
				break;
			}
		}
		src.outputs.insert(src.outputs.begin() + real_output_index, real_oe);
		//
		crypto::public_key tx_pub_key = AUTO_VAL_INIT(tx_pub_key);
		if(!string_tools::validate_hex(64, outputs[out_index].tx_pub_key)) {
			retVals.errCode = givenAnInvalidPubKey;
			return;
		}
		string_tools::hex_to_pod(outputs[out_index].tx_pub_key, tx_pub_key);
		src.real_out_tx_key = tx_pub_key;
		//
		add_tx_pub_key_to_extra(extra, tx_pub_key); // TODO: is this necessary? it doesn't seem to affect is_out_to_acc checks..
		//
		src.real_out_additional_tx_keys = get_additional_tx_pub_keys_from_extra(extra);
		//
		src.real_output = real_output_index;
		uint64_t internal_output_index = outputs[out_index].index;
		src.real_output_in_tx_index = internal_output_index;
		//
		src.rct = outputs[out_index].rct != boost::none && (*(outputs[out_index].rct)).empty() == false;
		if (src.rct) {
			rct::key decrypted_mask;
			_rct_hex_to_decrypted_mask(
				*(outputs[out_index].rct),
				sender_account_keys.m_view_secret_key,
				tx_pub_key,
				internal_output_index,
				decrypted_mask
			);
			src.mask = decrypted_mask;
//			rct::key calculated_commit = rct::commit(outputs[out_index].amount, decrypted_mask);
//			rct::key parsed_commit;
//			_rct_hex_to_rct_commit(*(outputs[out_index].rct), parsed_commit);
//			if (!(real_oe.second.mask == calculated_commit)) { // real_oe.second.mask==parsed_commit(outputs[out_index].rct)
//				retVals.errCode = invalidCommitOrMaskOnOutputRCT;
//				return;
//			}
		} else {
			rct::key I;
			rct::identity(I);
			src.mask = I; // in JS MM code this was left as null for generate_key_image_helper_rct to, I think, fill in with identity I
		}
		// not doing multisig here yet
		src.multisig_kLRki = rct::multisig_kLRki({rct::zero(), rct::zero(), rct::zero(), rct::zero()});
		sources.push_back(src);
	}
	//
	// TODO: if this is a multisig wallet, create a list of multisig signers we can use
	std::vector<cryptonote::tx_destination_entry> splitted_dsts = dsts;
	cryptonote::tx_destination_entry change_dts = AUTO_VAL_INIT(change_dts);
	change_dts.amount = found_money - needed_money;
	//
	/* This is commented because it's presently supplied by whoever is calling this function.... But there's a good argument for bringing it in, here, especially after MyMonero clients integrate with this code and soon, share an implementation of SendFunds() (the analog of create_transactions_2 + transfer_selected*)
			if (change_dts.amount == 0) {
				if (splitted_dsts.size() == 1) {
					// If the change is 0, send it to a random address, to avoid confusing
					// the sender with a 0 amount output. We send a 0 amount in order to avoid
					// letting the destination be able to work out which of the inputs is the
					// real one in our rings
					LOG_PRINT_L2("generating dummy address for 0 change");
					cryptonote::account_base dummy;
					dummy.generate();
					change_dts.addr = dummy.get_keys().m_account_address;
					LOG_PRINT_L2("generated dummy address for 0 change");
					splitted_dsts.push_back(change_dts);
				}
			} else {
				change_dts.addr = sender_account_keys.m_account_address;
				splitted_dsts.push_back(change_dts);
			}
	 */
	//
	// TODO: log: "sources: " << sources
	if (found_money > needed_money) {
		if (change_dts.amount != fee_amount) {
			retVals.errCode = resultFeeNotEqualToGiven; // aka "early fee calculation != later"
			return; // early
		}
	} else if (found_money < needed_money) {
		retVals.errCode = needMoreMoneyThanFound; // TODO: return actual found_money and needed_money in generalized err params in return val
		return;
	}
	cryptonote::transaction tx;
	// TODO: need to initialize tx here?
	//
	auto sources_copy = sources;
	crypto::secret_key tx_key;
	std::vector<crypto::secret_key> additional_tx_keys;
	bool r = cryptonote::construct_tx_and_get_tx_key(
		sender_account_keys, subaddresses,
		sources, splitted_dsts, change_dts.addr, extra,
		tx, unlock_time, tx_key, additional_tx_keys,
		true, bulletproof,
		/*m_multisig ? &msout : */NULL
	);
	LOG_PRINT_L2("constructed tx, r="<<r);
	if (!r) {
		// TODO: return error::tx_not_constructed, sources, dsts, unlock_time, nettype
		retVals.errCode = transactionNotConstructed;
		return;
	}
	if (get_object_blobsize(tx) >= get_upper_transaction_size_limit(0, use_fork_rules_fn)) {
		// TODO: return error::tx_too_big, tx, upper_transaction_size_limit
		retVals.errCode = transactionTooBig;
		return;
	}
	bool use_bulletproofs = !tx.rct_signatures.p.bulletproofs.empty();
	THROW_WALLET_EXCEPTION_IF(use_bulletproofs != bulletproof, error::wallet_internal_error, "Expected tx use_bulletproofs to equal bulletproof flag");
	//
	retVals.tx = tx;
}
//
//
void monero_transfer_utils::convenience__create_transaction(
	Convenience_TransactionConstruction_RetVals &retVals,
	const string &from_address_string,
	const string &sec_viewKey_string,
	const string &sec_spendKey_string,
	const string &to_address_string,
	optional<string> payment_id_string,
	uint64_t amount, // to send
	uint64_t fee_amount,
	const std::vector<cryptonote::tx_destination_entry> &dsts, // this must include change or dummy address
	vector<SpendableOutput> &outputs,
	vector<RandomAmountOutputs> &mix_outs,
	use_fork_rules_fn_type use_fork_rules_fn,
	uint64_t unlock_time,
	network_type nettype
) {
	retVals.errCode = noError;
	//
	cryptonote::address_parse_info from_addr_info;
	THROW_WALLET_EXCEPTION_IF(!cryptonote::get_account_address_from_str(from_addr_info, nettype, from_address_string), error::wallet_internal_error, "Couldn't parse from-address");
	cryptonote::account_keys account_keys;
	{
		account_keys.m_account_address = from_addr_info.address;
		//
		crypto::secret_key sec_viewKey;
		THROW_WALLET_EXCEPTION_IF(!string_tools::hex_to_pod(sec_viewKey_string, sec_viewKey), error::wallet_internal_error, "Couldn't parse view key");
		account_keys.m_view_secret_key = sec_viewKey;
		//
		crypto::secret_key sec_spendKey;
		THROW_WALLET_EXCEPTION_IF(!string_tools::hex_to_pod(sec_spendKey_string, sec_spendKey), error::wallet_internal_error, "Couldn't parse spend key");
		account_keys.m_spend_secret_key = sec_spendKey;
	}
	//
	cryptonote::address_parse_info to_addr_info;
	THROW_WALLET_EXCEPTION_IF(!cryptonote::get_account_address_from_str(to_addr_info, nettype, to_address_string), error::wallet_internal_error, "Couldn't parse to-address");
	//
	std::vector<uint8_t> extra;
	bool payment_id_seen = false;
	{ // Detect hash8 or hash32 char hex string as pid and configure 'extra' accordingly
		bool r = false;
		if (payment_id_string != none) {
			crypto::hash payment_id;
			r = monero_paymentID_utils::parse_long_payment_id(*payment_id_string, payment_id);
			if (r) {
				std::string extra_nonce;
				cryptonote::set_payment_id_to_tx_extra_nonce(extra_nonce, payment_id);
				r = cryptonote::add_extra_nonce_to_tx_extra(extra, extra_nonce);
			} else {
				crypto::hash8 payment_id8;
				r = monero_paymentID_utils::parse_short_payment_id(*payment_id_string, payment_id8);
				if (r) {
					std::string extra_nonce;
					cryptonote::set_encrypted_payment_id_to_tx_extra_nonce(extra_nonce, payment_id8);
					r = cryptonote::add_extra_nonce_to_tx_extra(extra, extra_nonce);
				}
			}
			payment_id_seen = true;
		} else {
			if (to_addr_info.is_subaddress && payment_id_seen) {
				retVals.errCode = cantUsePIDWithSubAddress; // Never use a subaddress with a payment ID
				return;
			}
			if (to_addr_info.has_payment_id) {
				if (payment_id_seen) {
					retVals.errCode = nonZeroPIDWithIntAddress; // can't use int addr at same time as supplying manual pid
					return;
				}
				if (to_addr_info.is_subaddress) {
					THROW_WALLET_EXCEPTION_IF(false, error::wallet_internal_error, "Unexpected is_subaddress && has_payment_id"); // should never happen
					return;
				}
				std::string extra_nonce;
				cryptonote::set_encrypted_payment_id_to_tx_extra_nonce(extra_nonce, to_addr_info.payment_id);
				bool r = cryptonote::add_extra_nonce_to_tx_extra(extra, extra_nonce);
				if (!r) {
					retVals.errCode = couldntSetPIDToTXExtra;
					return;
				}
				payment_id_seen = true;
			}
		}
	}
	//
	uint32_t subaddr_account_idx = 0;
	std::unordered_map<crypto::public_key, cryptonote::subaddress_index> subaddresses;
	subaddresses[account_keys.m_account_address.m_spend_public_key] = {0,0};
	//
	TransactionConstruction_RetVals actualCall_retVals;
	create_transaction(
		actualCall_retVals,
		account_keys,
		subaddr_account_idx,
		subaddresses,
		dsts,
		outputs,
		mix_outs,
		fee_amount,
		extra,
		use_fork_rules_fn,
		unlock_time,
		true, // rct
		nettype
	);
	if (actualCall_retVals.errCode != noError) {
		retVals.errCode = actualCall_retVals.errCode; // pass-through
		return; // already set the error
	}
	auto txBlob = t_serializable_object_to_blob(actualCall_retVals.tx);
	size_t txBlob_byteLength = txBlob.size();
	//	cout << "txBlob: " << txBlob << endl;
	cout << "txBlob_byteLength: " << txBlob_byteLength << endl;
	THROW_WALLET_EXCEPTION_IF(txBlob_byteLength <= 0, error::wallet_internal_error, "Expected tx blob byte length > 0");
	//
	// tx hash
	retVals.tx_hash_string = epee::string_tools::pod_to_hex(cryptonote::get_transaction_hash(actualCall_retVals.tx));
	retVals.signed_serialized_tx_string = epee::string_tools::buff_to_hex_nodelimer(cryptonote::tx_to_blob(actualCall_retVals.tx));
}