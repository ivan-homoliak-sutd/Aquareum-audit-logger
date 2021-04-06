#include "operator.h"

#include <mutex>

using namespace aql;

Dispatcher::Dispatcher(oe_enclave_t* _enclave, aql::Operator* _operator)
{
    this->enclave = _enclave;
    this->op = _operator;
}

Dispatcher::~Dispatcher()
{
    // TODO delete txs in queue
}

void Dispatcher::threadExecute()
{
    // consumer
    eevm::PersistantTransaction* tx = NULL;
    uint256_t output_u256;


    while (1) {
        // locking mechanism
        std::unique_lock<std::mutex> locker(this->mtx);
        this->cond.wait(locker, [&]() { return !txs.empty(); });
        debug_print("Dispatcher: Got new TX");

        tx = this->txs.front();
        this->txs.pop();
        locker.unlock();

        debug_print("Dispatcher: before tx execution");
        this->op->_dispatchTX(this->enclave, tx, output_u256);
        debug_print("Dispatcher: tx was executed");

        delete tx;
    }
}

int Dispatcher::addToDispatch(eevm::PersistantTransaction* tx)
{
    if (this->validTx(tx) != RET_SUCCESS) {
        delete tx;
        return 1;
    }

    // producer
    std::unique_lock<std::mutex> locker(this->mtx);
    this->txs.push(tx);
    locker.unlock();
    this->cond.notify_one();

    return RET_SUCCESS;
}

int Dispatcher::validTx(eevm::PersistantTransaction* tx)
{
    // sender exists
    // if (this->op->m_accounts.find(tx->origin) != this->op->m_accounts.end())
    // {
    //     debug_print("%%%%%%%%% ACCOUNT");
    // } else if (this->op->m_contracts.find(tx->origin) != this->op->m_contracts.end()) {
    //     debug_print("%%%%%%%%% CONTRACT");
    // } else {
    //     debug_print("%%%%%%%%% ERROR");
    //     return 1;
    // }
    
    // destination exists

    return RET_SUCCESS;
}
