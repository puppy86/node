/* Send blaming letters to @yrtimd */
#ifndef __NODE_HPP__
#define __NODE_HPP__
#include <memory>

#include <csconnector/csconnector.h>
#include <csstats.h>
#include <client/config.hpp>

#include "blockchain.hpp"
#include "packstream.hpp"

enum NodeLevel { Normal, Confidant, Main, Writer };

using Vector = std::string;
using Matrix = std::string;

class Transport;
namespace cs {
class Solver;
}

class Node {
 public:
  Node(const Config&);
  ~Node();

  bool isGood() const {
    return good_;
  }
  void run(const Config&);

  /* Incoming requests processing */
  void getRoundTable(const uint8_t*, const size_t, const RoundNum, uint8_t type = 0);
  void getBigBang(const uint8_t*, const size_t, const RoundNum, uint8_t type);
  void getTransaction(const uint8_t*, const size_t);
  void getFirstTransaction(const uint8_t*, const size_t);
  void getTransactionsList(const uint8_t*, const size_t);
  void getVector(const uint8_t*, const size_t, const PublicKey& sender);
  void getMatrix(const uint8_t*, const size_t, const PublicKey& sender);
  void getBlock(const uint8_t*, const size_t, const PublicKey& sender);
  void getHash(const uint8_t*, const size_t, const PublicKey& sender);
  void getTransactionsPacket(const uint8_t*, const std::size_t);

  // transaction's pack syncro
  void getPacketHashesRequest(const uint8_t*, const std::size_t, const PublicKey& sender);
  void getPacketHashesReply(const uint8_t*, const std::size_t);

  void getRoundTableUpdated(const uint8_t*, const size_t, const RoundNum);
  void getCharacteristic(const uint8_t* data, const size_t size, const PublicKey& sender);

  void getNotification(const uint8_t* data, const std::size_t size);
  void sendNotification(const PublicKey& destination);
  std::string createNotification();

  /*syncro get functions*/
  void getBlockRequest(const uint8_t*, const size_t, const PublicKey& sender);
  void getBlockReply(const uint8_t*, const size_t);
  void getWritingConfirmation(const uint8_t* data, const size_t size, const PublicKey& sender);
  void getRoundTableRequest(const uint8_t* data, const size_t size, const PublicKey& sender);

  void getBadBlock(const uint8_t*, const size_t, const PublicKey& sender);

  /* Outcoming requests forming */
  void sendRoundTable();
  void sendTransaction(csdb::Pool&&);
  void sendFirstTransaction(const csdb::Transaction&);
  void sendTransactionList(const csdb::Pool&);
  void sendVector(const cs::HashVector&);
  void sendMatrix(const cs::HashMatrix&);
  void sendBlock(const csdb::Pool&);
  void sendHash(const std::string&, const PublicKey&);

  // transaction's pack syncro
  void sendTransactionsPacket(const cs::TransactionsPacket& packet);
  void sendPacketHashesRequest(const std::vector<cs::TransactionsPacketHash>& hashes);
  void sendPacketHashesReply(const cs::TransactionsPacket& packet, const PublicKey& sender);

  void sendBadBlock(const csdb::Pool& pool);
  void sendCharacteristic(const csdb::Pool& emptyMetaPool, const uint32_t maskBitsCount,
                          const std::vector<uint8_t>& characteristic);

  /*syncro send functions*/
  void sendBlockRequest(uint32_t seq);
  void sendBlockReply(const csdb::Pool&, const PublicKey&);
  void sendWritingConfirmation(const PublicKey& node);
  void sendRoundTableRequest(size_t rNum);
  void sendRoundTableUpdated(const cs::RoundInfo& round);

  void sendVectorRequest(const PublicKey&);
  void sendMatrixRequest(const PublicKey&);

  void sendTLRequest();
  void getTlRequest(const uint8_t* data, const size_t size);

  void getVectorRequest(const uint8_t* data, const size_t size);
  void getMatrixRequest(const uint8_t* data, const size_t size);

  void flushCurrentTasks();
  void becomeWriter();
  void initNextRound(const cs::RoundInfo& roundInfo);
  bool getSyncroStarted();

  enum MessageActions { Process, Postpone, Drop };
  MessageActions chooseMessageAction(const RoundNum, const MsgTypes);

  const PublicKey& getMyPublicKey() const {
    return myPublicKey_;
  }
  NodeLevel getMyLevel() const {
    return myLevel_;
  }
  uint32_t getRoundNumber();
  uint8_t getMyConfNumber();

  const std::vector<PublicKey>& getConfidants() const {
    return confidantNodes_;
  }

  BlockChain& getBlockChain() {
    return bc_;
  }
  const BlockChain& getBlockChain() const {
    return bc_;
  }

#ifdef NODE_API
  csconnector::connector& getConnector() {
    return api_;
  }
#endif

  PublicKey writerId;
  void addToPackageTemporaryStorage(const csdb::Pool& pool);

private:
  bool init();

  // signature verification
  bool checkKeysFile();
  void generateKeys();
  bool checkKeysForSig();

  inline bool readRoundData(bool);
  void        onRoundStart();

  void composeMessageWithBlock(const csdb::Pool&, const MsgTypes);
  void composeCompressed(const void*, const uint32_t, const MsgTypes);

  // Info
  const PublicKey myPublicKey_;
  bool            good_ = true;

  // syncro variables
  bool     syncro_started = false;
  uint32_t sendBlockRequestSequence;
  bool     awaitingSyncroBlock   = false;
  uint32_t awaitingRecBlockCount = 0;

  // signature variables
  std::vector<uint8_t> myPublicForSig;
  std::vector<uint8_t> myPrivateForSig;

  std::string rcvd_trx_fname = "rcvd.txt";
  std::string sent_trx_fname = "sent.txt";

  // Current round state
  RoundNum  roundNum_ = 0;
  NodeLevel myLevel_;

  PublicKey              mainNode_;
  std::vector<PublicKey> confidantNodes_;

  uint8_t myConfNumber;
  uint16_t m_notificationsCount;

  // Resources
  BlockChain bc_;

  cs::Solver* solver_;
  Transport*  transport_;

#ifdef MONITOR_NODE
  csstats::csstats       stats_;
#endif

#ifdef NODE_API
  csconnector::connector api_;
#endif

  RegionAllocator packStreamAllocator_;
  RegionAllocator allocator_;

  size_t lastStartSequence_;
  bool blocksReceivingStarted_ = false;

  IPackStream istream_;
  OPackStream ostream_;
  std::vector<csdb::Pool> m_packageTemporaryStorage;
};
#endif  // __NODE_HPP__
