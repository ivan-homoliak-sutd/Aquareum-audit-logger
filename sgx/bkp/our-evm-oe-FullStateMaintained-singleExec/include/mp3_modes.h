#pragma once

// Possible modes of storing/transferring MP3 state in/to enclave
enum class MODE {
    FullStateMaintained = 0,
    FullStateTransfer,
    PartialStateTransfer,
    PartialStateTransferCaching,
};