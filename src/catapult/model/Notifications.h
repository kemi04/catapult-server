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
#include "ContainerTypes.h"
#include "EntityType.h"
#include "NetworkInfo.h"
#include "NotificationType.h"
#include "catapult/utils/ArraySet.h"
#include "catapult/types.h"
#include <vector>

namespace catapult { namespace model {

	/// A basic notification.
	struct Notification {
	public:
		/// Creates a new notification with \a type and \a size.
		explicit Notification(NotificationType type, size_t size)
				: Type(type)
				, Size(size)
		{}

	public:
		/// Notification type.
		NotificationType Type;

		/// Notification size.
		size_t Size;
	};

	// region account

	/// Notification of use of an account address.
	struct AccountAddressNotification : public Notification {
	public:
		/// Matching notification type.
		static constexpr auto Notification_Type = Core_Register_Account_Address_Notification;

	public:
		/// Creates a notification around \a address.
		explicit AccountAddressNotification(const UnresolvedAddress& address)
				: Notification(Notification_Type, sizeof(AccountAddressNotification))
				, Address(address)
		{}

	public:
		/// Address.
		UnresolvedAddress Address;
	};

	/// Notification of use of an account public key.
	struct AccountPublicKeyNotification : public Notification {
	public:
		/// Matching notification type.
		static constexpr auto Notification_Type = Core_Register_Account_Public_Key_Notification;

	public:
		/// Creates a notification around \a publicKey.
		explicit AccountPublicKeyNotification(const Key& publicKey)
				: Notification(Notification_Type, sizeof(AccountPublicKeyNotification))
				, PublicKey(publicKey)
		{}

	public:
		/// Public key.
		const Key& PublicKey;
	};

	// endregion

	// region balance

	/// A basic balance notification.
	template<typename TDerivedNotification>
	struct BasicBalanceNotification : public Notification {
	public:
		/// Creates a notification around \a sender, \a mosaicId and \a amount.
		explicit BasicBalanceNotification(const Key& sender, UnresolvedMosaicId mosaicId, Amount amount)
				: Notification(TDerivedNotification::Notification_Type, sizeof(TDerivedNotification))
				, Sender(sender)
				, MosaicId(mosaicId)
				, Amount(amount)
		{}

	public:
		/// Sender.
		const Key& Sender;

		/// Mosaic id.
		UnresolvedMosaicId MosaicId;

		/// Amount.
		catapult::Amount Amount;
	};

	/// Notifies a balance transfer from sender to recipient.
	struct BalanceTransferNotification : public BasicBalanceNotification<BalanceTransferNotification> {
	public:
		/// Matching notification type.
		static constexpr auto Notification_Type = Core_Balance_Transfer_Notification;

	public:
		/// Creates a notification around \a sender, \a recipient, \a mosaicId and \a amount.
		explicit BalanceTransferNotification(
				const Key& sender,
				const UnresolvedAddress& recipient,
				UnresolvedMosaicId mosaicId,
				catapult::Amount amount)
				: BasicBalanceNotification(sender, mosaicId, amount)
				, Recipient(recipient)
		{}

	public:
		/// Recipient.
		UnresolvedAddress Recipient;
	};

	/// Notifies a balance debit by sender.
	struct BalanceDebitNotification : public BasicBalanceNotification<BalanceDebitNotification> {
	public:
		/// Matching notification type.
		static constexpr auto Notification_Type = Core_Balance_Debit_Notification;

	public:
		using BasicBalanceNotification<BalanceDebitNotification>::BasicBalanceNotification;
	};

	// endregion

	// region entity

	/// Notifies the arrival of an entity.
	struct EntityNotification : public Notification {
	public:
		/// Matching notification type.
		static constexpr auto Notification_Type = Core_Entity_Notification;

	public:
		/// Creates an entity notification around \a networkIdentifier, \a minVersion, \a maxVersion and \a entityVersion.
		explicit EntityNotification(
				model::NetworkIdentifier networkIdentifier,
				uint8_t minVersion,
				uint8_t maxVersion,
				uint8_t entityVersion)
				: Notification(Notification_Type, sizeof(EntityNotification))
				, NetworkIdentifier(networkIdentifier)
				, MinVersion(minVersion)
				, MaxVersion(maxVersion)
				, EntityVersion(entityVersion)
		{}

	public:
		/// Network identifier.
		model::NetworkIdentifier NetworkIdentifier;

		/// Minimum supported version.
		uint8_t MinVersion;

		/// Maximum supported version.
		uint8_t MaxVersion;

		/// Entity version.
		uint8_t EntityVersion;
	};

	// endregion

	// region block

	/// Notifies the arrival of a block.
	struct BlockNotification : public Notification {
	public:
		/// Matching notification type.
		static constexpr auto Notification_Type = Core_Block_Notification;

	public:
		/// Creates a block notification around \a signer, \a timestamp and \a difficulty.
		explicit BlockNotification(const Key& signer, Timestamp timestamp, Difficulty difficulty)
				: Notification(Notification_Type, sizeof(BlockNotification))
				, Signer(signer)
				, Timestamp(timestamp)
				, Difficulty(difficulty)
				, NumTransactions(0)
		{}

	public:
		/// Block signer.
		const Key& Signer;

		/// Block timestamp.
		catapult::Timestamp Timestamp;

		/// Block difficulty.
		catapult::Difficulty Difficulty;

		/// Total block fee.
		Amount TotalFee;

		/// Number of block transactions.
		uint32_t NumTransactions;
	};

	// endregion

	// region transaction

	/// Notifies the arrival of a transaction.
	struct TransactionNotification : public Notification {
	public:
		/// Matching notification type.
		static constexpr auto Notification_Type = Core_Transaction_Notification;

	public:
		/// Creates a transaction notification around \a signer, \a transactionHash, \a transactionType and \a deadline.
		explicit TransactionNotification(const Key& signer, const Hash256& transactionHash, EntityType transactionType, Timestamp deadline)
				: Notification(Notification_Type, sizeof(TransactionNotification))
				, Signer(signer)
				, TransactionHash(transactionHash)
				, TransactionType(transactionType)
				, Deadline(deadline)
		{}

	public:
		/// Transaction signer.
		const Key& Signer;

		/// Transaction hash.
		const Hash256& TransactionHash;

		/// Transaction type.
		EntityType TransactionType;

		/// Transaction deadline.
		Timestamp Deadline;
	};

	/// Notifies the arrival of a transaction fee.
	struct TransactionFeeNotification : public Notification {
	public:
		/// Matching notification type.
		static constexpr auto Notification_Type = Core_Transaction_Fee_Notification;

	public:
		/// Creates a transaction fee notification around \a transactionSize, \a fee and \a maxFee.
		explicit TransactionFeeNotification(uint32_t transactionSize, Amount fee, Amount maxFee)
				: Notification(Notification_Type, sizeof(TransactionFeeNotification))
				, TransactionSize(transactionSize)
				, Fee(fee)
				, MaxFee(maxFee)
		{}

	public:
		/// Transaction size.
		uint32_t TransactionSize;

		/// Transaction fee.
		Amount Fee;

		// Maximum transaction fee.
		Amount MaxFee;
	};

	// endregion

	// region signature

	/// Notifies the presence of a signature.
	struct SignatureNotification : public Notification {
	public:
		/// Matching notification type.
		static constexpr auto Notification_Type = Core_Signature_Notification;

	public:
		/// Creates a signature notification around \a signer, \a signature and \a data.
		explicit SignatureNotification(const Key& signer, const Signature& signature, const RawBuffer& data)
				: Notification(Notification_Type, sizeof(SignatureNotification))
				, Signer(signer)
				, Signature(signature)
				, Data(data)
		{}

	public:
		/// Signer.
		const Key& Signer;

		/// Signature.
		const catapult::Signature& Signature;

		/// Signed data.
		RawBuffer Data;
	};

	// endregion

	// region address interaction

	/// Notifies that a source address interacts with participant addresses.
	/// \note This notification cannot be used by an observer.
	struct AddressInteractionNotification : public Notification {
	public:
		/// Matching notification type.
		static constexpr auto Notification_Type = Core_Address_Interaction_Notification;

	public:
		/// Creates a notification around \a source, \a transactionType and \a participantsByAddress.
		AddressInteractionNotification(const Key& source, EntityType transactionType, const UnresolvedAddressSet& participantsByAddress)
				: AddressInteractionNotification(source, transactionType, participantsByAddress, {})
		{}

		/// Creates a notification around \a source, \a transactionType, \a participantsByAddress and \a participantsByKey.
		AddressInteractionNotification(
				const Key& source,
				EntityType transactionType,
				const UnresolvedAddressSet& participantsByAddress,
				const utils::KeySet& participantsByKey)
				: Notification(Notification_Type, sizeof(AddressInteractionNotification))
				, Source(source)
				, TransactionType(transactionType)
				, ParticipantsByAddress(participantsByAddress)
				, ParticipantsByKey(participantsByKey)
		{}

	public:
		/// Source.
		Key Source;

		/// Transaction type.
		EntityType TransactionType;

		/// Participants given by address.
		UnresolvedAddressSet ParticipantsByAddress;

		/// Participants given by public key.
		utils::KeySet ParticipantsByKey;
	};

	// endregion

	// region mosaic required

	/// Notification of a required mosaic.
	struct MosaicRequiredNotification : public Notification {
	public:
		/// Mosaic types.
		enum class MosaicType { Resolved, Unresolved };

	public:
		/// Matching notification type.
		static constexpr auto Notification_Type = Core_Mosaic_Required_Notification;

	public:
		/// Creates a notification around \a signer and \a mosaicId.
		explicit MosaicRequiredNotification(const Key& signer, MosaicId mosaicId)
				: Notification(Notification_Type, sizeof(MosaicRequiredNotification))
				, Signer(signer)
				, MosaicId(mosaicId)
				, ProvidedMosaicType(MosaicType::Resolved)
		{}

		/// Creates a notification around \a signer and \a mosaicId.
		explicit MosaicRequiredNotification(const Key& signer, UnresolvedMosaicId mosaicId)
				: Notification(Notification_Type, sizeof(MosaicRequiredNotification))
				, Signer(signer)
				, UnresolvedMosaicId(mosaicId)
				, ProvidedMosaicType(MosaicType::Unresolved)
		{}

	public:
		/// Signer.
		const Key& Signer;

		/// Mosaic id (resolved).
		catapult::MosaicId MosaicId;

		/// Mosaic id (unresolved).
		catapult::UnresolvedMosaicId UnresolvedMosaicId;

		/// Type of mosaic provided and attached to this notification.
		MosaicType ProvidedMosaicType;
	};

	// endregion

	// region source change

	/// Notification of a source change.
	struct SourceChangeNotification : public Notification {
	public:
		/// Source change types.
		enum class SourceChangeType { Absolute, Relative };

	public:
		/// Matching notification type.
		static constexpr auto Notification_Type = Core_Source_Change_Notification;

	public:
		/// Creates a notification around \a primaryId, \a secondaryId and \a changeType.
		explicit SourceChangeNotification(uint32_t primaryId, uint32_t secondaryId, SourceChangeType changeType)
				: Notification(Notification_Type, sizeof(SourceChangeNotification))
				, PrimaryId(primaryId)
				, SecondaryId(secondaryId)
				, ChangeType(changeType)
		{}

	public:
		/// Primary source (e.g. index within block).
		uint32_t PrimaryId;

		/// Secondary source (e.g. index within aggregate).
		uint32_t SecondaryId;

		/// Type of source change.
		SourceChangeType ChangeType;
	};

	// endregion
}}
