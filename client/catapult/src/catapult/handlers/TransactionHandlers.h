#pragma once
#include "HandlerTypes.h"
#include "catapult/ionet/PacketHandlers.h"
#include "catapult/model/RangeTypes.h"
#include "catapult/model/Transaction.h"
#include "catapult/utils/ShortHash.h"
#include <unordered_set>

namespace catapult { namespace handlers {

	/// Transactions returned by the unconfirmed transactions retriever.
	using UnconfirmedTransactions = std::vector<std::shared_ptr<const model::Transaction>>;

	/// Registers a push transactions handler in \a handlers that forwards transactions to \a transactionRangeHandler
	/// given a \a registry composed of known transactions.
	void RegisterPushTransactionsHandler(
			ionet::ServerPacketHandlers& handlers,
			const model::TransactionRegistry& registry,
			const TransactionRangeHandler& transactionRangeHandler);

	/// Prototype for a function that retrieves unconfirmed transactions given a set of short hashes.
	using UtRetriever = std::function<UnconfirmedTransactions (const utils::ShortHashesSet&)>;

	/// Registers a pull transactions handler in \a handlers that responds with unconfirmed transactions
	/// returned by the retriever (\a utRetriever).
	void RegisterPullTransactionsHandler(ionet::ServerPacketHandlers& handlers, const UtRetriever& utRetriever);
}}
