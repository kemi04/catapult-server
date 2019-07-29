import "mosaic/mosaic_types.cats"
import "transaction.cats"

# binary layout for a mosaic supply change transaction
struct MosaicSupplyChangeTransactionBody
	# id of the affected mosaic
	mosaicId = UnresolvedMosaicId

	# supply change direction
	direction = MosaicSupplyChangeDirection

	# amount of the change
	delta = Amount

# binary layout for a non-embedded mosaic supply change transaction
struct MosaicSupplyChangeTransaction
	const uint8 version = 1
	const EntityType entityType = 0x424D

	inline Transaction
	inline MosaicSupplyChangeTransactionBody

# binary layout for an embedded mosaic supply change transaction
struct EmbeddedMosaicSupplyChangeTransaction
	inline EmbeddedTransaction
	inline MosaicSupplyChangeTransactionBody
