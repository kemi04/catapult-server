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
#include "finalization/src/model/FinalizationMessage.h"
#include "finalization/src/model/PackedFinalizationProof.h"
#include "catapult/model/HeightHashPair.h"
#include <memory>

namespace catapult { namespace io {

	/// Finalization proof.
	using FinalizationProof = std::vector<std::shared_ptr<const model::FinalizationMessage>>;

	/// Interface for saving and loading finalization proofs.
	class ProofStorage {
	public:
		virtual ~ProofStorage() = default;

	public:
		/// Gets the number of finalized proofs.
		virtual FinalizationPoint finalizationPoint() const = 0;

		/// Gets the last finalized height.
		virtual Height finalizedHeight() const = 0;

		/// Gets a range of at most \a maxHashes height-hash pairs starting at \a point.
		virtual model::HeightHashPairRange loadFinalizedHashesFrom(FinalizationPoint point, size_t maxHashes) const = 0;

		/// Gets the finalization proof at \a point.
		virtual std::shared_ptr<const model::PackedFinalizationProof> loadProof(FinalizationPoint point) const = 0;

		/// Saves finalization \a proof of block at \a height.
		virtual void saveProof(Height height, const FinalizationProof& proof) = 0;
	};
}}