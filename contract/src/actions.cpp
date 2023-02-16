#define NDEBUG 1 // make sure assert is no-op in processor.cpp

#include <eosio/system.hpp>
#include <eosio/transaction.hpp>
#include <evm_runtime/evm_contract.hpp>
#include <evm_runtime/tables.hpp>
#include <evm_runtime/state.hpp>
#include <evm_runtime/intrinsics.hpp>
#include <evm_runtime/eosio.token.hpp>

#include <silkworm/consensus/trust/engine.hpp>
// included here so NDEBUG is defined to disable assert macro
#include <silkworm/execution/processor.cpp>

#ifdef WITH_TEST_ACTIONS
#include <evm_runtime/test/engine.hpp>
#include <evm_runtime/test/config.hpp>
#endif

#ifdef WITH_LOGTIME
#define LOGTIME(MSG) eosio::internal_use_do_not_use::logtime(MSG)
#else
#define LOGTIME(MSG)
#endif

namespace silkworm {
    // provide no-op bloom
    Bloom logs_bloom(const std::vector<Log>& logs) {
        return {};
    }
}

namespace evm_runtime {

using namespace silkworm;

void evm_contract::init(const uint64_t chainid) {
    eosio::require_auth(get_self());

    check( !_config.exists(), "contract already initialized" );
    check( !!lookup_known_chain(chainid), "unknown chainid" );

    _config.set({
        .version = 0,
        .chainid = chainid,
        .genesis_time = current_time_point()
    }, get_self());

    stats_singleton(get_self(), get_self().value).get_or_create(get_self());

    open(get_self(), get_self());
}

void evm_contract::setingressfee(asset ingress_bridge_fee) {
    assert_inited();
    require_auth(get_self());

    check( ingress_bridge_fee.symbol == token_symbol, "unexpected bridge symbol" );
    check( ingress_bridge_fee.amount >= 0, "ingress bridge fee cannot be negative");

    config current_config = _config.get();
    current_config.ingress_bridge_fee = ingress_bridge_fee;
    _config.set(current_config, get_self());
}

void evm_contract::addegress(const std::vector<name>& accounts) {
    assert_inited();
    require_auth(get_self());

    egresslist egresslist_table(get_self(), get_self().value);

    for(const name& account : accounts)
        if(egresslist_table.find(account.value) == egresslist_table.end())
            egresslist_table.emplace(get_self(), [&](allowed_egress_account& a) {
                a.account = account;
            });
}

void evm_contract::removeegress(const std::vector<name>& accounts) {
    assert_inited();
    require_auth(get_self());

    egresslist egresslist_table(get_self(), get_self().value);

    for(const name& account : accounts)
        if(auto it = egresslist_table.find(account.value); it != egresslist_table.end())
            egresslist_table.erase(it);
}

void check_result( ValidationResult r, const Transaction& txn, const char* desc ) {
    if( r == ValidationResult::kOk )
        return;

    if( r == ValidationResult::kMissingSender ) {
        eosio::print("txn.from.has_value is empty\n");
    } else if ( r == ValidationResult::kSenderNoEOA ) {
        eosio::print("get_code_hash is empty\n");
    } else if ( r == ValidationResult::kWrongNonce ) {
        eosio::print("invalid nonce:", txn.nonce, "\n");
    } else if ( r == ValidationResult::kInsufficientFunds ) {
        eosio::print("get_balance of from insufficient\n");
    } else if ( r == ValidationResult::kBlockGasLimitExceeded ) {
        eosio::print("available_gas\n");
    }

    eosio::print( "ERR: ", uint64_t(r), "\n" );
    eosio::check( false, desc );
}

void evm_contract::push_trx( eosio::name ram_payer, Block& block, const bytes& rlptx, silkworm::consensus::IEngine& engine, const silkworm::ChainConfig& chain_config ) {

    Transaction tx;
    ByteView bv{(const uint8_t*)rlptx.data(), rlptx.size()};
    eosio::check(rlp::decode(bv,tx) == DecodingResult::kOk && bv.empty(), "unable to decode transaction");
    LOGTIME("EVM TX DECODE");

    tx.from.reset();
    tx.recover_sender();
    eosio::check(tx.from.has_value(), "unable to recover sender");
    LOGTIME("EVM RECOVER SENDER");

    evm_runtime::state state{get_self(), ram_payer};
    silkworm::ExecutionProcessor ep{block, engine, state, chain_config};

    ValidationResult r = consensus::pre_validate_transaction(tx, ep.evm().block().header.number, ep.evm().config(),
                                                             ep.evm().block().header.base_fee_per_gas);
    check_result( r, tx, "pre_validate_transaction error" );
    r = ep.validate_transaction(tx);
    check_result( r, tx, "validate_transaction error" );

    Receipt receipt;
    ep.execute_transaction(tx, receipt);

    engine.finalize(ep.state(), ep.evm().block(), ep.evm().revision());
    ep.state().write_to_db(ep.evm().block().header.number);

    LOGTIME("EVM EXECUTE");
}

void evm_contract::pushtx( eosio::name ram_payer, const bytes& rlptx ) {
    LOGTIME("EVM START");

    assert_inited();
    std::optional<std::pair<const std::string, const ChainConfig*>> found_chain_config = lookup_known_chain(_config.get().chainid);
    check( found_chain_config.has_value(), "failed to find expected chain config" );
    eosio::require_auth(ram_payer);

    Block block;
    block.header.difficulty  = 1;
    block.header.gas_limit   = 0x7ffffffffff;
    block.header.timestamp   = eosio::current_time_point().sec_since_epoch();
    block.header.number = 1 + (block.header.timestamp - _config.get().genesis_time.sec_since_epoch()); // same logic with block_mapping in TrustEVM

    silkworm::consensus::TrustEngine engine{*found_chain_config->second};
    push_trx( ram_payer, block, rlptx, engine, *found_chain_config->second );
}

void evm_contract::open(eosio::name owner, eosio::name ram_payer) {
    assert_inited();
    require_auth(ram_payer);
    check(is_account(owner), "owner account does not exist");

    balances balance_table(get_self(), get_self().value);
    if(balance_table.find(owner.value) == balance_table.end())
        balance_table.emplace(ram_payer, [&](balance& a) {
            a.owner = owner;
        });

    nextnonces nextnonce_table(get_self(), get_self().value);
    if(nextnonce_table.find(owner.value) == nextnonce_table.end())
        nextnonce_table.emplace(ram_payer, [&](nextnonce& a) {
            a.owner = owner;
        });
}

void evm_contract::close(eosio::name owner) {
    assert_inited();
    require_auth(owner);

    eosio::check(owner != get_self(), "Cannot close self");

    balances balance_table(get_self(), get_self().value);
    const balance& owner_account = balance_table.get(owner.value, "account is not open");

    eosio::check(owner_account.balance == balance_with_dust(), "cannot close because balance is not zero");
    balance_table.erase(owner_account);

    nextnonces nextnonce_table(get_self(), get_self().value);
    const nextnonce& next_nonce_for_owner = nextnonce_table.get(owner.value);
    //if the account has performed an EOS->EVM transfer the nonce needs to be maintained in case the account is re-opened in the future
    if(next_nonce_for_owner.next_nonce == 0)
        nextnonce_table.erase(next_nonce_for_owner);
}

uint64_t evm_contract::get_and_increment_nonce(const name owner) {
    nextnonces nextnonce_table(get_self(), get_self().value);

    const nextnonce& nonce = nextnonce_table.get(owner.value);
    uint64_t ret = nonce.next_nonce;
    nextnonce_table.modify(nonce, eosio::same_payer, [](nextnonce& n){
        ++n.next_nonce;
    });
    return ret;
}

checksum256 evm_contract::get_code_hash(name account) const {
    char buff[64];

    eosio::check(internal_use_do_not_use::get_code_hash(account.value, 0, buff, sizeof(buff)) <= sizeof(buff), "get_code_hash() too big");
    using start_of_code_hash_return = std::tuple<unsigned_int, uint64_t, checksum256>;
    const auto& [v, s, code_hash] = unpack<start_of_code_hash_return>(buff, sizeof(buff));

    return code_hash;
}

void evm_contract::handle_account_transfer(const eosio::asset& quantity, const std::string& memo) {
    eosio::name receiver(memo);

    balances balance_table(get_self(), get_self().value);
    const balance& receiver_account = balance_table.get(receiver.value, "receiving account has not been opened");

    balance_table.modify(receiver_account, eosio::same_payer, [&](balance& a) {
        a.balance.balance += quantity;
    });
}

void evm_contract::transfer(eosio::name from, eosio::name to, eosio::asset quantity, std::string memo) {
    assert_inited();
    eosio::check(quantity.symbol == token_symbol, "received unexpected token");

    if(to != get_self() || from == get_self())
        return;

    if(memo.size() == 42 && memo[0] == '0' && memo[1] == 'x')
        eosio::check(false, "unsupported");
    else if(!memo.empty() && memo.size() <= 13)
        handle_account_transfer(quantity, memo);
    else
        eosio::check(false, "memo must be either 0x EVM address or already opened account name to credit deposit to");
}

void evm_contract::withdraw(eosio::name owner, eosio::asset quantity) {
    assert_inited();
    require_auth(owner);

    balances balance_table(get_self(), get_self().value);
    const balance& owner_account = balance_table.get(owner.value, "account is not open");

    check(owner_account.balance.balance.amount >= quantity.amount, "overdrawn balance");
    balance_table.modify(owner_account, eosio::same_payer, [&](balance& a) {
        a.balance.balance -= quantity;
    });

    token::transfer_action transfer_act(token_account, {{get_self(), "active"_n}});
    transfer_act.send(get_self(), owner, quantity, std::string("Withdraw from EVM balance"));
}

bool evm_contract::gc(uint32_t max) {
    assert_inited();

    evm_runtime::state state{get_self(), eosio::same_payer};
    return state.gc(max);
}

#ifdef WITH_TEST_ACTIONS
ACTION evm_contract::testtx( const bytes& rlptx, const evm_runtime::test::block_info& bi ) {
    assert_inited();

    eosio::require_auth(get_self());

    Block block;
    block.header = bi.get_block_header();

    evm_runtime::test::engine engine;
    push_trx( get_self(), block, rlptx, engine, evm_runtime::test::kTestNetwork );
}

ACTION evm_contract::dumpstorage(const bytes& addy) {
    assert_inited();

    eosio::require_auth(get_self());

    account_table accounts(_self, _self.value);
    auto inx = accounts.get_index<"by.address"_n>();
    auto itr = inx.find(make_key(to_address(addy)));
    if(itr == inx.end()) {
        eosio::print("no data for: ");
        eosio::printhex(addy.data(), addy.size());
        eosio::print("\n");
        return;
    }

    eosio::print("storage: ");
    eosio::printhex(addy.data(), addy.size());

    uint64_t cnt=0;
    storage_table db(_self, itr->id);
    auto sitr = db.begin();
    while(sitr != db.end()) {
        eosio::print("\n");
        eosio::printhex(sitr->key.data(), sitr->key.size());
        eosio::print(":");
        eosio::printhex(sitr->value.data(), sitr->value.size());
        eosio::print("\n");
        ++sitr;
        ++cnt;
    }

    eosio::print(" = ", cnt, "\n");
}

ACTION evm_contract::dumpall() {
    assert_inited();

    eosio::require_auth(get_self());

    auto print_store = [](auto sitr) {
        eosio::print("    ");
        eosio::printhex(sitr->key.data(), sitr->key.size());
        eosio::print(":");
        eosio::printhex(sitr->value.data(), sitr->value.size());
        eosio::print("\n");
    };

    account_table accounts(_self, _self.value);
    auto itr = accounts.begin();
    eosio::print("DUMPALL start\n");
    while( itr != accounts.end() ) {
        eosio::print("  account:");
        eosio::printhex(itr->eth_address.data(), itr->eth_address.size());
        eosio::print("\n");
        storage_table db(_self, itr->id);
        auto sitr = db.begin();
        while( sitr != db.end() ) {
            print_store( sitr );
            sitr++;
        }
        
        itr++;
    }
    eosio::print("  gc:");
    gc_store_table gc(_self, _self.value);
    auto i = gc.begin();
    while( i != gc.end() ) {
        eosio::print("   storage_id:");
        eosio::print(i->storage_id);
        eosio::print("\n");
        storage_table db(_self, i->storage_id);
        auto sitr = db.begin();
        while( sitr != db.end() ) {
            print_store( sitr );
            ++sitr;
        }

        ++i;
    }

    eosio::print("DUMPALL end\n");
}


ACTION evm_contract::clearall() {
    assert_inited();

    eosio::require_auth(get_self());

    account_table accounts(_self, _self.value);
    auto itr = accounts.begin();
    eosio::print("CLEAR start\n");
    while( itr != accounts.end() ) {
        eosio::print("  account:");
        eosio::printhex(itr->eth_address.data(), itr->eth_address.size());
        eosio::print("\n");
        storage_table db(_self, itr->id);
        auto sitr = db.begin();
        while( sitr != db.end() ) {
            eosio::print("    ");
            eosio::printhex(sitr->key.data(), sitr->key.size());
            eosio::print(":");
            eosio::printhex(sitr->value.data(), sitr->value.size());
            eosio::print("\n");
            sitr = db.erase(sitr);
        }

        auto db_size = std::distance(db.cbegin(), db.cend());
        eosio::print("db size:", uint64_t(db_size), "\n");
        itr = accounts.erase(itr);
    }
    gc(std::numeric_limits<uint32_t>::max());

    auto account_size = std::distance(accounts.cbegin(), accounts.cend());
    eosio::print("accounts size:", uint64_t(account_size), "\n");

    eosio::print("CLEAR end\n");
}

ACTION evm_contract::updatecode( const bytes& address, uint64_t incarnation, const bytes& code_hash, const bytes& code) {
    assert_inited();

    eosio::require_auth(get_self());

    evm_runtime::state state{get_self(), get_self()};
    auto bvcode = ByteView{(const uint8_t *)code.data(), code.size()};
    state.update_account_code(to_address(address), incarnation, to_bytes32(code_hash), bvcode);
}

ACTION evm_contract::updatestore(const bytes& address, uint64_t incarnation, const bytes& location, const bytes& initial, const bytes& current) {
    assert_inited();

    eosio::require_auth(get_self());

    evm_runtime::state state{get_self(), get_self()};
    eosio::print("updatestore: ");
    eosio::printhex(address.data(), address.size());
    eosio::print("\n   ");
    eosio::printhex(location.data(), location.size());
    eosio::print(":");
    eosio::printhex(current.data(), current.size());
    eosio::print("\n");
    
    state.update_storage(to_address(address), incarnation, to_bytes32(location), to_bytes32(initial), to_bytes32(current));
}

ACTION evm_contract::updateaccnt(const bytes& address, const bytes& initial, const bytes& current) {
    assert_inited();

    eosio::require_auth(get_self());

    evm_runtime::state state{get_self(), get_self()};
    auto maybe_account = [](const bytes& data) -> std::optional<Account> {
        std::optional<Account> res{};
        if(data.size()) {
            Account tmp;
            ByteView bv{(const uint8_t *)data.data(), data.size()};
            auto dec_res = Account::from_encoded_storage(bv);
            eosio::check(dec_res.second == DecodingResult::kOk, "unable to decode account");
            res = dec_res.first;
        }
        return res;
    };

    auto oinitial = maybe_account(initial);
    auto ocurrent = maybe_account(current);

    state.update_account(to_address(address), oinitial, ocurrent);
}

ACTION evm_contract::setbal(const bytes& addy, const bytes& bal) {
    assert_inited();

    eosio::require_auth(get_self());

    account_table accounts(_self, _self.value);
    auto inx = accounts.get_index<"by.address"_n>();
    auto itr = inx.find(make_key(addy));

    if(itr == inx.end()) {
        accounts.emplace(get_self(), [&](auto& row){
            row.id = accounts.available_primary_key();;
            row.code_hash = to_bytes(kEmptyHash);
            row.eth_address = addy;
            row.balance = bal;
        });
    } else {
        accounts.modify(*itr, eosio::same_payer, [&](auto& row){
            row.balance = bal;
        });
    }
}
#endif //WITH_TEST_ACTIONS

} //evm_runtime
