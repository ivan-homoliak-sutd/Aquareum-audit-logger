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
    while (true) {
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

void Dispatcher::addToDispatch(eevm::PersistantTransaction* tx)
{
    // producer
    std::unique_lock<std::mutex> locker(this->mtx);
    this->txs.push_back(tx);
    locker.unlock();
    this->cond.notify_one();
}
