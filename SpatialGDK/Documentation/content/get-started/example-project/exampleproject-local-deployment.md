<%(TOC)%>
# The Example Project

## 2.  Launch a local deployment

When you want to try out your game, you need to launch a deployment; the deployment runs the game's functionality. We call the running deployment the game "simulation", and what's happening in it, the "SpatialOS world".

There are two types of deployment: local and cloud.

- A **local deployment** is for testing only. A local deployment runs your game simulation in your Unreal Editor. 
</br>Your game simulation, with its own instance of the SpatialOS Runtime, runs on your development machine. You can run multiple clients in a local deployment, which is useful for fast development iteration.
<%(#Expandable title="What is the SpatialOS Runtime?")%>
There is a Runtime instance for every game simulation. A Runtime instance holds the canononical store of all a game's SpatialOS-relevant object data in the SpatialOS entity database. It handles server and client connections, and coordinates each server and client's write and read access to the entity database.
<%(/Expandable)%>

- A **cloud deployment** runs on remote networked nodes. A node is a single machine used as server hardware. </br>
The cloud is where your game simulation runs when you release it, so you always deploy your released game to the cloud but you can also use a cloud deployment during development to share it with test users and run it at scale. You share your game with test users through the SpatialOS Launcher. (You'll use the Launcher in a later step of this guide.)
<%(#Expandable title="What is the SpatialOS Launcher?")%>
The Launcher is a distribution tool which downloads and launches game clients for your deployment. You installed the Launcher when you [installed SpatialOS on your machine]({{urlRoot}}/content/get-started/dependencies#step-3-software). You access the Launcher from the Console; use it to create a URL to give end-users access to a game client for your game. </br>Find out more in the [glossary]({{urlRoot}}/content/glossary#launcher).
<%(/Expandable)%>

Use local deployments for small-scale tests, to quickly test and iterate on changes to your project. For large-scale tests with several players, use a cloud deployment.


### Step 1: Generate schema and a snapshot

Before you launch a deployment (local or cloud) you must generate schema and a snapshot to set up SpatialOS entities.


1. In the Editor, on the GDK Toolbar, open the **Schema** drop-down menu and select **Schema (Full Scan)**. <br/>
   ![Schema]({{assetRoot}}assets/screen-grabs/toolbar/schema-button-full-scan.png)
   </br></br>
2. Select **Snapshot** to generate a snapshot.<br/>
   ![Toolbar]({{assetRoot}}assets/screen-grabs/toolbar/snapshot-button.png)<br/>

<%(#Expandable title="What is Schema?")%>

Schema is a set of definitions which represent your game’s objects in SpatialOS as entities. Schema is defined in .schema files and written in schemalang by the GDK.</br>
Select **Schema** from the GDK toolbar and the GDK generates schema files and their contents for you, so you do not have to write or edit schema files manually.

You can find out more about schema, including how to generate it from the command line, making schema work with source control, and how to exclude certain directories from schema in the [GDK schema documentation]({{urlRoot}}/content/how-to-use-schema).

<%(/Expandable)%>

<%(#Expandable title="What is an entity?")%>
An entity is the SpatialOS equivalent of an Unreal Actor. It’s made up of a set of SpatialOS components. Each component stores data about the entity. (Note that SpatialOS components are not the same thing as Unreal Actor Components.)
<%(/Expandable)%>

<%(#Expandable title="What is  a snapshot?")%>

A snapshot is a representation of the state of a SpatialOS world at a given point in time. A snapshot stores the current state of each entity’s component data. You start each deployment with a snapshot; if it’s a re-deployment of an existing game, you can use the snapshot you originally started your deployment with, or use a snapshot that contains the exact state of a deployment before you stopped it.

You can find out more about snapshots in the [GDK snapshot documentation]({{urlRoot}}/content/how-to-use-snapshots).

<%(/Expandable)%>

### Step 2: Launch and play

This step shows you how to start one game server and two game clients in a local deployment in your Unreal Editor. </br>
In SpatialOS, game servers are called "server-workers" and game clients are called "client-workers".  


<%(#Expandable title="More about server-workers and client-workers")%>
A server-worker is your game's server-side code. It's the equivalent of a "server" in native Unreal networking.

You can think of client-workers as game clients as they are essentially the same. 

Both server-workers and client-workers are programs that compute a SpatialOS game world: they send updates to and receive updates from the entity database in the SpatialOS Runtime.

You can have different types of server-workers doing different tasks, depending how you have set up your game. You can also set up your game to have more than one instance of a server-worker type. This guide using the Example Project sets up one instance of one server-worker type; a main Unreal server-worker.

You usually only have one type of client-worker in your game but you can have many instances of a client-worker, each representing a game player. The Starter Template has one client-worker type.

<%(/Expandable)%>

To launch a local deployment in your Unreal Editor, set up the networking and run the game:

1. From the Editor toolbar, open the **Play** drop-down menu:</br></br>
    ![Multiplayer Options]({{assetRoot}}assets/set-up-template/template-multiplayer-options.png))<br/>
   _Image: The Unreal Editor toolbar's **Play** drop-down menu, with the relevant options hightlighted_</br></br>
1. To set up the networking:</br>
In the **Multiplayer Options** section of the window:</br>
 * enter the number of players as `2`,</br>
 * check the **Run Dedicated Server** setting and</br>
 * check the **Spatial Networking** setting. </br>
 (The settings might already be checked.)</br></br>
The **Spatial Networking** option is the networking switch; you use this to switch your game's deployment from native Unreal networking to SpatialOS networking.</br></br>


1. Now, run the game: in the **Modes** section of the window, select **New Editor Window (PIE)**.</br>

You are now running one game server and two game clients in a local deployment in your Unreal Editor. You don't need to worry about the loading bar being stuck on 45%; this is just the shaders being compiled for the first time. (If it doesn't run, see **Note** below.)</br>

**What's running?**</br>
You have started one SpatialOS server-worker instance and two SpatialOS client-worker instances locally, in your Unreal Editor. You have also started an instance of the SpatialOS Runtime locally.</br></br>
**What is this doing?**</br>
The server-worker instance is acting as an Unreal server.  The two client-worker instances are acting as two Unreal game clients, as if two game players were playing your game.</br>
You can switch between the two Editor windows to see and interact with each game client.</br></br>
**Note:** If the game does not run automatically after you have selected **New Editor Window (PIE)**, try selecting **Play** on the Editor toolbar. This should to start a local deployment and play the game.<br/><br/>

![]({{assetRoot}}assets/example-project/first-client-launch.png)

### Step 3: Inspect and stop play

When your game is running, you can see how it is running by looking at the Inspector on the SpatialOS Console.

1. From the GDK toolbar, select **Inspector** to open it in your default web browser. 

    ![]({{assetRoot}}assets/screen-grabs/toolbar/inspector-button.png)<br/>
    _Image: GDK toolbar's **Inspector** button_

    <%(#Expandable title="What are the Console and the Inspector?")%>
The **Console** is a web-based tool for managing cloud deployments. It gives you access to information about your games’ SpatialOS project names, the SpatialOS assemblies you have uploaded, the internal state of any games you have running (via the Inspector), as well as logs and metrics. </br>
The **Inspector** is part of the Console. You can use it to explore the internal state of a SpatialOS world. It gives you a real-time view of what’s happening in a local or cloud deployment. <br/><br/>
You can find out more about the Inspector and the Console in the [Glossary]({{urlRoot}}/content/glossary#console).
<%(/Expandable)%>

    Selecting **Inspector** opens the Inspector in your browser:</br></br>
    ![]({{assetRoot}}assets/set-up-template/template-two-client-inspector.png)<br/>
    _Image: The Inspector on the SpatialOS Console_
</br></br>
1. When you’re done, select **Stop** on the Unreal toolbar to stop the client.

    ![]({{assetRoot}}assets/toolbar/stop-button-native.png)<br/>
    _Image: Unreal toolbar's **Stop** button_</br></br>

    This stops the running client and server-worker instances but keeps your deployment running.

### Step 4: Iterate on game development

As you iterate on development of your game (by modifying classes or Blueprints, for example), you will make changes changes to replicated components - this requires incremental schema regeneration. 

To do this:

1. Select the **Schema** button in the GDK toolbar. (Note you do not need to do a full scan schema generation for incremental changes.)

    ![Stop]({{assetRoot}}assets/screen-grabs/toolbar/schema-button.png)<br/>
    _Image: GDK toolbar's **Schema** button_</br></br>

    Once you've regenerated schema, the GDK restarts the running deployment with the new schema.

    If you haven't modified anything related to replication, you don't need to regenerate schema and SpatialOS continues to use the running deployment. </br></br>

1. To test your changes, select **Play** on the Unreal toolbar; this starts your game's clients and server-worker instances.

<%(#Expandable title="Local deployment workflow summary")%>
There is a sumary on the [Local deployment workflow]({{urlRoot}}/content/local-deployment-workflow) page. It is the same as the one here.
 <%(Lightbox image="https://docs.google.com/drawings/d/e/2PACX-1vQCTOucXKMkDJ3-Vpg17_tpUS7IxOXD6Mps-FzWe2tQl3vw5alQPngCnw339cFy3u2NvrcBxhYASKsS/pub?w=710&h=1033")%>

<%(/Expandable)%>

### Step 5: Stop your deployment

To fully stop your local deployment, select **Stop** in the GDK toolbar.<br/></br>
![Stop]({{assetRoot}}assets/screen-grabs/toolbar/stop-button.png)<br/>
 _Image: GDK toolbar's **Stop** button_</br></br>

#### **> Next:** [3: Launch a cloud deployment]({{urlRoot}}/content/get-started/example-project/exampleproject-cloud-deployment)

<br/>
<br/>------------<br/>
_2019-07-22 Page updated with editorial review: Added loading bar note._
_2019-07-22 Page updated with limited editorial review._
