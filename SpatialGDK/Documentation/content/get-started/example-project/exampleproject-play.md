<%(TOC)%>
# The Example Project 
## 4: Play the game

To get playing, you need to set up clients with the Launcher and then share clients using a dedicated URL.

<%(#Expandable title="What is the SpatialOS Launcher?")%>

The Launcher is a distribution tool which downloads and launches game clients for your deployment. You access the Launcher from the Console; use the Console to create a URL to give end-users access to a game client for your game.

Find out more in the [glossary]({{urlRoot}}/content/glossary#launcher).
<%(/Expandable)%>

### Step 1: Set up a client with the Launcher
![img]({{assetRoot}}assets/tutorial/console.png)<br/>
_Image: The SpatialOS Console_


In the Console, Select **Launch** on the left of the page. Then, select the **Launch** button that appears in the center of the page to open the SpatialOS Launcher. The Launcher automatically downloads the game client for this deployment and runs it on your local machine.

![img]({{assetRoot}}assets/tutorial/launch.png)<br/>
_Image: The SpatialOS console launch window_

> **TIP:** Check out the [cloud deployment workflow page]({{urlRoot}}/content/cloud-deployment-workflow) for a reference diagram of this workflow.

Once the client has launched, enter a name for your player character and select **Start** to start playing. <br/>

![img]({{assetRoot}}assets/example-project/example-project-lobby.png)<br/>
_Image: The Example project lobby screen_

### Step 2: Share your game
To share your cloud deployment: 

1. Open the Console and select your deployment by the name you gave it when you ran the `spatial cloud launch` command. 
1. Select **Share** on the right-hand side of the screen. 
1. Accept the terms and conditions.

![img]({{assetRoot}}assets/example-project/example-project-share-tos.png))<br/>

_Image: The SpatialOS Console Share Application terms of service screen_

After you have accepted the terms and conditions, you can send your share URL to other people so they can try out your game. 

![img]({{assetRoot}}assets/example-project/example-project-share-screen.png)<br/>

_Image: The SpatialOS Console Share Application screen_
</br>
</br>
**Congratulations!**</br>
You've sucessfully set up and launched the Example Project using the Deployment Manager. You are now ready to start developing your own games with SpatialOS. 
</br>
</br>
**Next steps:**

* Do more with the Example Project: follow the tutorial on setting up [multiple deployments for session-based games]({{urlRoot}}/content/tutorials/deployment-manager/tutorial-deploymentmgr-intro) using the Example Project. 

* If you have an existing Unreal multiplayer project, you can follow the detailed [porting guide]({{urlRoot}}/content/tutorials/tutorial-porting-guide) to get it onto the GDK.

* If you want to start a project from scratch, follow the set up guide for the [Starter Template]({{urlRoot}}/content/get-started/gdk-template) to set up a blank project using the GDK. 

--------<br/>

_2019-05-23 Page added with editorial review_