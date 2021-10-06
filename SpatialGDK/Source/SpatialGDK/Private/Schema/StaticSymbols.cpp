#include "LoadBalancing/LegacyLoadbalancingComponents.h"
#include "Schema/ActorGroupMember.h"
#include "Schema/ActorOwnership.h"
#include "Schema/ActorSetMember.h"
#include "Schema/AuthorityIntent.h"
#include "Schema/DebugComponent.h"
#include "Schema/GameplayDebuggerComponent.h"
#include "Schema/Interest.h"
#include "Schema/MigrationDiagnostic.h"
#include "Schema/NetOwningClientWorker.h"
#include "Schema/PlayerSpawner.h"
#include "Schema/Restricted.h"
#include "Schema/ServerWorker.h"
#include "Schema/SnapshotVersionComponent.h"
#include "Schema/SpatialDebugging.h"
#include "Schema/SpawnData.h"
#include "Schema/StandardLibrary.h"
#include "Schema/Tombstone.h"
#include "Schema/UnrealMetadata.h"

// Support older C++ standard by defining constexpr static members, this file should be removed once UE 4.25 is no longer supported.
// Comment this section and build UE 4.25 on Linux if you wish to analyze the underlying issue.
#if __cplusplus <= 201402L
namespace SpatialGDK
{
constexpr Worker_ComponentId ActorOwnership::ComponentId;
constexpr Worker_ComponentId ActorGroupMember::ComponentId;
constexpr Worker_ComponentId ActorSetMember::ComponentId;
constexpr Worker_ComponentId DebugComponent::ComponentId;
constexpr Worker_ComponentId AuthorityIntent::ComponentId;
constexpr Worker_ComponentId AuthorityIntentACK::ComponentId;
constexpr Worker_ComponentId GameplayDebuggerComponent::ComponentId;
constexpr Worker_ComponentId Interest::ComponentId;
constexpr Worker_ComponentId MigrationDiagnostic::ComponentId;
constexpr Worker_ComponentId NetOwningClientWorker::ComponentId;
constexpr Worker_ComponentId PlayerSpawner::ComponentId;
constexpr Worker_ComponentId Partition::ComponentId;
constexpr Worker_ComponentId ServerWorker::ComponentId;
constexpr Worker_ComponentId SnapshotVersion::ComponentId;
constexpr Worker_ComponentId SpatialDebugging::ComponentId;
constexpr Worker_ComponentId SpawnData::ComponentId;
constexpr Worker_ComponentId Metadata::ComponentId;
constexpr Worker_ComponentId Position::ComponentId;
constexpr Worker_ComponentId Persistence::ComponentId;
constexpr Worker_ComponentId Worker::ComponentId;
constexpr Worker_ComponentId AuthorityDelegation::ComponentId;
constexpr Worker_ComponentId Tombstone::ComponentId;
constexpr Worker_ComponentId UnrealMetadata::ComponentId;

constexpr Worker_ComponentId LegacyLB_GridCell::ComponentId;
constexpr Worker_ComponentId LegacyLB_Layer::ComponentId;
constexpr Worker_ComponentId LegacyLB_VirtualWorkerAssignment::ComponentId;
constexpr Worker_ComponentId LegacyLB_CustomWorkerAssignments::ComponentId;
} // namespace SpatialGDK
#endif
