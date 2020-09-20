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
#include "catapult/model/FinalizationRound.h"

namespace catapult { namespace model {

#pragma pack(push, 1)

	/// Finalization round range.
	struct FinalizationRoundRange {
	public:
		/// Creates a default range.
		constexpr FinalizationRoundRange() = default;

		/// Creates a range from \a min to \a max.
		constexpr FinalizationRoundRange(const FinalizationRound& min, const FinalizationRound& max)
				: Min(min)
				, Max(max)
		{}

	public:
		/// Minimum round.
		FinalizationRound Min;

		/// Maximum round.
		FinalizationRound Max;

	public:
		/// Returns \c true if this range is equal to \a rhs.
		bool operator==(const FinalizationRoundRange& rhs) const;

		/// Returns \c true if this range is not equal to \a rhs.
		bool operator!=(const FinalizationRoundRange& rhs) const;
	};

#pragma pack(pop)

	/// Insertion operator for outputting \a roundRange to \a out.
	std::ostream& operator<<(std::ostream& out, const FinalizationRoundRange& roundRange);

	/// Returns \c true if \a round is contained in \a roundRange, inclusive.
	bool IsInRange(const FinalizationRoundRange& roundRange, const FinalizationRound& round);
}}
