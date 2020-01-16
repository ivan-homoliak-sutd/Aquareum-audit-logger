// Aleth: Ethereum C++ client, tools and libraries.
// Copyright 2014-2019 Aleth Authors.
// Licensed under the GNU General Public License, Version 3.

#pragma once

#include "Common.h"
#include "RLP.h"
#include "SHA3.h"

#include <sstream>
#include <fmt/format_header_only.h>

namespace dev
{
extern const h256 EmptyTrie;

inline byte nibble(bytesConstRef _data, unsigned _i)
{
	return (_i & 1) ? (_data[_i / 2] & 15) : (_data[_i / 2] >> 4);
}

/// Interprets @a _first and @a _second as vectors of nibbles and returns the length of the longest common
/// prefix of _first[_beginFirst..._endFirst] and _second[_beginSecond..._endSecond].
inline unsigned sharedNibbles(bytesConstRef _first, unsigned _beginFirst, unsigned _endFirst, bytesConstRef _second, unsigned _beginSecond, unsigned _endSecond)
{
	unsigned ret = 0;
	while (_beginFirst < _endFirst && _beginSecond < _endSecond && nibble(_first, _beginFirst) == nibble(_second, _beginSecond))
	{
		++_beginFirst;
		++_beginSecond;
		++ret;
	}
	return ret;
}

/**
 * Nibble-based view on a bytesConstRef.
 */
struct NibbleSlice
{
	bytesConstRef data;
	unsigned offset;

	NibbleSlice(bytesConstRef _data = bytesConstRef(), unsigned _offset = 0): data(_data), offset(_offset) {}
	byte operator[](unsigned _index) const { return nibble(data, offset + _index); }
	unsigned size() const { return data.size() * 2 - offset; } //IH: the number of valid nibbles in this slice
	bool empty() const { return !size(); }
	NibbleSlice mid(unsigned _index) const { return NibbleSlice(data, offset + _index); }
	void clear() { data.reset(); offset = 0; }

	/// @returns true iff _k is a prefix of this.
	bool contains(NibbleSlice _k) const { return shared(_k) == _k.size(); }
	/// @returns the number of shared nibbles at the beginning of this and _k.
	unsigned shared(NibbleSlice _k) const { return sharedNibbles(data, offset, offset + size(), _k.data, _k.offset, _k.offset + _k.size()); }
	/**
	 * @brief Determine if we, a full key, are situated prior to a particular key-prefix.
	 * @param _k The prefix.
	 * @return true if we are strictly prior to the prefix.
	 */
	bool isEarlierThan(NibbleSlice _k) const
	{
		unsigned i = 0;
		for (; i < _k.size() && i < size(); ++i)
			if (operator[](i) < _k[i])		// Byte is lower - we're earlier..
				return true;
			else if (operator[](i) > _k[i])	// Byte is higher - we're not earlier.
				return false;
		if (i >= _k.size())					// Ran past the end of the prefix - we're == for the entire prefix - we're not earlier.
			return false;
		return true;						// Ran out before the prefix had finished - we're earlier.
	}
	bool operator==(NibbleSlice _k) const { return _k.size() == size() && shared(_k) == _k.size(); }
	bool operator!=(NibbleSlice _s) const { return !operator==(_s); }
};

inline std::ostream& operator<<(std::ostream& _out, NibbleSlice const& _m)
{
	for (unsigned i = 0; i < _m.size(); ++i)
		_out << std::hex << (int)_m[i] << std::dec;
	return _out;
}

inline bool isLeaf(RLP const& _twoItem)
{
	assert(_twoItem.isList() && _twoItem.itemCount() == 2);
	auto pl = _twoItem[0].payload();
	return (pl[0] & 0x20) != 0; // IH bit 0x20 in HPE encodes whether a node is leaf (=true) or not
}

// IH: returns just data part of HPE passed
inline NibbleSlice keyOf(bytesConstRef _hpe)
{
	if (!_hpe.size())
		return NibbleSlice(_hpe, 0);
	if (_hpe[0] & 0x10) // IH: 0x10 means odd number of nibbles (in which 1st byte contains the 1st nibble of data => skip one nibble in slice)
		return NibbleSlice(_hpe, 1);
	else
		return NibbleSlice(_hpe, 2); // IH: even number of nibbles encodes no data at the 1st byte => skipe 2 nibbles in the slice
}

// IH: input node is list with 2 items (partialPath and value|key)
inline NibbleSlice keyOf(RLP const& _twoItem)
{
	return keyOf(_twoItem[0].payload());
}

byte uniqueInUse(RLP const& _orig, byte except);
std::string hexPrefixEncode(bytes const& _hexVector, bool _leaf = false, int _begin = 0, int _end = -1);
std::string hexPrefixEncode(bytesConstRef _data, bool _leaf, int _beginNibble, int _endNibble, unsigned _offset);
std::string hexPrefixEncode(bytesConstRef _d1, unsigned _o1, bytesConstRef _d2, unsigned _o2, bool _leaf);

inline std::string hexPrefixEncode(NibbleSlice _s, bool _leaf, int _begin = 0, int _end = -1)
{
	return hexPrefixEncode(_s.data, _leaf, _begin, _end, _s.offset);
}

inline std::string hexPrefixEncode(NibbleSlice _s1, NibbleSlice _s2, bool _leaf)
{
	return hexPrefixEncode(_s1.data, _s1.offset, _s2.data, _s2.offset, _leaf);
}

// Author: IH
inline std::string RLP2MP3String(const RLP& rlp)
    {
        std::string mp3_data = "";

        // std::cerr << "RLP |items| = %ld", rlp.itemCount();
        if (2 == rlp.itemCount() && isLeaf(rlp)) {  // has 2 items
            // append partial nibble
            std::stringstream s;
            s << dev::keyOf(rlp);
            mp3_data += fmt::format("[Leaf]\t k={} | ", s.str());

            // append data of entry
            mp3_data += fmt::format("v={}\n", rlp[1].toString());

        } else if (2 == rlp.itemCount()) {  //  extension node (or in a special case might be empty root node)
            std::stringstream s;
            s << dev::keyOf(rlp);
            auto h = h256(rlp[1]);
            mp3_data += fmt::format("[Extension] path={} | db_k={}\n", s.str(), h.hex());

        } else if (17 == rlp.itemCount()) {  // branch node
            mp3_data += "[Branch]\n";
            int j = 0;
            for (auto r : rlp) {
                auto h = h256(r);
                std::string idx = (j != 16) ? fmt::format("{}", j) : "val";
                mp3_data += fmt::format("\t\t {} : {}\n", idx, h.hex());
                j++;
            }
        } else {
            assert(0 == rlp.itemCount());  // initial root
            mp3_data += "[ROOT] is initialy empty";
        }
        auto h = sha3(rlp.data());
        return fmt::format("db_k = {} => {}", h.hex(), mp3_data);
    }

}
