<%(TOC)%>
# Multiserver Shooter tutorial
## 4: Test changes in the cloud
### Step 1: Build your workers

## Step 4: Test your changes in the cloud

### Build your assemblies

An assembly is what’s created when you run `BuildWorker.bat`. They’re `.zip` files that contain all the files that your game uses when running in the cloud.

**Note:** You must close the Unreal Editor before building your workers. If the Editor is open when you try to build your workers the command will fail.

**Note:** Unreal GDK projects default to using [Spatial for networking](). However, if the `bSpatialNetworking` option is present in your `DefaultGame.ini` configuration file (located in `<ProjectRoot>\Game\Config` directory), ensure that it is set to `True` (as in, `bSpatialNetworking=True`) to enable networking with Spatial for your cloud deployment.

1. In a terminal window, change directory to the root directory of the Third-Person Shooter repository.
1. Build a server-worker assembly by running: `Game\Plugins\UnrealGDK\SpatialGDK\Build\Scripts\BuildWorker.bat ThirdPersonShooterServer Linux Development ThirdPersonShooter.uproject`
1. Build a client-worker assembly by running: `Game\Plugins\UnrealGDK\SpatialGDK\Build\Scripts\BuildWorker.bat ThirdPersonShooter Win64 Development ThirdPersonShooter.uproject`

<br/>
### Step 2: Upload your game

1. In File Explorer, navigate to `UnrealGDKThirdPersonShooter\spatial` and open `spatialos.json` in a text editor.
1. Change the `name` field to the name of your project. You can find this in the [Console](https://console.improbable.io). It’ll be something like `beta_someword_anotherword_000`.
    ![]({{assetRoot}}assets/tutorial/project-name.png)
1. In a terminal window, change directory to `UnrealGDKThirdPersonShooter\spatial\` and run `spatial cloud upload <assembly_name>`, where `<assembly_name>` is a name of your choice (for example `myassembly`). A valid upload command looks like this:

```
spatial cloud upload myassembly
```

> **Note:** Depending on your network speed it may take a little while (1-10 minutes) to upload your assembly.

<br/>
### Step 3: Launch a cloud deployment

The next step is to launch a cloud deployment using the assembly that you just uploaded. This can only be done through the SpatialOS command-line interface (also known as the [SpatialOS CLI]({{urlRoot}}/content/glossary#spatialos-command-line-tool-cli).

When launching a cloud deployment you must provide three parameters:

* **the assembly name**, which identifies the worker assemblies to use.
* **a launch configuration**, which declares the world and load balancing configuration.
* **a name for your deployment**, which is used to label the deployment in the [Console]({{urlRoot}}/content/glossary#console).

1. In a  terminal window, in the same directory you used to upload your game, run: `spatial cloud launch --snapshot=snapshots/default.snapshot <assembly_name> two_worker_test.json <deployment_name>` 
    <br/>where `assembly_name` is the name you gave the assembly in the previous step and `deployment_name` is a name of your choice. A valid launch command would look like this:

```
spatial cloud launch --snapshot=snapshots/default.snapshot myassembly two_worker_test.json mydeployment
```

> **Note:** This command defaults to deploying to clusters located in the US. If you’re in Europe, add the `--cluster_region=eu` flag for lower latency.

<br/>
### Step 4: Play your game

![]({{assetRoot}}assets/tutorial/console.png)

When your deployment has launched, SpatialOS automatically opens the [Console](https://console.improbable.io) in your browser.

1. In the Console, Select the **Launch** button on the left of the page, and then click the **Launch** button that appears in the centre of the page. The SpatialOS Launcher, which was installed along with SpatialOS, downloads the game client for this deployment and runs it on your local machine.
    ![]({{assetRoot}}assets/tutorial/launch.png)
1. Once the client has launched, enter the game and fire a few celebratory shots - you are now playing in your first SpatialOS cloud deployment!

> **TIP:** Check out the [cloud deployment workflow page]({{urlRoot}}/content/cloud-deployment-workflow) for a reference diagram of this workflow.

<br/>
### Step 5: Invite your friends

1. To invite other players to this game, head back to the Deployment Overview page in your [Console](https://console.improbable.io), and select the **Share** button.
1. Share the generated link with your friends!

When you’re done shooting your friends, you can click the **Stop** button in the [Console](https://console.improbable.io) to stop your deployment.

<br/>
** Next steps:**

* If you want to create a new game using the GDK, you should get the [Starter Template]({{urlRoot}}/content/get-started/starter-template/get-started-template-intro). 
* If you want to port your existing game to the GDK, follow the [Porting guide]({{urlRoot}}/content/tutorials/tutorial-porting-guide).


<br/>------<br/>
_2019-03-20 Page updated with limited editorial review_