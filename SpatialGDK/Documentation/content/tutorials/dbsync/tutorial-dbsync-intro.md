# Tutorials and guides

## \[Experimental\] Database sync worker

<%(Callout type="warn" message="The Database Sync Worker Example is experimental, as is this tutorial. We have made the tutorial and feature available to gather feedback to help us shape this functionality.

Do not use the feature for production game development: we do not support it or its tutorial and are not committed to further developing this functionality. See the GDK's [roadmap](https://github.com/spatialos/UnrealGDK/projects/1) on GitHub for information on features in development.")%>
  

	

This tutorial takes you through how to integrate the [Database Sync Worker Example](https://github.com/spatialos/database-sync-worker) in a GDK project and use it to store persistent data outside of a SpatialOS deployment.

The Database Sync Worker is a SpatialOS server-worker designed to easily synchronize and persist cross-session game data (such as player inventories) between SpatialOS and an external database.

This tutorial uses the GDK’s [Example Project](https://github.com/spatialos/UnrealGDKExampleProject), where the Database Sync Worker will synchronise players’ “All Time Kills” and “All Time Deaths” counts in a local Postgres database.

<%(Lightbox title="DB Sync Worker Example Project K/D Count" image="{{assetRoot}}assets/dbsync/dbsync-kd-exampleproject.gif")%>

## Prerequisites

Before starting this tutorial you _**must**_ follow:

- [Get started 1 - Get the dependencies]({{urlRoot}}/content/get-started/dependencies)
- [Get started 2 - Set up the fork and plugin]({{urlRoot}}/content/get-started/build-unreal-fork)

This tutorial assumes that you are familiar with the basic development workflows of the GDK from the "Set up a project" guides: [Example Project]({{urlRoot}}/content/get-started/example-project/exampleproject-intro), [Starter Template]({{urlRoot}}/content/get-started/starter-template/get-started-template-intro).

Additionally, to communicate with the Database Sync Worker, you will be sending SpatialOS commands and listening to events, so a basic understanding of [SpatialOS messaging](https://docs.improbable.io/reference/latest/shared/design/object-interaction) is required.
(This is something the GDK typically abstracts from you when you build Unreal workers - in this case we will be integrating a C# worker using the [SpatialOS Worker SDK](https://docs.improbable.io/reference/latest/shared/glossary#worker-sdk) directly so these concepts are important to understand.)

</br>
#### **> Next:** [1: Set up]({{urlRoot}}/content/tutorials/dbsync/tutorial-dbsync-setup)
</br>

<br/>------<br/>
_2019-11-25 Page updated without editorial review: experimental wording_<br/>
_2019-07-31 Page added without editorial review_
[//]: # (TODO: https://improbableio.atlassian.net/browse/DOC-1248)
