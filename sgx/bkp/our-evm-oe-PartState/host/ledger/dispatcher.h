#ifndef DISPATCHER_H
#define DISPATCHER_H

#include "eEVM/transaction.h"
#include "operator.h"
#include "../utils.h"

#include <condition_variable>
#include <mutex>
#include <vector>

namespace aql
{
    class Operator;

    class Dispatcher {
    private:
    public:
        std::vector<eevm::PersistantTransaction*> txs;

        std::mutex mtx;
        std::condition_variable cond;

        oe_enclave_t* enclave;
        aql::Operator* op;

        Dispatcher(oe_enclave_t* _enclave, aql::Operator* _operator);
        ~Dispatcher();
        void threadExecute();
        void addToDispatch(eevm::PersistantTransaction* tx);
    };

}  // namespace aql

#endif
