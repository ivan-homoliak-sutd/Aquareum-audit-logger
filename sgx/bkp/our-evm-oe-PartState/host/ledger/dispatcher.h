#ifndef DISPATCHER_H
#define DISPATCHER_H

#include "eEVM/transaction.h"
#include "eEVM/util.h"
#include "operator.h"
#include "utils.h"

#include <condition_variable>
#include <mutex>
#include <openenclave/host.h>
#include <queue>
#include <stdio.h>
#include <unistd.h>

namespace aql
{
    class Operator;

    class Dispatcher {
    private:
    public:
        std::queue<eevm::PersistantTransaction*> txs;
        std::queue<int> numbers;

        std::mutex mtx;
        std::condition_variable cond;

        oe_enclave_t* enclave;
        aql::Operator* op;

        int i = 0;
        Dispatcher(oe_enclave_t* _enclave, aql::Operator* _operator);  //, aql::Operator* _operator
        ~Dispatcher();
        void threadExecute();
        int addToDispatch(eevm::PersistantTransaction* tx);
        int validTx(eevm::PersistantTransaction* tx);
    };

    typedef enum {
        send = 0,
        recv
    } IomcType;

}  // namespace aql

#endif
