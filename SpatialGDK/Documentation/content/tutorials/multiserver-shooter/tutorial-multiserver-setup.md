<%(TOC)%>
# Multiserver Shooter tutorial
## 1: Set up
### Step 1: Check you have the prerequistes

Before following the Multiserver Shooter tutorial, you need to have followed:

* [Get started 1 - Get the dependencies]({{urlRoot}}/content/get-started/dependencies)
* [Get started 2 - Get and build the GDK's Unreal Engine Fork]({{urlRoot}}/content/get-started/build-unreal-fork)

Once you have done this, you are ready to get going with the Multiserver Shooter tutorial by following the steps below.
<br/>
<br/>
**Let's get started!**<br/>
<br/>

### Step 2: Clone the Unreal GDK Third Person Shooter repository

Clone the Unreal GDK Third Person Shooter repository and checkout the tutorial branch using one of the following commands:

|          |      |
| -------- | ---- |
| **HTTPS:** | `git clone https://github.com/spatialos/UnrealGDKThirdPersonShooter.git -b tutorial`|
| **SSH:** | `git clone git@github.com:spatialos/UnrealGDKThirdPersonShooter.git -b tutorial`|

This repository contains a version of Unreal’s Third Person template that has been ported to the SpatialOS GDK. It includes a character model with a gun and hit detection logic.

> **Note:**  A completed version of this tutorial is available in the `tutorial-complete` branch.

<br/>
### Step 3: Clone the GDK into the `Plugins` directory

1. Navigate to `UnrealGDKThirdPersonShooter\Game` and create a `Plugins` directory.
1. In a terminal window,  change directory to the  `Plugins` directory and clone the [Unreal GDK](https://github.com/spatialos/UnrealGDK) repository using one of the following commands:

|          |      |
| -------- | ---- |
| **HTTPS:** | `git clone https://github.com/spatialos/UnrealGDK.git`|
| **SSH:** | `git clone git@github.com:spatialos/UnrealGDK.git`|

The GDK's [default branch (GitHub documentation)](https://help.github.com/en/articles/setting-the-default-branch) is `release`. This means that, at any point during the development of your game, you can get the latest release of the GDK by running `git pull` inside the `UnrealGDK` directory. When you pull the latest changes, you must also run `git pull` inside the `UnrealEngine` directory, so that your GDK and your Unreal Engine fork remain in sync.

> **Note:**  You need to ensure that the root folder of the Unreal GDK repository is called `UnrealGDK` so its path is: `UnrealGDKThirdPersonShooter\Game\Plugins\UnrealGDK\`.

<br/>

### Step 4: Build dependencies 

In this step, you're going to build the Unreal GDK's dependencies.

1. Open **File Explorer**, navigate to the root directory of the GDK for Unreal repository (`ThirdPersonShooter\Plugins\UnrealGDK\...`), and double-click `Setup.bat`. If you haven't already signed into your SpatialOS account, the SpatialOS developer website may prompt you to sign in. 
1. In **File Explorer**, navigate to the ThirdPersonShooter directory, right-click `ThirdPersonShooter.uproject` and select Generate Visual Studio Project files.
1. In the same directory, double-click `ThirdPersonShooter.sln` to open it with Visual Studio.
1. In the Solution Explorer window, right-click on **ThirdPersonShooter** and select **Build**.
1. When Visual Studio has finished building your project, right-click **ThirdPersonShooter** and select **Set as StartUp Project**.
1. Press F5 on your keyboard or select **Local Windows Debugger** in the Visual Studio toolbar to open your project in the Unreal Editor.<br/>
![Visual Studio toolbar]({{assetRoot}}assets/set-up-template/template-vs-toolbar.png)<br/>
_Image: The Visual Studio toolbar_<br/><br/>
1. In the Unreal Editor, on the GDK toolbar, open the **Schema** drop-down menu and select **Schema (Full Scan)**. <br/>
  ![Toolbar]({{assetRoot}}assets/screen-grabs/toolbar/schema-button-full-scan.png)<br/>
  _Image: On the GDK toolbar in the Unreal Editor, select **Schema (Full Scan)**_<br/>
1. Select **Snapshot** to generate a snapshot.<br/>
![Toolbar]({{assetRoot}}assets/screen-grabs/toolbar/snapshot-button.png)<br/>
_Image: On the GDK toolbar in the Unreal Editor, select **Snapshot**_<br/>

<br/>
### Step 5: Deploy the project locally

In this section you’ll run a [local deployment](https://docs.improbable.io/reference/latest/shared/glossary#local-deployment) of the project. As the name suggests, local deployments run on your development machine (you will run a [cloud deployment](https://docs.improbable.io/reference/latest/shared/glossary#cloud-deployment) later in this tutorial).

1. In the Unreal Editor, on the Unreal toolbar, open the **Play** drop-down menu.<br/>
1. Under **Multiplayer Options**, enter the number of players as **2**.
1. Enter the number of servers as **2**.
1. Ensure the box next to **Run Dedicated Server** is checked.<br/>
![]({{assetRoot}}assets/set-up-template/template-multiplayer-options.png)<br/>
_Image: The Unreal Engine **Play** drop-down menu, with **Multiplayer Options** and **New Editor Window (PIE)** highlighted_<br/><br/>
1. From the Unreal toolbar's **Play** drop-down menu, select **SpatialOS Settings...** to open the SpatialOS Editor Settings panel.
1. In the panel, under the **Launch** drop-down menu, select the following drop-down menus: **Launch configuration file description** > **Workers** > **0**.
1. Locate the **Rectangle grid row count** field beolow this and set it to **2**.
1. In the Unreal Editor, in the SpatialOS GDK toolbar, select **Start** (the green play icon). This opens a terminal window and runs the [`spatial local launch`](https://docs.improbable.io/reference/latest/shared/spatial-cli/spatial-local-launch#spatial-local-launch) command, which starts the [SpatialOS Runtime](https://docs.improbable.io/reference/latest/shared/glossary#the-runtime).
1. It's ready when you see `SpatialOS ready. Access the inspector at http://localhost:21000/inspector`.
1. From the Unreal Editor toolbar, select **Play** to run the game. This starts two SpatialOS server-worker instances and two SpatialOS client-worker instances locally, in your Unreal Editor.
<br/>The two server-worker instances are acting as two Unreal servers and the two client-worker instances are acting as two Unreal game clients (as would be used by two game players).
<br/>(You can find out about workers in the [glossary](https://docs.improbable.io/unreal/alpha/content/glossary#workers).)

Notice that when players shoot each other, their health does not go down. It's not much fun with no skin in the game is it? Let’s fix the health system.
</br>
</br>
**> Next:** 
[2: Replicate health changes]({{urlRoot}}/content/tutorials/multiserver-shooter/tutorial-multiserver-healthchanges)
<br/>
<br/>


<br/>------<br/>
_2019-04-30 Page updated with limited editorial review_