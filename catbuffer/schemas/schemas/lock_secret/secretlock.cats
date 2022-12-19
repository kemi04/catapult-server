import "lock_secret/lockhashalgorithm.cats"
import "transaction.cats"

# binary layout for a secret lock transaction
struct SecretLockTransactionBody
	# lock mosaic
	mosaic = UnresolvedMosaic

	# number of blocks for which a lock should be valid
	duration = BlockDuration

	# hash alghoritm
	hashAlgorithm = LockHashAlgorithm

	# secret
	secret = Hash512

	# recipient of the locked mosaic
	recipient = UnresolvedAddress

# binary layout for a non-embedded secret lock transaction
struct SecretLockTransaction
	const uint8 version = 1
	const EntityType entityType = 0x4152

	inline Transaction
	inline SecretLockTransactionBody

# binary layout for an embedded secret lock transaction
struct EmbeddedSecretLockTransaction
	inline EmbeddedTransaction
	inline SecretLockTransactionBody
