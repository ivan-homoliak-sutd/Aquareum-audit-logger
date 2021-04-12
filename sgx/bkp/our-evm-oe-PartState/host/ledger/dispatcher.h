#ifndef DISPATCHER_H
#define DISPATCHER_H

#include "eEVM/transaction.h"
#include "operator.h"

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
        int addToDispatch(eevm::PersistantTransaction* tx);
        int validTx(eevm::PersistantTransaction* tx);
    };

    typedef enum {
        send = 0,
        recv
    } IomcType;

    typedef enum {
        sendInitialize = 3,
        sendCommit = 2,
        sendRevert = 4,
        receiveInitialize = 5,
        receiveClaim = 4,
        fund = 1
    } IomcFunctions;

}  // namespace aql

#endif
