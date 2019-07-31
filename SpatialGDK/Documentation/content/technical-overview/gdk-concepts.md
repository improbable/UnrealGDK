<%(TOC)%>

> This page assumes that you’re familiar with Unreal Engine, but not with SpatialOS.

# GDK concepts

## Unreal Engine and GDK concept mapping
Key concepts in Unreal Engine map to concepts in the GDK, as shown in the table below.

| Unreal Engine | GDK | More information |
| --- | --- | --- |
| Replicated Actor | Entity | An entity is made up of a set of SpatialOS components. Each component stores data about the entity. Note that SpatialOS components are **not** the same thing as Unreal Actor Components. |
| Replicated property | SpatialOS component’s property | A type of data stored in a SpatialOS component.|
| RPC | SpatialOS component’s command or event | A type of data stored in a SpatialOS component. Note that a SpatialOS component’s command is **not** the same as an Unreal command. |
| Owning connection | EntityACL component | |
| Conditional property replication | Dynamic `component_delivery` filter | |
| Server | Server-worker instance | You can have multiple server-worker instances running the cloud element of your game. |

You can find out more about [entities]({{urlRoot}}/content/glossary#entity), SpatialOS [components]({{urlRoot}}/content/glossary#spatialos-component) and their properties, commands, and events, as well as [server-workers]({{urlRoot}}/content/glossary#worker), in the [glossary]({{urlRoot}}/content/glossary).

## GDK for Unreal concepts
We’ve introduced some new concepts to facilitate the fact that SpatialOS enables you to spread computation between multiple servers - known as “server-worker instances” in SpatialOS.

### Offloading
Offloading is the new architecture that the SpatialOS GDK for Unreal provides to allocate the authority of specific Actor groups from the main Unreal server worker instance to a different worker instance. By using offloading, you can save the resources of the main Unreal server-worker instance when you want to build richer game features.

<%(Lightbox image="{{assetRoot}}assets/offloading-diagram.png")%>
_Offloading: Offloaded Unreal server-worker instance has authority only over Red Actors and the Main Unreal server-worker instance that runs major game systems has authority over all Actors except the Red Actors._

For more information, see [Offloading overview]({{urlRoot}}/content/workers/offloading-concept).

### Actor groups
To facilitate offloading, we've created the concept of Actor groups to help you configure what Actor types that a given server-worker type has authority over. In the Unreal Editor, you can create Actor groups, assign Actor classes to a group, and then assign each group to a server-worker type.

Before you start to use offloading in your game, ensure that you’re familiar with the [best practices for using offloading]({{urlRoot}}/content/workers/offloading-concept#best-practices) and [offloading workflow]({{urlRoot}}/content/workers/set-up-offloading).

### Zoning
Because the GDK uses SpatialOS networking, you can have multiple server-worker instances simulating your game world. This allows you to extend the size of the world.

We call this _zoning_ - splitting up the world into zones, known as “areas of authority”. Each area is simulated by one server-worker instance. This means that only one server-worker instance has authority to make updates to SpatialOS components at a time.

> Support for zoning is currently in pre-alpha. We invite you to try out the [Multiserver Shooter tutorial]({{urlRoot}}/content/tutorials/multiserver-shooter/tutorial-multiserver-intro) and learn about how it works, but we don’t recommend you start developing features that use zoning yet.

### Cross-server RPCs
To facilitate zoning, we created the concept of a cross-server RPC to make updates to Actors, known as “entities” in SpatialOS. This is a type of RPC that enables a server-worker instance  which does not have authority over an entity to tell the server-worker instance that does have authority over that entity to make an update to it. This is necessary if you’re using zoning because areas of authority mean that one server-worker instance can't make updates to every entity in the world; it can make updates only to the entities in its area of authority.

Player 1 and Player 2 are player entities in different areas of authority.

When Player 1 shoots at Player 2, the server-worker instance that has authority over Player 1 (server-worker A) invokes a cross-server RPC.

SpatialOS sends this to the server-worker instance that has authority over Player 2 (server-worker B). Server-worker B then executes the RPC.

<%(Lightbox image="{{assetRoot}}assets/screen-grabs/shooting-across-boundaries.png")%>
_Cross-server RPC: Player 1’s action affects Player 2, even though they are in different areas of authority being updated by different server-worker instances._ 

You set up a cross-server RPC in the same way as you would set up any other RPC within Unreal.

Here’s an example of what one might look like:

```
UFUNCTION(CrossServer)
void TakeDamage(int Damage);
```

For more information, see the documentation on [cross-server RPCs]({{urlRoot}}/content/cross-server-rpcs).

### Actor handover
If your game uses zoning, you need to make sure that entities can move seamlessly between areas of authority and the relevant server-worker instances can simulate them. 

In Unreal’s single-server architecture, authority over an Actor stays with the single server; an Actor’s properties never leave the server’s memory. With multiple server-worker instances in SpatialOS, authority needs to pass from one server-worker instance to another as an Actor moves around the game world. Passing authority, known as “Actor handover”, allows the second server-worker instance to continue where the first one left off. You set this up by adding the `Handover` tag to the Actor’s properties. 

> * Replicated Actors equate to “entities” in SpatialOS, so we refer to them as “entities” when we’re talking about what happens to them in the GDK, and “Actors” when we’re talking about what you need to do with them in Unreal. 
> 
> * Replicated Actor properties map to “properties” in an entity’s components in SpatialOS.

<%(Lightbox image="{{assetRoot}}assets/screen-grabs/moving-across-boundaries.gif")%>


 _An AI following a player across the boundary between two server-worker instances' areas of authority. To demonstrate Actor handover, the AI changes its material every time authority is handed over._

See the [Multiserver Shooter tutorial]({{urlRoot}}/content/tutorials/multiserver-shooter/tutorial-multiserver-intro) for a tutorial that demonstrates this functionality.

For more information, see the documentation on [Actor handover]({{urlRoot}}/content/actor-handover).

### Singleton Actors
You can use a Singleton Actor to define a “single source of truth” for operations or data across a game world that uses zoning. You can only have one instance of an entity that represents a Singleton Actor per game world.

You create a Singleton Actor by tagging an Actor with the `SpatialType=Singleton` class attribute. For example, if you are implementing a scoreboard, you probably want only one scoreboard in your world, so you can tag the scoreboard Actor as a Singleton Actor.

For more information, see the documentation on [Singleton Actors]({{urlRoot}}/content/singleton-actors).

### Actor Throttling
We have introduced a concept of 'Actor Throttling' in order to optimize server performance when using the Unreal GDK, it can be accessed via the [Runtime Settings]({{urlRoot}}/content/unreal-editor-interface/runtime-settings). With the Unreal GDK it is possible to have multiple servers simulating on the same game world, this means that no one server could have a full view of the entire world at once. Additionally, servers in the Unreal GDK replicate actors only to a single connection to SpatialOS rather than to each individual client as in native Unreal. To accommodate this, in order to keep the state of the SpatialOS simulation accurate, all actors which are in a particular server's view are marked as relevant for replication on each frame. This can however cause performance issues if there are say, 3000 actors on a single server. 

The concept of Actor Throttling allows you to set a simple limit on the number of actors which can be replicated to SpatialOS in each frame and can help boost server performance. The number set in the throttle will dictate how many actors can be replicated, with the highest priority actors (using Unreal's actor priority system) being replicated first.

We recommend using Actor Throttling with a combination of Unreal's Net Update Frequency, as this will limit the number of actors which are added to the 'ConsiderList' for replication in order to optimize your servers performance. Experiment with the limit by setting it high and then lowering over time until you see client latency issues in a realistic game scenario. 

## Non-Unreal computation
By default, the GDK uses a single Unreal server-worker type (the template for a server-worker instance) to handle all server-side computation. However, you can set up additional server-worker types that do not use Unreal or the GDK. 

You can use these non-Unreal server-worker types to modularize your game’s functionality so you can re-use the functionality across different games. For example, you could use a non-Unreal server-worker type written in Python that interacts with a database or other third-party service, such as [Firebase](https://firebase.google.com/) or [PlayFab](https://playfab.com/).

For more information, see the documentation on [non-Unreal server-worker types]({{urlRoot}}/content/workers/non-unreal-server-worker-types).


<br/>------------<br/>
_2019-07-31 Page updated with limited editorial review: Actor Throttling added_<br/>
_2019-07-26 Page updated with limited editorial review: offloading, Actor group added_<br/>
_2019-04-11 Page updated with limited editorial review_<br/>
_2019-04-25 Page added with editorial review_
[//]: # (TODO: https://improbableio.atlassian.net/browse/DOC-1142)
------------
