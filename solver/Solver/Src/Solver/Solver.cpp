////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                    Created by Analytical Solytions Core Team 07.09.2018                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <iostream>
#include <random>
#include <sstream>
#include <fstream>
#include <chrono>

#include <csdb/address.h>
#include <csdb/currency.h>
#include <csdb/wallet.h>

#include <csnode/node.hpp>

#include "Solver/Generals.hpp"
#include "Solver/Solver.hpp"
#include <algorithm>
#include <cmath>

#include <lib/system/logger.hpp>

#include <base58.h>
#include <sodium.h>

#pragma region moved to solver2
namespace {
void addTimestampToPool(csdb::Pool& pool)
{
  auto now_time = std::chrono::system_clock::now();
  pool.add_user_field(0, std::to_string(
    std::chrono::duration_cast<std::chrono::milliseconds>(
      now_time.time_since_epoch()).count()));
}

#if 0 // method is replaced with CallsQueueScheduler:
void runAfter(const std::chrono::milliseconds& ms, std::function<void()> cb)
{    
 // std::cout << "SOLVER> Before calback" << std::endl;
  const auto tp = std::chrono::system_clock::now() + ms;
  std::thread tr([tp, cb]() {

    std::this_thread::sleep_until(tp);
  //  LOG_WARN("Inserting callback");
    CallsQueue::instance().insert(cb);
  });
 // std::cout << "SOLVER> After calback" << std::endl;
  tr.detach();
}
#endif // 0

#if defined(SPAM_MAIN) || defined(SPAMMER)
static int
randFT(int min, int max)
{
  return rand() % (max - min + 1) + min;
}
#endif
} // anonimous namespace

namespace Credits {
using ScopedLock = std::lock_guard<std::mutex>;
constexpr short min_nodes = 3;

Solver::Solver(Node* node)
  : node_(node)
  , generals(std::unique_ptr<Generals>(new Generals()))
  , vector_datas()
  , m_pool()
  , v_pool()
  , b_pool()
{}

Solver::~Solver()
{
	// stop possible deferred calls
    calls_scheduler.Stop();
	//		csconnector::stop();
  //		csstats::stop();
}

void Solver::set_keys(const std::vector<uint8_t>& pub, const std::vector<uint8_t>& priv)
{
	myPublicKey = pub;
	myPrivateKey = priv;
}
#pragma endregion

void Solver::buildBlock(csdb::Pool& block)
{
  csdb::Transaction transaction;

  transaction.set_target(csdb::Address::from_string("0000000000000000000000000000000000000000000000000000000000000003"));
  transaction.set_source(csdb::Address::from_string("0000000000000000000000000000000000000000000000000000000000000002"));

  transaction.set_currency(csdb::Currency("CS"));
  transaction.set_amount(csdb::Amount(10, 0));
  transaction.set_balance(csdb::Amount(100, 0));
  transaction.set_innerID(0);

  block.add_transaction(transaction);

  transaction.set_target(csdb::Address::from_string("0000000000000000000000000000000000000000000000000000000000000004"));
  transaction.set_source(csdb::Address::from_string("0000000000000000000000000000000000000000000000000000000000000002"));

  transaction.set_currency(csdb::Currency("CS"));
  transaction.set_amount(csdb::Amount(10, 0));
  transaction.set_balance(csdb::Amount(100, 0));
  transaction.set_innerID(0);

  block.add_transaction(transaction);

}

#pragma region moved to solver2
void Solver::prepareBlockForSend(csdb::Pool& block)
{
  //std::cout << "SOLVER> Before time stamp" << std::endl;
//block is build in buildvector
  addTimestampToPool(block);
  //std::cout << "SOLVER> Before write pub key" << std::endl;
  block.set_writer_public_key(myPublicKey);
   //std::cout << "SOLVER> Before write last sequence" << std::endl;
  block.set_sequence((node_->getBlockChain().getLastWrittenSequence()) + 1);
  csdb::PoolHash prev_hash;
  prev_hash.from_string("");
  block.set_previous_hash(prev_hash);
 // std::cout << "SOLVER> Before private key" << std::endl;
  block.sign(myPrivateKey);
#ifdef MYLOG
  std::cout << "last sequence: " << (node_->getBlockChain().getLastWrittenSequence()) << std::endl;// ", last time:" << node_->getBlockChain().loadBlock(node_->getBlockChain().getLastHash()).user_field(0).value<std::string>().c_str() 
  std::cout << "prev_hash: " << node_->getBlockChain().getLastHash().to_string() << " <- Not sending!!!" << std::endl;
  std::cout << "new sequence: " << block.sequence() << ", new time:" << block.user_field(0).value<std::string>().c_str() << std::endl;
  #endif
}

void Solver::sendTL()
{
  if (gotBigBang) return;
  uint32_t tNum = v_pool.transactions_count();
  std::cout << "AAAAAAAAAAAAAAAAAAAAAAAA -= TRANSACTION RECEIVING IS OFF =- AAAAAAAAAAAAAAAAAAAAAAAAAAAA" << std::endl;
 // std::cout << "                          Total received " << tNum << " transactions" << std::endl;
  std::cout << "========================================================================================" << std::endl;
  m_pool_closed = true;  
  std::cout << "Solver -> Sending " << tNum << " transactions " << std::endl;
  v_pool.set_sequence(node_->getRoundNumber());
  //std::cout << "Solver -> Sending TransactionList to ALL" << std::endl;//<< byteStreamToHex(it.str, 32)  //<< 
  node_->sendTransactionList(std::move(v_pool)); // Correct sending, better when to all one time

}
#pragma endregion

size_t Solver::getTLsize() const
{
  return v_pool.transactions_count();
}


void Solver::setLastRoundTransactionsGot(size_t trNum)
{
  lastRoundTransactionsGot = trNum;
}

#pragma region moved to solver2
void Solver::closeMainRound()
{
  if (node_->getRoundNumber()==1)// || (lastRoundTransactionsGot==0)) //the condition of getting 0 transactions by previous main node should be added!!!!!!!!!!!!!!!!!!!!!
  //node_->sendFirstTransaction();
  
  {
      node_->becomeWriter();
#ifdef MYLOG
	std::cout << "Solver -> Node Level changed 2 -> 3" << std::endl;
  #endif
#ifdef SPAM_MAIN
    createSpam = false;
    spamThread.join();
    prepareBlockForSend(testPool);
    node_->sendBlock(testPool);
#else
    prepareBlockForSend(m_pool);

    b_pool.set_sequence((node_->getBlockChain().getLastWrittenSequence()) + 1);
    csdb::PoolHash prev_hash;
    prev_hash.from_string("");
    b_pool.set_previous_hash(prev_hash);
	
    std::cout << "Solver -> new sequence: " << m_pool.sequence() << ", new time:" << m_pool.user_field(0).value<std::string>().c_str() << std::endl;
 
    node_->sendBlock(std::move(m_pool));
    node_->sendBadBlock(std::move(b_pool));
	std::cout << "Solver -> Block is sent ... awaiting hashes" << std::endl;
#endif
	node_->getBlockChain().setGlobalSequence(m_pool.sequence());
#ifdef MYLOG
	std::cout << "Solver -> Global Sequence: "  << node_->getBlockChain().getGlobalSequence() << std::endl;
	std::cout << "Solver -> Writing New Block"<< std::endl;
  #endif
    node_->getBlockChain().putBlock(m_pool);
    }
}

bool Solver::mPoolClosed()
{
  return m_pool_closed;
}

void Solver::runMainRound()
{
    if(timer_used) {
        timer_service.TimeConsoleOut("runMainRound()", node_->getRoundNumber());
    }
  m_pool_closed = false;
  std::cout << "========================================================================================" << std::endl;
  std::cout << "VVVVVVVVVVVVVVVVVVVVVVVVV -= TRANSACTION RECEIVING IS ON =- VVVVVVVVVVVVVVVVVVVVVVVVVVVV" << std::endl;
 
  uint32_t duration_main_round = T_coll_trans;
  if(node_->getRoundNumber()==1) 
  {
	  // original logic: 2000 msec for 1st round
	  duration_main_round = 2000;
  }
  scheduleCloseMainRound(duration_main_round);
}
#pragma endregion

const HashVector& Solver::getMyVector() const
{
  return hvector;
}

const HashMatrix& Solver::getMyMatrix() const
{
 return (generals->getMatrix());
}

#pragma region moved to solver2

void Solver::flushTransactions()
{
	if (node_->getMyLevel() != NodeLevel::Normal) {
		return;
	}
	{
		std::lock_guard<std::mutex> l(m_trans_mut);
		if (m_transactions.size()) {
			node_->sendTransaction(std::move(m_transactions));
			sentTransLastRound = true;
			m_transactions.clear();
		}
		else {
			return;
		}
	}
}
#pragma endregion

bool Solver::getIPoolClosed() {
  return m_pool_closed;
}
#pragma region moved to solver2
void Solver::gotTransaction(csdb::Transaction&& transaction)
{
#ifdef MYLOG
	//std::cout << "SOLVER> Got Transaction" << std::endl;
#endif
	if (m_pool_closed)
	{
#ifdef MYLOG
		LOG_EVENT("m_pool_closed already, cannot accept your transactions");
#endif
		return;
	}

	if (transaction.is_valid())
	{
#ifndef SPAMMER
		std::vector<uint8_t>	message		= transaction.to_byte_stream_for_sig();
		std::vector<uint8_t>	public_key	= transaction.source().public_key();
		std::string				signature	= transaction.signature();

		if (verify_signature((uint8_t *)signature.data(), public_key.data(), message.data(), message.size()))
		{
#endif
			v_pool.add_transaction(transaction);
#ifndef SPAMMER
		}
		else
		{
			LOG_EVENT("Wrong signature");
		}
#endif
	}
#ifdef MYLOG
	else
	{
		LOG_EVENT("Invalid transaction received");
	}
#endif
}

void Solver::initConfRound()
{
    if(timer_used) {
        timer_service.TimeConsoleOut("initConfRound()", node_->getRoundNumber());
    }
  memset(receivedVecFrom, 0, 100);
  memset(receivedMatFrom, 0, 100);
  trustedCounterVector = 0;
  trustedCounterMatrix = 0;
  if (gotBigBang) sendZeroVector();
  //runAfter(std::chrono::milliseconds(TIME_TO_AWAIT_ACTIVITY),
  //  [this, _rNum]() { if(!transactionListReceived) node_->sendTLRequest(_rNum); });
}

void Solver::gotTransactionList(csdb::Pool&& _pool)
{
    cancelReqTransactionList();
    if(timer_used) {
        timer_service.TimeConsoleOut("gotTransactionList()", node_->getRoundNumber());
    }
    if(transactionListReceived) return;
    transactionListReceived = true;
    uint8_t numGen = node_->getConfidants().size();
    //	std::cout << "SOLVER> GotTransactionList" << std::endl;
    m_pool = csdb::Pool {};
    Hash_ result = generals->buildvector(_pool, m_pool, node_->getConfidants().size(), b_pool);
    receivedVecFrom [node_->getMyConfNumber()] = true;
    hvector.Sender = node_->getMyConfNumber();
    hvector.hash = result;
    receivedVecFrom [node_->getMyConfNumber()] = true;
    generals->addvector(hvector);
    node_->sendVector(std::move(hvector));
    if(timer_used) {
        timer_service.TimeConsoleOut("gotTransactionList(): vector sent", node_->getRoundNumber());
    }
    trustedCounterVector++;
    if(trustedCounterVector == numGen)
    {
        vectorComplete = true;

    memset(receivedVecFrom, 0, 100);
    trustedCounterVector = 0;
    //compose and send matrix!!!
    //receivedMat_ips.insert(node_->getMyId());
    generals->addSenderToMatrix(node_->getMyConfNumber());
    receivedMatFrom[node_->getMyConfNumber()] = true;
    ++trustedCounterMatrix;
    node_->sendMatrix(generals->getMatrix());
    generals->addmatrix(generals->getMatrix(), node_->getConfidants());//MATRIX SHOULD BE DECOMPOSED HERE!!!
#ifdef MYLOG
        std::cout << "SOLVER> Matrix added" << std::endl;
#endif
        if(timer_used) {
            timer_service.TimeConsoleOut("gotTransactionList(): matrix added and sent", node_->getRoundNumber());
        }
    }
    // for T-nodes: track vectors and matrices received:
    if(node_->getMyLevel() == NodeLevel::Confidant) {
        scheduleReqVectors(T_vec);
        scheduleReqMatrices(T_mat);
  }
}
#pragma endregion

void Solver::sendZeroVector()
{

  if (transactionListReceived && !getBigBangStatus()) return;
  std::cout << "SOLVER> Generating ZERO TransactionList" << std::endl;
  csdb::Pool test_pool = csdb::Pool{};
  gotTransactionList(std::move(test_pool));

}

#pragma region moved to solver2
void Solver::gotVector(HashVector&& vector)
{
#ifdef MYLOG
	std::cout << "SOLVER> GotVector" << std::endl;
  #endif
 // runAfter(std::chrono::milliseconds(200),
 //   [this]() { sendZeroVector(); });

  uint8_t numGen = node_->getConfidants().size();
  //if (vector.roundNum==node_->getRoundNumber())
  //{
	 // std::cout << "SOLVER> This is not the information of this round" << std::endl;
	 // return;
  //}
  if (receivedVecFrom[vector.Sender]==true)
  {
#ifdef MYLOG
		std::cout << "SOLVER> I've already got the vector from this Node" << std::endl;
    #endif
		return;
  }
  receivedVecFrom[vector.Sender] = true;
  generals->addvector(vector);//building matrix
  trustedCounterVector++;

  if(timer_used) {
      timer_service.TimeConsoleOut("gotVector(): new vector", node_->getRoundNumber());
  }
  if (trustedCounterVector == numGen)
  {
      if(timer_used) {
          timer_service.TimeConsoleOut("gotVector(): all received", node_->getRoundNumber());
      }
      cancelReqVectors();
	  //std::cout << "SOLVER> GotVector : " << std::endl;
    vectorComplete = true;

	  memset(receivedVecFrom, 0, 100);
	  trustedCounterVector = 0;
	  //compose and send matrix!!!
      //receivedMat_ips.insert(node_->getMyId());
	  generals->addSenderToMatrix(node_->getMyConfNumber());
	  receivedMatFrom[node_->getMyConfNumber()] = true;
	  trustedCounterMatrix++;
	  node_->sendMatrix(generals->getMatrix());
	  generals->addmatrix(generals->getMatrix(), node_->getConfidants());//MATRIX SHOULD BE DECOMPOSED HERE!!!
      //   std::cout << "SOLVER> Matrix added" << std::endl;

    if (trustedCounterMatrix == numGen) takeDecWorkaround();
  }
#ifdef MYLOG
  std::cout << "Solver>  VECTOR GOT SUCCESSFULLY!!!" << std::endl;
  #endif
}

void Solver::takeDecWorkaround()
{
  memset(receivedMatFrom, 0, 100);
  trustedCounterMatrix = 0;
  uint8_t wTrusted = (generals->take_decision(node_->getConfidants(), node_->getMyConfNumber(), node_->getBlockChain().getHashBySequence(node_->getRoundNumber() - 1)));

  if(wTrusted == 100)
  {

      //        std::cout << "SOLVER> CONSENSUS WASN'T ACHIEVED!!!" << std::endl;
      scheduleWriteNewBlock(T_coll_trans);
  }
  else
  {
      consensusAchieved = true;
      //       std::cout << "SOLVER> wTrusted = " << (int)wTrusted << std::endl;
      if(wTrusted == node_->getMyConfNumber())
      {
          node_->becomeWriter();
          if(timer_used) {
              timer_service.TimeConsoleOut("Become writer", node_->getRoundNumber());
          }
          // cancel some scheduled calls then write block 
          cancelReqBlock(); // we will send block
          cancelReqRoundTable(); // we will send round table
          cancelReqTransactionList(); // not at this round
          cancelWriteNewBlock(); // direct call immediately:
          writeNewBlock();
          //scheduleWriteNewBlock(T_coll_trans);
      }
      //LOG_WARN("This should NEVER happen, NEVER");
  }
}

void Solver::gotMatrix(HashMatrix&& matrix)
{
	uint8_t numGen = node_->getConfidants().size();

  if(gotBlockThisRound) return;
	if (receivedMatFrom[matrix.Sender])
	{
#ifdef MYLOG
		std::cout << "SOLVER> I've already got the matrix from this Node" << std::endl;
#endif
		return;
	}
	receivedMatFrom[matrix.Sender] = true;
	trustedCounterMatrix++;
	generals->addmatrix(matrix, node_->getConfidants());
#ifdef MYLOG
  std::cout << "SOLVER> Matrix added" << std::endl;
#endif
  if(trustedCounterMatrix == numGen) {
      cancelReqMatrices();
      takeDecWorkaround();
  }
}

//what block does this function write???
void Solver::writeNewBlock()
{
#ifdef MYLOG
	std::cout << "Solver -> writeNewBlock ... start";
  #endif
  if (consensusAchieved &&
    node_->getMyLevel() == NodeLevel::Writer) {
    prepareBlockForSend(m_pool);
    node_->sendBlock(std::move(m_pool));
    node_->getBlockChain().putBlock(m_pool);
    node_->getBlockChain().setGlobalSequence(m_pool.sequence());
    b_pool.set_sequence((node_->getBlockChain().getLastWrittenSequence()) + 1);
    csdb::PoolHash prev_hash;
    prev_hash.from_string("");
    b_pool.set_previous_hash(prev_hash);

#ifdef MYLOG
	  std::cout << "Solver -> writeNewBlock ... finish" << std::endl;
#endif
	  consensusAchieved = false;
      if(timer_used) {
          timer_service.TimeConsoleOut("writeNewBlock(): done", node_->getRoundNumber());
      }
      scheduleReqHashes(T_hash);
  }
  else {
    //LOG_WARN("Consensus achieved: " << (consensusAchieved ? 1 : 0) << ", ml=" << (int)node_->getMyLevel());
  }
}

void Solver::gotBlock(csdb::Pool&& block, const PublicKey& sender)
{
	if (node_->getMyLevel() == NodeLevel::Writer)
		return;

    cancelReqBlock();
    if(timer_used) {
        timer_service.TimeConsoleOut("gotBlock()", node_->getRoundNumber());
    }
  gotBigBang = false;
  gotBlockThisRound = true;
#ifdef MONITOR_NODE
  addTimestampToPool(block);
#endif
  uint32_t g_seq = block.sequence();
#ifdef MYLOG
  std::cout << "GOT NEW BLOCK: global sequence = " << g_seq << std::endl;
  #endif
  if(g_seq > node_->getRoundNumber()) return; // remove this line when the block candidate signing of all trusted will be implemented
  
  node_->getBlockChain().setGlobalSequence(g_seq);
  if (g_seq == node_->getBlockChain().getLastWrittenSequence() + 1)
  {
		std::cout << "Solver -> getblock calls writeLastBlock" << std::endl;		if(block.verify_signature()) //INCLUDE SIGNATURES!!!
		{
      node_->getBlockChain().putBlock(block);
#ifndef MONITOR_NODE
		  if ((node_->getMyLevel() != NodeLevel::Writer) && (node_->getMyLevel() != NodeLevel::Main))
		  {
              if(timer_used) {
                  timer_service.TimeConsoleOut("gotBlock(): send hash", node_->getRoundNumber());
              }
			  //std::cout << "Solver -> before sending hash to writer" << std::endl;
			  Hash test_hash((char*)(node_->getBlockChain().getLastWrittenHash().to_binary().data()));//getLastWrittenHash().to_binary().data()));//SENDING HASH!!!
			  node_->sendHash(test_hash, sender);
#ifdef MYLOG
        std::cout << "SENDING HASH: " << byteStreamToHex(test_hash.str,32) << std::endl;
#endif
		  }
#endif
    }

		//std::cout << "Solver -> finishing gotBlock" << std::endl;
  }

  auto rnum = node_->getRoundNumber();
    scheduleReqRoundTable(T_rt, rnum);
    // for T-nodes: also request transaction list starting from the 2nd round
    if(rnum >= 2 && node_->getMyLevel() == NodeLevel::Confidant) {
        scheduleReqTransactionList(T_tl);
    }
}
#pragma endregion

bool Solver::getBigBangStatus()
{
  return gotBigBang;
}

#pragma region moved to solver2
void Solver::setBigBangStatus(bool _status)
{
  gotBigBang = _status;
  if(timer_used) {
      if(_status) {
          timer_service.TimeConsoleOut("BIGBANG status is ON", node_->getRoundNumber());
      }
      else {
          timer_service.TimeConsoleOut("BIGBANG status is OFF", node_->getRoundNumber());
      }
  }
}
#pragma endregion

void Solver::gotBadBlockHandler(csdb::Pool&& _pool, const PublicKey& sender)
{
  //insert code here
}

void Solver::gotBlockCandidate(csdb::Pool&& block)
{
#ifdef MYLOG
	std::cout << "Solver -> getBlockCanditate" << std::endl;
  #endif
  if (blockCandidateArrived)
    return;

  //m_pool = std::move(block);

  blockCandidateArrived = true;
 // writeNewBlock();
}

#pragma region moved to solver 2
void Solver::gotHash(const Hash& hash, const PublicKey& sender)
{
	if (round_table_sent) return;

	//std::cout << "Solver -> gotHash: " << hash.to_string() << "from sender: " << sender.to_string() << std::endl;//<-debug feature
	Hash myHash((char*)(node_->getBlockChain().getLastWrittenHash().to_binary().data()));
#ifdef MYLOG
	std::cout << "Solver -> My Hash: " << byteStreamToHex(myHash.str,32) << std::endl;
#endif
	size_t ips_size = ips.size();
	if (ips_size <= min_nodes)
	{
		if (hash == myHash) 
		{
#ifdef MYLOG
			std::cout << "Solver -> Hashes are good" << std::endl;
#endif
			//hashes.push_back(hash);
			ips.push_back(sender);

            if(timer_used) {
                timer_service.TimeConsoleOut("gotHash(): accept", node_->getRoundNumber());
            }
		}
		else
		{
#ifdef MYLOG
			if (hash != myHash) std::cout << "Hashes do not match!!!" << std::endl;
      #endif
			return;
		}
	}
	else
	{
#ifdef MYLOG
		std::cout << "Solver -> We have enough hashes!" << std::endl;
    #endif
		return;
	}

	
	if ((ips_size == min_nodes) && (!round_table_sent))
	{
		
#ifdef MYLOG
    std::cout << "Solver -> sending NEW ROUND table" << std::endl;
#endif
    node_->initNextRound(node_->getMyPublicKey(), std::move(ips));
		round_table_sent = true;

        if(timer_used) {
            timer_service.TimeConsoleOut("gotHash(): new RT sent", node_->getRoundNumber());
        }
        cancelReqHashes();
	}
  }
#pragma endregion
#if 0 // unused code
void Solver::initApi()
{
  _initApi();
}

void Solver::_initApi()
{
  //        csconnector::start(&(node_->getBlockChain()),csconnector::Config{});
  //
  //		csstats::start(&(node_->getBlockChain()));
}
#endif // 0
  /////////////////////////////

#ifdef SPAM_MAIN
void
Solver::createPool()
{
  std::string mp = "0123456789abcdef";
  const unsigned int cmd = 6;

  struct timeb tt;
  ftime(&tt);
  srand(tt.time * 1000 + tt.millitm);

  testPool = csdb::Pool();

  std::string aStr(64, '0');
  std::string bStr(64, '0');

  uint32_t limit = randFT(5, 15);

  if (randFT(0, 150) == 42) {
    csdb::Transaction smart_trans;
    smart_trans.set_currency(csdb::Currency("CS"));

    smart_trans.set_target(Credits::BlockChain::getAddressFromKey(
      "3SHCtvpLkBWytVSqkuhnNk9z1LyjQJaRTBiTFZFwKkXb"));
    smart_trans.set_source(csdb::Address::from_string(
      "0000000000000000000000000000000000000000000000000000000000000001"));

    smart_trans.set_amount(csdb::Amount(1, 0));
    smart_trans.set_balance(csdb::Amount(100, 0));

    api::SmartContract sm;
    sm.address = "3SHCtvpLkBWytVSqkuhnNk9z1LyjQJaRTBiTFZFwKkXb";
    sm.method = "store_sum";
    sm.params = { "123", "456" };

    smart_trans.add_user_field(0, serialize(sm));

    testPool.add_transaction(smart_trans);
  }

  csdb::Transaction transaction;
  transaction.set_currency(csdb::Currency("CS"));

  while (createSpam && limit > 0) {
    for (size_t i = 0; i < 64; ++i) {
      aStr[i] = mp[randFT(0, 15)];
      bStr[i] = mp[randFT(0, 15)];
    }

    transaction.set_target(csdb::Address::from_string(aStr));
    transaction.set_source(csdb::Address::from_string(bStr));

    transaction.set_amount(csdb::Amount(randFT(1, 1000), 0));
    transaction.set_balance(
      csdb::Amount(transaction.balance().integral() + 1, 0));

    testPool.add_transaction(transaction);
    --limit;
  }

  addTimestampToPool(testPool);
}
#endif

#pragma region moved to solver2
#ifdef SPAMMER
void
Solver::spamWithTransactions()
{
	//if (node_->getMyLevel() != Normal) return;
  std::cout << "STARTING SPAMMER..." << std::endl;
  std::string mp = "1234567890abcdef";
  
  // std::string cachedBlock;
  // cachedBlock.reserve(64000);
  uint64_t iid=0;
  std::this_thread::sleep_for(std::chrono::seconds(5));

  auto aaa = csdb::Address::from_string(
    "0000000000000000000000000000000000000000000000000000000000000001");
  auto bbb = csdb::Address::from_string(
    "0000000000000000000000000000000000000000000000000000000000000002");

  csdb::Transaction transaction;
  transaction.set_target(aaa);
  transaction.set_source(
    csdb::Address::from_public_key((char*)myPublicKey.data()));
  //transaction.set_max_fee();

  transaction.set_currency(csdb::Currency("CS"));

  while (true) {
    if (spamRunning && (node_->getMyLevel() == Normal))
    {
      if ((node_->getRoundNumber()<10) || (node_->getRoundNumber() > 20) )
      {
      

          transaction.set_amount(csdb::Amount(randFT(1, 1000), 0));
          transaction.set_max_fee(csdb::Amount(0, 1,10));
          transaction.set_balance(csdb::Amount(transaction.amount().integral() + 2, 0));
          transaction.set_innerID(iid);
  #ifdef MYLOG
         // std::cout << "Solver -> Transaction " << iid << " added" << std::endl;
          #endif
          {
          std::lock_guard<std::mutex> l(m_trans_mut);
          m_transactions.push_back(transaction);
          }
          iid++;
      }
    }

    std::this_thread::sleep_for(std::chrono::microseconds(TRX_SLEEP_TIME));
  }
}
#endif
///////////////////
void Solver::send_wallet_transaction(const csdb::Transaction& transaction)
{
  //TRACE("");
  std::lock_guard<std::mutex> l(m_trans_mut);
  //TRACE("");
  m_transactions.push_back(transaction);
}

void Solver::addInitialBalance()
{
  std::cout << "===SETTING DB===" << std::endl;
  const std::string start_address =
    "0000000000000000000000000000000000000000000000000000000000000002";

  //csdb::Pool pool;
  csdb::Transaction transaction;
  transaction.set_target(
    csdb::Address::from_public_key((char*)myPublicKey.data()));
  transaction.set_source(csdb::Address::from_string(start_address));

  transaction.set_currency(csdb::Currency("CS"));
  transaction.set_amount(csdb::Amount(10000, 0));
  transaction.set_balance(csdb::Amount(10000000, 0));
  transaction.set_innerID(1);

  {
	  std::lock_guard<std::mutex> l(m_trans_mut);
	  m_transactions.push_back(transaction);
  }

#ifdef SPAMMER
  spamThread = std::thread(&Solver::spamWithTransactions, this);
  spamThread.detach();
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////// gotBlockRequest
void Solver::gotBlockRequest(csdb::PoolHash&& hash, const PublicKey& nodeId) {
	csdb::Pool pool = node_->getBlockChain().loadBlock(hash);
	if (pool.is_valid())
	{
		csdb::PoolHash prev_hash;
		prev_hash.from_string("");
		pool.set_previous_hash(prev_hash);
		node_->sendBlockReply(std::move(pool), nodeId);
	}

}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////// gotBlockReply
void Solver::gotBlockReply(csdb::Pool&& pool) {
#ifdef MYLOG
	std::cout << "Solver -> Got Block for my Request: " << pool.sequence() << std::endl;
  #endif
	if (pool.sequence() == node_->getBlockChain().getLastWrittenSequence() + 1)
		node_->getBlockChain().putBlock(pool);
    
	

}
#pragma endregion

void Solver::addConfirmation(uint8_t confNumber_) {
  if(writingConfGotFrom[confNumber_]) return;
  writingConfGotFrom[confNumber_]=true;
  writingCongGotCurrent++;
  if(writingCongGotCurrent==2)
  {
    node_->becomeWriter();

    scheduleWriteNewBlock(T_coll_trans);
  } 
  
}

#pragma region moved to solver2
void Solver::beforeNextRound()
{
    auto rnum = node_->getRoundNumber();
	if (rnum == 0) {
		// actual before the first round
        if(timer_used) {
            timer_service.Reset();
        }
		return;
	}

	// moved from begin of node::onRoundStart() from where this method called
	if ((!mPoolClosed()) && (!getBigBangStatus()))
	{
		sendTL();
	}

    if(timer_used) {
        timer_service.TimeConsoleOut("beforeNextRound()", rnum);
        timer_service.TimeConsoleOut("Cancel all sheduled calls", rnum);
    }
    // cancel all scheduled calls if any:
    calls_scheduler.RemoveAll();
    doSelfTest();
	// get duration of finishing round, independent from timer_used value
	passedRoundsDuration += timer_service.Time();
	passedRoundsCount++;
    timer_service.Reset();
}

void Solver::nextRound()
{
    if(timer_used) {
        timer_service.TimeConsoleOut("nextRound()", node_->getRoundNumber());
    }
#ifdef MYLOG
	std::cout << "SOLVER> next Round : Starting ... nextRound" << std::endl;
  #endif
  receivedVec_ips.clear();
  receivedMat_ips.clear();

  hashes.clear();
  ips.clear();
  vector_datas.clear();

  vectorComplete = false;
  consensusAchieved = false;
  blockCandidateArrived = false;
  transactionListReceived = false;
  vectorReceived = false;
  gotBlockThisRound=false;
  allMatricesReceived = false;

  round_table_sent = false;
  sentTransLastRound = false;
  m_pool = csdb::Pool{};
  if(m_pool_closed) v_pool = csdb::Pool{};
#ifdef MYLOG
  std::cout << "SOLVER> next Round : the variables initialized" << std::endl;
  #endif
  auto lvl = node_->getMyLevel();
  if(lvl == NodeLevel::Confidant) {
      initConfRound();
  }
  if (lvl == NodeLevel::Main) {
    runMainRound();
#ifdef SPAM_MAIN
    createSpam = true;
    spamThread = std::thread(&Solver::createPool, this);
#endif
#ifdef SPAMMER
    spamRunning = false;
#endif
  } else {
#ifdef SPAMMER
    spamRunning = true;
#endif
  //  std::cout << "SOLVER> next Round : before flush transactions" << std::endl;
    m_pool_closed = true;
  }
  // original logic: only normal nodes call flushTRansactions()
  if (lvl == NodeLevel::Normal) {
      scheduleFlushTransactions(T_flush_trans);
  }
  else {
      cancelFlushTransactions();
  }

  // for every node type (N, G, T, W): track round duration
  // calculate new desired round duration as average duration of previous ones
  uint32_t desired_round_duration = T_round;
  if(passedRoundsCount > 0) {
      desired_round_duration = passedRoundsDuration / passedRoundsCount;
      if(desired_round_duration < T_coll_trans) {
          // duration too small
          desired_round_duration = T_coll_trans;
      }
  }
  scheduleOnRoundExpired(desired_round_duration);
  // for (N, G, T) node types: track block received
  scheduleReqBlock(T_blk);
}
bool Solver::verify_signature(uint8_t signature[64], uint8_t public_key[32],
									uint8_t* message, size_t message_len)
{
	// if crypto_sign_ed25519_verify_detached(...) returns 0 - succeeded, 1 - failed
	return !crypto_sign_ed25519_verify_detached(signature, message, message_len, public_key);
}
#pragma endregion
} // namespace Credits

