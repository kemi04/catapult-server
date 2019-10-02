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
#include "catapult/model/AnnotatedEntityRange.h"
#include "catapult/functions.h"

namespace catapult { namespace handlers {

	/// Handler for processing an annotated entity range.
	template<typename TEntity>
	using RangeHandler = consumer<model::AnnotatedEntityRange<TEntity>&&>;

	/// Prototype for a function that processes a range of blocks.
	using BlockRangeHandler = RangeHandler<model::Block>;

	/// Prototype for a function that processes a range of transactions.
	using TransactionRangeHandler = RangeHandler<model::Transaction>;

	/// Accepts a range and returns a producer that produces specified shared pointer elements.
	template<typename TIdentifier, typename TEntity>
	using SharedPointerProducerFactory = std::function<supplier<std::shared_ptr<const TEntity>> (const model::EntityRange<TIdentifier>&)>;

	/// Accepts a range and returns a producer that produces specified raw pointer elements.
	template<typename TIdentifier, typename TEntity>
	using RawPointerProducerFactory = std::function<supplier<const TEntity*> (const model::EntityRange<TIdentifier>&)>;
}}
