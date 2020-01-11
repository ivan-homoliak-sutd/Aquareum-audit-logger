// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2014-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#pragma once

#include <Common.h>
// #include <Log.h>
#include <StateCacheDB.h>
#include <db.h>
#include <iostream>
#include <memory>

namespace dev
{
class OverlayDB : public StateCacheDB {
public:
    explicit OverlayDB(std::unique_ptr<db::DatabaseFace> _db = nullptr)
      : m_db(_db.release(), [](db::DatabaseFace* db) { // the lambda function is deleter of managed object (called when the last shared pointer is released)
            // if(VerbosityDebug == currentVerbosity)
            std::cerr << "overlaydb: " << "Closing MP3 database...\n";
            delete db;
        }) {}

    ~OverlayDB();

    // Copyable
    OverlayDB(OverlayDB const&) = default;
    OverlayDB& operator=(OverlayDB const&) = default;
    // Movable
    OverlayDB(OverlayDB&&) = default;
    OverlayDB& operator=(OverlayDB&&) = default;

    void commit();  // this methods commits data from state cache to the PERSISTANT DB in m_db. It stores entries from both 'm_main' and 'm_aux' !
    void rollback();

    std::string lookup(h256 const& _h) const;
    bool exists(h256 const& _h) const;
    void kill(h256 const& _h);

    bytes lookupAux(h256 const& _h) const;

    std::shared_ptr<db::DatabaseFace> db() { return m_db; };

private:
    using StateCacheDB::clear;

    std::shared_ptr<db::DatabaseFace> m_db; // this is the pointer on the PERSISTANT database (i.e., MemoryDB or LevelDB)
};

}  // namespace dev
