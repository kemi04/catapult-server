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
#include "MultisigCacheTypes.h"
#include "catapult/utils/ArraySet.h"

namespace catapult { namespace cache {

	/// Finds all ancestors of a \a key in \a cache, adds them to \a ancestorKeys and returns the maximum distance between
	/// \a key and any ancestor.
	size_t FindAncestors(const MultisigCacheTypes::CacheReadOnlyType& cache, const Key& key, utils::KeySet& ancestorKeys);

	/// Finds all descendants of a \a key in \a cache, adds them to \a descendantKeys and returns the maximum distance between
	/// \a key and any descendant.
	size_t FindDescendants(const MultisigCacheTypes::CacheReadOnlyType& cache, const Key& key, utils::KeySet& descendantKeys);
}}
