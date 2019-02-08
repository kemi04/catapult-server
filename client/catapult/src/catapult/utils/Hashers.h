/**
*** Copyright (c) 2016-present,
*** Jaguar0625, gimre, BloodyRookie, Tech Bureau, Corp. All rights reserved.
***
*** This file is part of Catapult.
***
*** Catapult is free software: you can redistribute it and/or modify
*** it under the terms of the GNU Lesser General Public License as published by
*** the Free Software Foundation, either version 3 of the License, or
*** (at your option) any later version.
***
*** Catapult is distributed in the hope that it will be useful,
*** but WITHOUT ANY WARRANTY; without even the implied warranty of
*** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*** GNU Lesser General Public License for more details.
***
*** You should have received a copy of the GNU Lesser General Public License
*** along with Catapult. If not, see <http://www.gnu.org/licenses/>.
**/

#pragma once
#include <array>
#include <cstring>

namespace catapult { namespace utils {

	/// Hasher object for an std::array with a variable offset.
	/// \note Offset defaults to 4 because because some arrays (e.g. Address) don't have a lot of entropy at the beginning.
	/// \note Hash is composed of only sizeof(size_t) bytes starting at offset.
	template<typename TArray, size_t Offset = 4>
	struct ArrayHasher {
	private:
		static constexpr auto N = std::tuple_size<TArray>::value;

	public:
		/// Hashes \a arrayData.
		size_t operator()(const std::array<uint8_t, N>& arrayData) const {
			size_t hash;
			std::memcpy(static_cast<void*>(&hash), &arrayData[Offset], sizeof(size_t));
			return hash;
		}
	};

	/// Hasher object for a base value.
	template<typename TValue>
	struct BaseValueHasher {
		/// Hashes \a value.
		size_t operator()(TValue value) const {
			return static_cast<size_t>(value.unwrap());
		}
	};
}}
