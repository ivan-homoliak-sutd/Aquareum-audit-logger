#pragma once

#include "address.h"
#include "bigint.h"
// #include "exception.h"
// #include "util.h"

// #include <fmt/format_header_only.h>
#include <vector>

namespace ecl
{

  class Blockchain
  {
  private:
    uint256_t LRoot; // root of the history tree that aggregates all blocks of the ledger L

  public:
    Blockchain();



  };


} // namespace ecl
