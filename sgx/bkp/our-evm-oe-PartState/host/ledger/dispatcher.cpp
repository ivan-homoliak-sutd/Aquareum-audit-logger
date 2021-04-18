#include "dispatcher.h"

using namespace aql;

Dispatcher::Dispatcher(oe_enclave_t* _enclave, aql::Operator* _operator)
{
    this->enclave = _enclave;
    this->op = _operator;
}

Dispatcher::~Dispatcher()
{
    for (auto tx : this->txs) {
        delete tx;
    }
}

void Dispatcher::threadExecute()
{
    // consumer
    while (1) {
        // locking mechanism
        std::unique_lock<std::mutex> locker(this->mtx);
        this->cond.wait(locker, [&]() { return !txs.empty(); });
        debug_print("Dispatcher: Got new TX: " + to_string(this->txs.size()));

        std::vector<eevm::PersistantTransaction*> batch_txs;
        this->txs.swap(batch_txs);

        locker.unlock();

        this->op->_dispatchManyTXs(this->enclave, batch_txs);

        for (auto tx : batch_txs) {
            delete tx;
        }
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
    this->txs.push_back(tx);
    locker.unlock();
    this->cond.notify_one();

    return RET_SUCCESS;
}

// TODO
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
