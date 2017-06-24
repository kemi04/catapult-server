#pragma once
#include "plugins/mongo/coremongo/src/ExternalCacheStorage.h"
#include <memory>

namespace catapult {
	namespace mongo {
		namespace plugins {
			class MongoBulkWriter;
			class MongoDatabase;
		}
	}
}

namespace catapult { namespace mongo { namespace storages {

	/// Creates a mongo namespace cache storage around \a database and \a bulkWriter.
	std::unique_ptr<plugins::ExternalCacheStorage> CreateMongoNamespaceCacheStorage(
			plugins::MongoDatabase&& database,
			plugins::MongoBulkWriter& bulkWriter);
}}}
