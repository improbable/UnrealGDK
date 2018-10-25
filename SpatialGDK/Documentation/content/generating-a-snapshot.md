# How to generate a snapshot

If you're not familiar with snapshots in the context of SpatialOS, please look at the [full snapshot documentation](https://docs.improbable.io/reference/latest/shared/operate/snapshots) (SpatialOS documentation).

The GDK snapshots contain three kinds of entities: critical entities, stably named replicated actors and placeholders.

### Startup actors

Startup actors are replicated actors that have been placed in a level. We write these startup actors to the snapshot so we can ensure that only one of each entity is spawned when launching multiple server-workers.

### Critical entities

Critical entities are entities which are used for functionality critical to the GDK and are never deleted. They are saved into the initial snapshot and must always exist when launching a deployment.

Currently the critical entities are:

* `SpatialSpawner` - an entity with the `PlayerSpawner` component which has a command. Connecting client-workers use this entity to spawn their player.
* `GlobalStateManager` - an entity with the `GlobalStateManager` component which has a map of singleton classes to entity IDs. This entity is used for orchestrating the replication of [Singleton Actors]({{urlRoot}}/content/singleton-actors.md).

### Placeholders

These entities exists only to set up server-worker boundaries in a way that is easy to test in a scenario with two server-workers. They will not spawn as actors when checked out and serve no purpose within the GDK. For most intents and purposes, you can safely ignore them.

## Generating a snapshot

To generate a snapshot, use the **Snapshot** button on the SpatialOS GDK for Unreal toolbar in the Unreal Editor:

 ![Snapshot]({{assetRoot}}assets/screen-grabs/snapshot.png)

 This creates a snapshot called `default.snapshot` in `spatial\snapshots`.

You need to regenerate snapshots when:
1. Generating schema for a new class.
1. Modifying replicated properties or RPC signatures for any class whose schema was previously generated.
1. Placing or removing replicated actors in the level.
1. Modifying replicated values on placed replicated actors.
1. Adding or removing a singleton class.
