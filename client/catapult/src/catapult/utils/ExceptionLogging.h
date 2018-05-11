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
#include <boost/exception/diagnostic_information.hpp>

namespace catapult { namespace utils {

/// Outputs exception diagnostic information on a new line.
#define EXCEPTION_DIAGNOSTIC_MESSAGE() std::endl << boost::current_exception_diagnostic_information()

/// Outputs a message for an unhandled exception that occurred during \a ACTION.
#define UNHANDLED_EXCEPTION_MESSAGE(ACTION) "unhandled exception while " << ACTION << "!" << EXCEPTION_DIAGNOSTIC_MESSAGE()
}}

