#include <smartcontracts.hpp>

#include <csdb/transaction.hpp>

#include <sstream>

namespace cs
{

  /*explicit*/
  SmartContracts::SmartContracts(BlockChain& blockchain)
    : bc(blockchain)
  {}

  /*static*/
  bool SmartContracts::is_smart_contract(const csdb::Transaction tr)
  {
    // see apihandler.cpp #319:
    //csdb::UserField uf = tr.user_field(0);
    //return uf.type() == csdb::UserField::Type::String;

    csdb::UserField f = tr.user_field(trx_uf::deploy::Code);
    return f.is_valid() && f.type() == csdb::UserField::Type::String;
  }

  /*static*/
  std::optional<api::SmartContractInvocation> SmartContracts::get_smart_contract(const csdb::Transaction tr)
  {
    const auto& smart_fld = tr.user_field(trx_uf::deploy::Code); // see apihandler.cpp near #494
    if(smart_fld.is_valid()) {
      return deserialize<api::SmartContractInvocation>(smart_fld.value<std::string>());
    }
    return std::nullopt;
  }

  /*static*/
  bool SmartContracts::is_deploy(const csdb::Transaction tr)
  {
    //TODO: correctly define tx type

    // see apihandler.cpp #319:
    if(!is_smart_contract(tr)) {
      return false;
    }
    auto contract = get_smart_contract(tr);
    if(!contract.has_value()) {
      return false;
    }
    return contract.value().method.empty();
  }

  /*static*/
  bool SmartContracts::is_start(const csdb::Transaction tr)
  {
    //TODO: correctly define tx type
    if(!is_smart_contract(tr)) {
      return false;
    }
    if(is_new_state(tr)) {
      return false;
    }
    if(is_deploy(tr)) {
      return false;
    }
    return true;
  }

  /*static*/
  bool SmartContracts::is_new_state(const csdb::Transaction tr)
  {
    //TODO: correctly define tx type
    if(!is_smart_contract(tr)) {
      return false;
    }
    return tr.user_field_ids().size() == trx_uf::new_state::Count;
  }

  csdb::Transaction SmartContracts::get_transaction(const SmartContractRef& ref)
  {
    csdb::Pool block = bc.loadBlock(ref.hash);
    if(!block.is_valid()) {
      return csdb::Transaction {};
    }
    if(ref.transaction >= block.transactions_count()) {
      return csdb::Transaction {};
    }
    return block.transactions().at(ref.transaction);
  }

  std::pair<SmartContractStatus, const SmartContractRef&> SmartContracts::enqueue(
    const csdb::PoolHash blk_hash, csdb::Pool::sequence_t blk_seq, size_t trx_idx, cs::RoundNumber round)
  {
    SmartContractRef new_item { blk_hash, blk_seq, trx_idx };
    SmartContractStatus new_status = SmartContractStatus::Running;
    auto it = exe_queue.cbegin();
    if(!exe_queue.empty()) {
      for(; it != exe_queue.cend(); ++it) {
        // test the same contract
        if(it->contract == new_item) {
          cserror() << "Smarts: attempt to queue duplicated contract transaction, already queued on round #" << it->round;
          return std::make_pair(it->status, it->contract);
        }
      }
      // only the 1st item currently is allowed to execute
      new_status = SmartContractStatus::Waiting;
      csdebug() << "Smarts: enqueue contract for execution";
    }
    else {
      csdebug() << "Smarts: starting contract execution";
    }
    // enqueue to end
    const auto& ref = exe_queue.emplace_back(QueueItem { new_item, new_status, round });
    return std::make_pair(ref.status, ref.contract);
  }

} // cs