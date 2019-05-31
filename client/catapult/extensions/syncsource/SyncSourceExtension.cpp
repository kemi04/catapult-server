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

#include "src/SyncSourceService.h"
#include "src/VerifiableStateService.h"
#include "catapult/extensions/ProcessBootstrapper.h"

namespace catapult { namespace syncsource {

	namespace {
		void RegisterExtension(extensions::ProcessBootstrapper& bootstrapper) {
			// register service(s)
			auto& extensionManager = bootstrapper.extensionManager();
			extensionManager.addServiceRegistrar(CreateSyncSourceServiceRegistrar());

			if (bootstrapper.config().BlockChain.ShouldEnableVerifiableState)
				extensionManager.addServiceRegistrar(CreateVerifiableStateServiceRegistrar());
		}
	}
}}

extern "C" PLUGIN_API
void RegisterExtension(catapult::extensions::ProcessBootstrapper& bootstrapper) {
	catapult::syncsource::RegisterExtension(bootstrapper);
}
