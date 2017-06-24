#pragma once
#include "plugins/mongo/coremongo/src/MongoTransactionPlugin.h"
#include "tests/test/core/mocks/MockTransaction.h"

namespace catapult { namespace mocks {

	/// Creates a mock mongo transaction plugin with the specified \a type.
	std::unique_ptr<mongo::plugins::MongoTransactionPlugin> CreateMockTransactionMongoPlugin(int type);

	/// Creates a mock mongo transaction plugin with \a options and \a numDependentDocuments dependent documents.
	std::unique_ptr<mongo::plugins::MongoTransactionPlugin> CreateMockTransactionMongoPlugin(
			PluginOptionFlags options = PluginOptionFlags::Default,
			size_t numDependentDocuments = 0);
}}
