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
    // producer
    std::unique_lock<std::mutex> locker(this->mtx);
    this->txs.push(tx);
    locker.unlock();
    this->cond.notify_one();

    return 0;
}
