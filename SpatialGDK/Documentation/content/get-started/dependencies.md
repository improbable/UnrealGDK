<%(TOC)%>
# Get started
## 1 - Get the dependencies

To start using the GDK for Unreal, you need to ensure you have the correct software installed and that your machine is capable of running Unreal Engine. 

### Step 1: Login

Before you start, make sure you are logged in to your SpatialOS account.

* If you have already signed up to SpatialOS, make sure you are logged into [Improbable.io](https://improbable.io). If you are logged in, you should see your picture in the top right of this page. If you are not logged in, select __Sign in__ at the top of this page and follow the instructions.

* If you have not signed up before, you can sign up [here](<https://improbable.io/get-spatialos>).

### Step 2: Hardware

- Ensure your machine meets the minimum hardware requirements for Unreal Engine. 

Refer to the <a href="https://docs.unrealengine.com/en-US/GettingStarted/RecommendedSpecifications" data-track-link="Clicked UE4 Recommendations|product=Docs|platform=Win|label=Win" target="_blank">Unreal Engine hardware recommendations</a> for further information about the minimum hardware requirements.

- Recommended storage: 60GB+ available space

### Step 3: Network settings

To configure your network to work with SpatialOS, refer to the [SpatialOS network settings](https://docs.improbable.io/reference/latest/shared/setup/requirements#network-settings). 

### Step 4: Software

To build the GDK for Unreal you need the following software installed on your machine:

- **Windows 10,** with Command Prompt or PowerShell.

  - **The GDK for Unreal is currently only supported on Windows 10.** 
- <a href="https://gitforwindows.org" data-track-link="Clicked GIT for Windows|product=Docs|platform=Win|label=Win" target="_blank">**Git for Windows**</a>

  - You need Git for windows to clone the GDK and Unreal Engine GitHub repositories.
- <a href="https://console.improbable.io/installer/download/stable/latest/win" data-track-link="Clicked Download SpatialOS|product=Docs|platform=Win|label=Win" target="_blank">**SpatialOS**</a>
    - This installs the [SpatialOS CLI]({{urlRoot}}/content/glossary#spatialos-command-line-tool-cli), the [SpatialOS Launcher]({{urlRoot}}/content/glossary#launcher), and 32-bit and 64-bit Visual C++ Redistributables.
- The [**DirectX End-User Runtimes (June 2010)**](https://www.microsoft.com/en-us/download/details.aspx?id=8109)

  - You need the DirectX End-User Runtime to run Unreal Engine 4 clients.
- **Visual Studio** <a href="https://visualstudio.microsoft.com/vs/" data-track-link="Clicked VS 2019|product=Docs|platform=Win|label=Win">2019</a> or <a href="https://visualstudio.microsoft.com/vs/older-downloads/" data-track-link="Clicked VS 2017|product=Docs|platform=Win|label=Win">2017</a>.   
    **Note:** You must sign into your Microsoft account to download the Visual Studio 2017 installer.  
    You must also sign up to - Microsoft [Visual Studio Dev Essentials](https://visualstudio.microsoft.com/dev-essentials/) if you do not already have a Visual Studios download  
    
    During the installation of Visual Studio, select the following items in the Workloads tab:
    - **Universal Windows Platform development**<br>
    - **.NET desktop development** <br>
      - You must also select the **.NET Framework 4.6.2 development tools**
    - **Desktop development with C++**<br>
    - **Game development with C++**, including the optional **Unreal Engine installer** component.
- [**v13 clang 7.0.1 toolchain**](https://docs.unrealengine.com/en-US/Platforms/Linux/GettingStarted/index.html)
    - You need the v13 clang 7.0.1 toolchain to compile your server-workers. 

</br>
</br>
**> Next:** [2 - Get and build the GDK’s Unreal Engine Fork]({{urlRoot}}/content/get-started/build-unreal-fork.md)

<br/>

------</br>
_2019-07-22 Page updated with limited editorial review_
