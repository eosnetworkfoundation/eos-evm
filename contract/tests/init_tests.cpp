#include "basic_evm_tester.hpp"

BOOST_AUTO_TEST_SUITE(evm_init_tests)
BOOST_FIXTURE_TEST_CASE(check_init, basic_evm_tester) try {
   //all of these should fail since the contract hasn't been init'd yet

   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "pushtx"_n, "evm"_n, mvo()("ram_payer", "evm"_n)("rlptx", bytes())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract not initialized");});

   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "open"_n, "evm"_n, mvo()("owner", "evm"_n)("ram_payer", "evm"_n)),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract not initialized");});
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "close"_n, "evm"_n, mvo()("owner", "evm"_n)),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract not initialized");});
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "withdraw"_n, "evm"_n, mvo()("owner", "evm"_n)("quantity", asset())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract not initialized");});
   // Test of transfer notification w/o init is handled in native_token_evm_tests/transfer_notifier_without_init test as it requires additional setup

   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "testtx"_n, "evm"_n, mvo()("rlptx", bytes())("bi", mvo()("coinbase", bytes())("difficulty", 0)("gasLimit", 0)("number", 0)("timestamp", 0))),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract not initialized");});
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "updatecode"_n, "evm"_n, mvo()("address", bytes())("incarnation", 0)("code_hash", bytes())("code", bytes())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract not initialized");});
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "updateaccnt"_n, "evm"_n, mvo()("address", bytes())("initial", bytes())("current", bytes())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract not initialized");});
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "updatestore"_n, "evm"_n, mvo()("address", bytes())("incarnation" ,0)("location", bytes())("initial", bytes())("current", bytes())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract not initialized");});
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "dumpstorage"_n, "evm"_n, mvo()("addy", bytes())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract not initialized");});
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "clearall"_n, "evm"_n, mvo()),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract not initialized");});
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "dumpall"_n, "evm"_n, mvo()),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract not initialized");});
   BOOST_REQUIRE_EXCEPTION(push_action("evm"_n, "setbal"_n, "evm"_n, mvo()("addy", bytes())("bal", bytes())),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract not initialized");});

   BOOST_REQUIRE_EXCEPTION(init(42),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: unknown chainid");});
   init(15555);
   produce_block();
   BOOST_REQUIRE_EXCEPTION(init(15555),
                           eosio_assert_message_exception,
                           [](const eosio_assert_message_exception& e) {return testing::expect_assert_message(e, "assertion failure with message: contract already initialized");});

} FC_LOG_AND_RETHROW()
BOOST_AUTO_TEST_SUITE_END()