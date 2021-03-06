#include "csnode/spammer.hpp"

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <string>

#include <base58.h>
#include <csdb/amount.hpp>
#include <csdb/amount_commission.hpp>
#include <csdb/currency.hpp>
#include <csdb/internal/types.hpp>
#include <csdb/transaction.hpp>
#include <csnode/conveyer.hpp>
#include <csnode/node.hpp>

namespace cs {
namespace {
// number of wallets, which will be generated to send transactions to
constexpr auto kMyWalletsNum = 10u;
// spamming starts after this timeout
constexpr auto kTimeStartSleepSec = 5u;
// increase this value to make spammer slower, otherwise decrease
constexpr auto kSpammerSleepTimeMicrosec = 350u;
// from this address public_key_ will be fund, genesis block address for test purposes
std::string kGenesisPublic = "5B3YXqDTcWQFGAqEJQJP3Bg1ZK8FFtHtgCiFLT5VAxpe";
std::string kGenesisPrivate = "3rUevsW5xfob6qDxWMDFwwTQCq39SYhzstuyfUGSDvF2QHBRyPD8fSk49wFXaPk3GztfxtuU85QHfMV3ozfqa7rN";

constexpr auto kMaxTransactionsFromOneSource = 1000u;
constexpr auto kMaxMoneyForOneSpammer = 1'000'000u;
constexpr auto kMaxTransactionsInOneRound = 100;
}  // namespace

void Spammer::StartSpamming(Node& node) {
    GenerateMyWallets();
    spam_thread_ = std::thread(&Spammer::SpamWithTransactions, this, std::ref(node));
    spam_thread_.detach();
}

void Spammer::GenerateMyWallets() {
    cscrypto::PublicKey public_key;
    cscrypto::PrivateKey private_key;
    for (auto i = 0u; i < kMyWalletsNum; ++i) {
        private_key = cscrypto::generateKeyPair(public_key);
        my_wallets_.push_back(std::pair<csdb::Address, cscrypto::PrivateKey>(csdb::Address::from_public_key(public_key), private_key));
        csdebug() << "Spammer: wallet[" << i << "] " << EncodeBase58(public_key.data(), public_key.data() + public_key.size()) << " - "
                  << EncodeBase58(private_key.access().data(), private_key.access().data() + private_key.access().size());
    }
}

void Spammer::SpamWithTransactions(Node& node) {
    std::this_thread::sleep_for(std::chrono::seconds(kTimeStartSleepSec));
    FundMyWallets(node);
    csdb::Transaction transaction;
    transaction.set_currency(csdb::Currency(1));
    transaction.set_amount(csdb::Amount(0.001));
    transaction.set_max_fee(csdb::AmountCommission(0.1));

    size_t target_wallet_counter = 0;
    size_t spammer_index = 0;
    int64_t inner_id_counter = 0;
    uint64_t round_spamming = 0;
    uint32_t tr_gen_in_round = 0;
    const cs::Conveyer& conveyer = cs::Conveyer::instance();
    cs::RoundNumber round_number = conveyer.currentRoundNumber();

    while (true) {
        if (!node.isPoolsSyncroStarted() && node.subRound() == 0) {
            if (target_wallet_counter == spammer_index) {
                ++target_wallet_counter;
                if (target_wallet_counter == kMyWalletsNum) {
                    target_wallet_counter = 0;
                }
            }

            transaction.set_source(OptimizeAddress(my_wallets_[spammer_index].first, node));
            transaction.set_target(OptimizeAddress(my_wallets_[target_wallet_counter].first, node));
            transaction.set_innerID(inner_id_counter);
            SignTransaction(transaction, my_wallets_[spammer_index].second);
            node.getSolver()->send_wallet_transaction(transaction);

            ++inner_id_counter;
            ++target_wallet_counter;
            ++tr_gen_in_round;
            if (target_wallet_counter == kMyWalletsNum) {
                target_wallet_counter = 0;
            }
            if (cs::numeric_cast<uint64_t>(inner_id_counter) == (round_spamming + 1) * kMaxTransactionsFromOneSource - 1) {
                ++spammer_index;
                if (spammer_index == kMyWalletsNum) {
                    spammer_index = 0;
                    ++round_spamming;
                }
                inner_id_counter = cs::numeric_cast<int64_t>(round_spamming * kMaxTransactionsFromOneSource);
            }
        }
        while (tr_gen_in_round >= kMaxTransactionsInOneRound && round_number == conveyer.currentRoundNumber()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(kSpammerSleepTimeMicrosec * 2));
        }
        while (kMaxTransactionsInOneRound <= conveyer.packetQueueTransactionsCount()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(kSpammerSleepTimeMicrosec * 2));
        }
        if (round_number != conveyer.currentRoundNumber()) {
            tr_gen_in_round = 0;
            round_number = conveyer.currentRoundNumber();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(kSpammerSleepTimeMicrosec));
    }
}

void Spammer::FundMyWallets(Node& node) {
    csdb::Address genesis_address;
    std::vector<uint8_t> genesis;
    DecodeBase58(kGenesisPublic, genesis);
    genesis_address = csdb::Address::from_public_key(genesis);
    DecodeBase58(kGenesisPrivate, genesis);
    for (auto i = 0u; i < kMyWalletsNum; ++i) {
        csdb::Transaction transaction;
        transaction.set_source(OptimizeAddress(genesis_address, node));
        transaction.set_target(my_wallets_[i].first);
        transaction.set_currency(csdb::Currency(1));
        transaction.set_amount(csdb::Amount(kMaxMoneyForOneSpammer / kMyWalletsNum, 0));
        transaction.set_max_fee(csdb::AmountCommission(0.1));
        transaction.set_counted_fee(csdb::AmountCommission(0.0));
        srand(cs::numeric_cast<unsigned int>(time(0)));
        transaction.set_innerID((rand() + 2) & 0x3fffffffffff);
        SignTransaction(transaction, genesis);
        node.getSolver()->send_wallet_transaction(transaction);
    }
}

csdb::Address Spammer::OptimizeAddress(const csdb::Address& address, Node& node) {
    csdb::internal::WalletId id;
    // thread safety is provided by findWalletId method
    if (node.getBlockChain().findWalletId(address, id)) {
        return csdb::Address::from_wallet_id(id);
    }
    return address;
}

void Spammer::SignTransaction(csdb::Transaction& transaction, const std::vector<cscrypto::Byte>& private_key) {
    const auto transaction_bytes = transaction.to_byte_stream_for_sig();
    const auto key = cscrypto::PrivateKey::readFromBytes(private_key);

    cs::Signature signature = cscrypto::generateSignature(key, transaction_bytes.data(), transaction_bytes.size());
    transaction.set_signature(signature);
}

void Spammer::SignTransaction(csdb::Transaction& transaction, const cscrypto::PrivateKey& private_key) {
    const auto transaction_bytes = transaction.to_byte_stream_for_sig();
    cs::Signature signature = cscrypto::generateSignature(private_key, transaction_bytes.data(), transaction_bytes.size());
    transaction.set_signature(signature);
}

}  // namespace cs
