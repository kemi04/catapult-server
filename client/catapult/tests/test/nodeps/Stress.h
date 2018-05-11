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
#include "catapult/utils/ExceptionLogging.h"
#include "catapult/utils/Logging.h"
#include <sstream>
#include <gtest/gtest.h>

namespace catapult { namespace test {

// if STRESS is defined to an iteration number, every test will be repeated that number of times
// for example, the following code will cause all tests to run 100 times
// #define STRESS 100u
#ifdef STRESS
#undef TEST

#define TEST_FRIENDLY_NAME_(TEST_FIXTURE, TEST_NAME) #TEST_FIXTURE "::" #TEST_NAME
#define TEST_IMPL_NAME_(TEST_FIXTURE, TEST_NAME) TEST_FIXTURE##_##TEST_NAME##_Impl

#define TEST(TEST_FIXTURE, TEST_NAME) \
	void TEST_IMPL_NAME_(TEST_FIXTURE, TEST_NAME)(); \
	\
	GTEST_TEST(TEST_FIXTURE, TEST_NAME) { \
		for (auto stressCounter = 1u; stressCounter <= STRESS; ++stressCounter) { \
			const auto Test_Fqn = TEST_FRIENDLY_NAME_(TEST_FIXTURE, TEST_NAME); \
			CATAPULT_LOG(debug) << "---- iter " << stressCounter << "/" << STRESS << " " << Test_Fqn << " ----"; \
			\
			try { \
				TEST_IMPL_NAME_(TEST_FIXTURE, TEST_NAME)(); \
			} catch (...) { \
				/* flatten error information into std::runtime_error for better jenkins reporting */ \
				std::stringstream exceptionMessage; \
				exceptionMessage \
						<< "unhandled exception during " \
						<< Test_Fqn << " iteration " << stressCounter << "!" \
						<< EXCEPTION_DIAGNOSTIC_MESSAGE(); \
				CATAPULT_LOG(fatal) << exceptionMessage.str(); \
				throw std::runtime_error(exceptionMessage.str().c_str()); \
			} \
			\
			if (!::testing::Test::HasFailure()) continue; \
			CATAPULT_LOG(error) << Test_Fqn << " failed on iteration " << stressCounter; \
			return; \
		} \
	} \
	\
	void TEST_IMPL_NAME_(TEST_FIXTURE, TEST_NAME)()
#endif

// NO_STRESS_TEST should be used by tests that shouldn't be stressed
#define NO_STRESS_TEST(TEST_FIXTURE, TEST_NAME) GTEST_TEST(TEST_FIXTURE, TEST_NAME)
}}
