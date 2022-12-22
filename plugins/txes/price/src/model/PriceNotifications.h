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
#include "catapult/model/Mosaic.h"
#include "catapult/model/Notifications.h"

namespace catapult { namespace model {

	// region price notification types

/// Defines a price notification type with \a DESCRIPTION, \a CODE and \a CHANNEL.
#define DEFINE_PRICE_NOTIFICATION(DESCRIPTION, CODE, CHANNEL) DEFINE_NOTIFICATION_TYPE(CHANNEL, Price, DESCRIPTION, CODE)

	/// Price was received with a message.
	DEFINE_PRICE_NOTIFICATION(Message, 0x0001, All);

#undef DEFINE_PRICE_NOTIFICATION

	// endregion

	// region PriceMessageNotification

	/// Notification of a price transaction with a message.
	struct PriceMessageNotification : public Notification {
	public:
		/// Matching notification type.
		static constexpr auto Notification_Type = Price_Message_Notification;

	public:
		/// Creates a notification around \a senderPublicKey, \a messageSize and \a pMessage.
		PriceMessageNotification(
				const Key& senderPublicKey,
				uint64_t BlockHeight,
				uint64_t LowPrice,
				uint64_t HighPrice)
				: Notification(Notification_Type, sizeof(PriceMessageNotification))
				, SenderPublicKey(senderPublicKey)
				, blockHeight(BlockHeight)
				, lowPrice(LowPrice)
				, highPrice(HighPrice)
		{}

	public:
		/// Message sender public key.
		const Key& SenderPublicKey;
		
		uint64_t blockHeight;

		/// Message size in bytes.
		uint64_t lowPrice;

		/// Const pointer to the message data.
		uint64_t highPrice;
	};

	// endregion
}}
