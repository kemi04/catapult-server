/**
*** Copyright (c) 2016-2019, Jaguar0625, gimre, BloodyRookie, Tech Bureau, Corp.
*** Copyright (c) 2020-present, Jaguar0625, gimre, BloodyRookie.
*** All rights reserved.
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
#include "PriceEntityType.h"
#include "catapult/model/Mosaic.h"
#include "catapult/model/Transaction.h"

namespace catapult { namespace model {

#pragma pack(push, 1)

	/// Binary layout for a price transaction body.
	template<typename THeader>
	struct PriceTransactionBody : public THeader {
	private:
		using TransactionType = PriceTransactionBody<THeader>;

	public:
		DEFINE_TRANSACTION_CONSTANTS(Entity_Type_Price, 1)

	public:

		/// Message size in bytes.
		uint16_t MessageSize;

		// followed by message data if MessageSize != 0
		DEFINE_TRANSACTION_VARIABLE_DATA_ACCESSORS(Message, uint8_t)

	private:

		template<typename T>
		static auto* MessagePtrT(T& transaction) {
			auto* pPayloadStart = THeader::PayloadStart(transaction);
			return transaction.MessageSize && pPayloadStart
					? pPayloadStart
					: nullptr;
		}

	public:
		/// Calculates the real size of price \a transaction.
		static constexpr uint64_t CalculateRealSize(const TransactionType& transaction) noexcept {
			return sizeof(TransactionType) + transaction.MessageSize;
		}
	};

	DEFINE_EMBEDDABLE_TRANSACTION(Price)

#pragma pack(pop)
}}
