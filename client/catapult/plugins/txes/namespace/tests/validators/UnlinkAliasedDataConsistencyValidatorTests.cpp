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

#include "src/validators/Validators.h"
#include "tests/test/AliasTestUtils.h"
#include "tests/test/NamespaceCacheTestUtils.h"
#include "tests/test/NamespaceTestUtils.h"
#include "tests/test/plugins/ValidatorTestUtils.h"

namespace catapult { namespace validators {

	namespace {
		template<typename TTraits>
		class UnlinkAliasedDataConsistencyValidatorTests {
		private:
			using NotificationType = typename TTraits::NotificationType;

			static constexpr auto DefaultNamespaceId() {
				return NamespaceId(123);
			}

			static auto CreateDefaultRootNamespace() {
				auto owner = test::GenerateRandomData<Key_Size>();
				return state::RootNamespace(DefaultNamespaceId(), owner, test::CreateLifetime(10, 20));
			}

			static NotificationType CreateNotification(model::AliasAction aliasAction = model::AliasAction::Unlink) {
				typename TTraits::AliasType alias;
				test::FillWithRandomData(alias);
				return NotificationType(DefaultNamespaceId(), aliasAction, alias);
			}

			template<typename TSeedCacheFunc>
			static auto CreateAndSeedCache(TSeedCacheFunc seedCache) {
				auto cache = test::NamespaceCacheFactory::Create();
				auto cacheDelta = cache.createDelta();
				auto& namespaceCacheDelta = cacheDelta.sub<cache::NamespaceCache>();
				seedCache(namespaceCacheDelta);
				cache.commit(Height());
				return cache;
			}

			template<typename TSeedCacheFunc>
			static void RunUnlinkValidatorTest(
				validators::ValidationResult expectedResult,
				const NotificationType& notification,
				TSeedCacheFunc seedCache) {
				// Arrange:
				auto cache = CreateAndSeedCache(seedCache);
				auto pValidator = TTraits::CreateValidator();

				// Act:
				auto result = test::ValidateNotification(*pValidator, notification, cache);

				// Assert:
				EXPECT_EQ(expectedResult, result) << utils::HexFormat(notification.AliasedData);
			}

		public:
			static void AssertSuccessIfActionIsNotUnlink() {
				// Arrange:
				auto notification = CreateNotification(model::AliasAction::Link);

				// Assert:
				RunUnlinkValidatorTest(validators::ValidationResult::Success, notification, [](const auto&) {});
			}

			static void AssertFailureIfNamespaceIsUnknown() {
				// Arrange:
				auto notification = CreateNotification();

				// Assert:
				RunUnlinkValidatorTest(validators::Failure_Namespace_Alias_Namespace_Unknown, notification, [](const auto&) {});
			}

			static void AssertFailureIfNamespaceDoesNotHaveAlias() {
				// Arrange:
				auto notification = CreateNotification();

				// Assert:
				RunUnlinkValidatorTest(validators::Failure_Namespace_Alias_Unlink_Type_Inconsistency, notification, [](auto& cache) {
					cache.insert(CreateDefaultRootNamespace());
				});
			}

			static void AssertFailureIfAliasedTypeMismatch() {
				// Arrange:
				auto notification = CreateNotification();

				// Assert:
				RunUnlinkValidatorTest(validators::Failure_Namespace_Alias_Unlink_Type_Inconsistency, notification, [](auto& cache) {
					cache.insert(CreateDefaultRootNamespace());
					test::SetRandomAlias<typename TTraits::InvalidAliasType>(cache, DefaultNamespaceId());
				});
			}

			static void AssertFailureIfAliasedDataMismatch() {
				// Arrange:
				auto notification = CreateNotification();

				// Assert:
				RunUnlinkValidatorTest(validators::Failure_Namespace_Alias_Unlink_Data_Inconsistency, notification, [](auto& cache) {
					cache.insert(CreateDefaultRootNamespace());
					test::SetRandomAlias<typename TTraits::AliasType>(cache, DefaultNamespaceId());
				});
			}

			static void AssertSuccessIfAliasedTypeAndDataMatch() {
				// Arrange:
				auto notification = CreateNotification();

				// Assert:
				RunUnlinkValidatorTest(validators::ValidationResult::Success, notification, [&notification](auto& cache) {
					cache.insert(CreateDefaultRootNamespace());
					test::SetAlias(cache, DefaultNamespaceId(), notification.AliasedData);
				});
			}
		};
	}

#define MAKE_UNLINK_VALIDATOR_TEST(TEST_CLASS, TRAITS_NAME, TEST_NAME) \
	TEST(TEST_CLASS, TEST_NAME) { UnlinkAliasedDataConsistencyValidatorTests<TRAITS_NAME>::Assert##TEST_NAME(); }

#define DEFINE_ALIAS_TRANSACTION_PLUGIN_TESTS(TEST_CLASS, TRAITS_NAME) \
	MAKE_UNLINK_VALIDATOR_TEST(TEST_CLASS, TRAITS_NAME, SuccessIfActionIsNotUnlink) \
	MAKE_UNLINK_VALIDATOR_TEST(TEST_CLASS, TRAITS_NAME, FailureIfNamespaceIsUnknown) \
	MAKE_UNLINK_VALIDATOR_TEST(TEST_CLASS, TRAITS_NAME, FailureIfNamespaceDoesNotHaveAlias) \
	MAKE_UNLINK_VALIDATOR_TEST(TEST_CLASS, TRAITS_NAME, FailureIfAliasedTypeMismatch) \
	MAKE_UNLINK_VALIDATOR_TEST(TEST_CLASS, TRAITS_NAME, FailureIfAliasedDataMismatch) \
	MAKE_UNLINK_VALIDATOR_TEST(TEST_CLASS, TRAITS_NAME, SuccessIfAliasedTypeAndDataMatch)

	// region unlink aliased address consistency validator tests

	namespace {
		struct AddressTraits {
		public:
			using NotificationType = model::AliasedAddressNotification;
			using AliasType = Address;
			using InvalidAliasType = MosaicId;

			static auto CreateValidator() {
				return CreateUnlinkAliasedAddressConsistencyValidator();
			}
		};
	}

	DEFINE_COMMON_VALIDATOR_TESTS(UnlinkAliasedAddressConsistency,)

	DEFINE_ALIAS_TRANSACTION_PLUGIN_TESTS(UnlinkAliasedAddressConsistencyValidatorTests, AddressTraits)

	// endregion

	// region unlink aliased mosaic id consistency validator tests

	namespace {
		struct MosaicIdTraits {
		public:
			using NotificationType = model::AliasedMosaicIdNotification;
			using AliasType = MosaicId;
			using InvalidAliasType = Address;

			static auto CreateValidator() {
				return CreateUnlinkAliasedMosaicIdConsistencyValidator();
			}
		};
	}

	DEFINE_COMMON_VALIDATOR_TESTS(UnlinkAliasedMosaicIdConsistency,)

	DEFINE_ALIAS_TRANSACTION_PLUGIN_TESTS(UnlinkAliasedMosaicIdConsistencyValidatorTests, MosaicIdTraits)

	// endregion
}}
