#include "basic_evm_tester.hpp"

BOOST_AUTO_TEST_SUITE(evm_freeze_tests)
BOOST_FIXTURE_TEST_CASE(check_freeze, basic_evm_tester) try {
   init(15555);
   produce_block();

   push_action("evm"_n, "freeze"_n, "evm"_n, mvo()("value", true));

   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "pushtx"_n, "evm"_n, mvo()("ram_payer", "evm"_n)("rlptx", bytes())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract is frozen");});

   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "open"_n, "evm"_n, mvo()("owner", "evm"_n)("ram_payer", "evm"_n)),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract is frozen");});
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "close"_n, "evm"_n, mvo()("owner", "evm"_n)),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract is frozen");});
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "withdraw"_n, "evm"_n, mvo()("owner", "evm"_n)("quantity", asset())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract is frozen");});
   // Test of transfer notification w/o init is handled in native_token_evm_tests/transfer_notifier_without_init test as it requires additional setup

   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "testtx"_n, "evm"_n, mvo()("rlptx", bytes())("bi", mvo()("coinbase", bytes())("difficulty", 0)("gasLimit", 0)("number", 0)("timestamp", 0))),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract is frozen");});
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "updatecode"_n, "evm"_n, mvo()("address", bytes())("incarnation", 0)("code_hash", bytes())("code", bytes())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract is frozen");});
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "updateaccnt"_n, "evm"_n, mvo()("address", bytes())("initial", bytes())("current", bytes())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract is frozen");});
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "updatestore"_n, "evm"_n, mvo()("address", bytes())("incarnation" ,0)("location", bytes())("initial", bytes())("current", bytes())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract is frozen");});

   // dump storage is ok                        
   push_action("evm"_n, "dumpstorage"_n, "evm"_n, mvo()("addy", bytes()));

   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "clearall"_n, "evm"_n, mvo()),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract is frozen");});

   // dumpall is ok 
   push_action("evm"_n, "dumpall"_n, "evm"_n, mvo());

   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "setbal"_n, "evm"_n, mvo()("addy", bytes())("bal", bytes())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract is frozen");});
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "gc"_n, "evm"_n, mvo()("max", 42)),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract is frozen");});

   // unfreeze
   push_action("evm"_n, "freeze"_n, "evm"_n, mvo()("value", false));
   produce_block();

   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "pushtx"_n, "evm"_n, mvo()("ram_payer", "evm"_n)("rlptx", bytes())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: unable to decode transaction");});

   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "withdraw"_n, "evm"_n, mvo()("owner", "evm"_n)("quantity", asset())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: account is not open");});

   push_action("evm"_n, "open"_n, "evm"_n, mvo()("owner", "evm"_n)("ram_payer", "evm"_n));

   push_action("evm"_n, "close"_n, "evm"_n, mvo()("owner", "evm"_n));

   // Test of transfer notification w/o init is handled in native_token_evm_tests/transfer_notifier_without_init test as it requires additional setup

   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "testtx"_n, "evm"_n, mvo()("rlptx", bytes())("bi", mvo()("coinbase", bytes())("difficulty", 0)("gasLimit", 0)("number", 0)("timestamp", 0))),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: invalid coinbase");});

   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "updatecode"_n, "evm"_n, mvo()("address", bytes())("incarnation", 0)("code_hash", bytes())("code", bytes())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: wrong length");});

   push_action("evm"_n, "updateaccnt"_n, "evm"_n, mvo()("address", bytes())("initial", bytes())("current", bytes()));

   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "updatestore"_n, "evm"_n, mvo()("address", bytes())("incarnation" ,0)("location", bytes())("initial", bytes())("current", bytes())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: wrong length");});

   push_action("evm"_n, "dumpstorage"_n, "evm"_n, mvo()("addy", bytes()));

   push_action("evm"_n, "clearall"_n, "evm"_n, mvo());

   push_action("evm"_n, "dumpall"_n, "evm"_n, mvo());

   push_action("evm"_n, "setbal"_n, "evm"_n, mvo()("addy", bytes())("bal", bytes()));

   push_action("evm"_n, "gc"_n, "evm"_n, mvo()("max", 42));

} FC_LOG_AND_RETHROW()
BOOST_AUTO_TEST_SUITE_END()
