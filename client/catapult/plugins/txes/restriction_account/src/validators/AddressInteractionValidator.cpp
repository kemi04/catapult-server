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

#include "Validators.h"
#include "AccountRestrictionView.h"
#include "src/cache/AccountRestrictionCache.h"
#include "catapult/model/Address.h"
#include "catapult/validators/ValidatorContext.h"

namespace catapult { namespace validators {

	using Notification = model::AddressInteractionNotification;
	using CacheReadOnlyType = typename cache::AccountRestrictionCacheTypes::CacheReadOnlyType;

	namespace {
		constexpr auto Address_Restriction_Type = model::AccountRestrictionType::Address;
		constexpr auto Address_Outgoing_Restriction_Type = Address_Restriction_Type | model::AccountRestrictionType::Outgoing;

		bool IsInteractionAllowed(
				const cache::ReadOnlyCatapultCache& cache,
				model::AccountRestrictionType restrictionType,
				const Address& source,
				const Address& participant) {
			if (source == participant)
				return true;

			AccountRestrictionView view(cache);
			return !view.initialize(participant) || view.isAllowed(restrictionType, source);
		}

		bool IsInteractionAllowed(const cache::ReadOnlyCatapultCache& cache, const Address& source, const Address& participant) {
			return
					IsInteractionAllowed(cache, Address_Restriction_Type, source, participant) &&
					IsInteractionAllowed(cache, Address_Outgoing_Restriction_Type, participant, source);
		}
	}

	DEFINE_STATEFUL_VALIDATOR(AddressInteraction, [](const Notification& notification, const ValidatorContext& context) {
		auto networkIdentifier = context.Network.Identifier;
		auto sourceAddress = model::PublicKeyToAddress(notification.Source, networkIdentifier);
		for (const auto& address : notification.ParticipantsByAddress) {
			auto participant = context.Resolvers.resolve(address);
			if (!IsInteractionAllowed(context.Cache, sourceAddress, participant))
				return Failure_RestrictionAccount_Address_Interaction_Not_Allowed;
		}

		for (const auto& key : notification.ParticipantsByKey) {
			auto participant = model::PublicKeyToAddress(key, networkIdentifier);
			if (!IsInteractionAllowed(context.Cache, sourceAddress, participant))
				return Failure_RestrictionAccount_Address_Interaction_Not_Allowed;
		}

		return ValidationResult::Success;
	});
}}
