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
#include "AccountLinkEntityType.h"
#include "catapult/model/KeyLinkSharedTransaction.h"

namespace catapult { namespace model {

#pragma pack(push, 1)

	/// Binary layout for a node key link transaction body.
	template<typename THeader>
	struct NodeKeyLinkTransactionBody : public BasicKeyLinkTransactionBody<THeader, Key, Entity_Type_Node_Key_Link> {};

	DEFINE_EMBEDDABLE_TRANSACTION(NodeKeyLink)

#pragma pack(pop)
}}
