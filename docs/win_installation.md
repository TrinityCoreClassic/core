# Software Requirements Overview & Links



| Software                               | Link                                                                                         | Notes                                                |
|----------------------------------------|----------------------------------------------------------------------------------------------|------------------------------------------------------|
| **GIT**                                | [Download](https://git-scm.com/downloads/win)                                                | Version control system                               |
| **Visual Studio 2022 Community**       | [Download](https://visualstudio.microsoft.com/downloads/)                                    | Choose Desktop development with C++ workload         |
| **CMake**                              | [Download](https://cmake.org/download/)                                                      | Required for generating project files                |
| **MySQL Server 8.0 Community Edition** | [Download](https://dev.mysql.com/downloads/windows/installer/8.0.html)                       | Database server                                      |
| **MySQL Client**                       | [Download](https://www.heidisql.com/download.php)                                            | Good for beginners                                   |
| **Boost**                              | [Download](https://www.boost.org/releases/1.84.0/)                                           | Use Boost 1.78.0–1.84.0 (newer not confirmed working)|
| **OpenSSL**                            | [Download](https://slproweb.com/products/Win32OpenSSL.html)                                  | Latest version                                       |  
| **Notepad++** (optional)               | [Download](https://notepad-plus-plus.org/downloads/)                                         | Optional but handy for editing text files            |                                 
 <br><br> 
  

# Installing & preparing required software

1. Install GIT. Just click next on every step unless you know what you're doing.   

2. Install Visual Studio 22 Community. Make sure you toggle the Desktop development for C++ workload for installation.  
![Alt text](https://raw.githubusercontent.com/TrinityCoreClassic/core/refs/heads/vanilla_classic/docs/visualstudio-install.png)

3. Install the latest version of CMake. We'll get into more details later.  

4. Install MySQL Server Community Edition. You only need to install server. I suggest using root as username for now, but make a secure password and don't forget it.  

5. Install a MySQL Client. This is for managing the databases. I recommend HeidiSQL as it's easy and beginner friendly.

Open up HeidiSQL and make a new session. Name it TrinityCoreClassic and type **127.0.0.1** right of Hostname\IP if it doesn't already say so. Insert username (root) and the password you gave when installing MySQL server in step 4. Click **Open**.  

![Alt text](https://raw.githubusercontent.com/TrinityCoreClassic/core/refs/heads/vanilla_classic/docs/heidsql.png)

I highly recommend making a new user now with a distinctive username and password that you will use from now on for connecting to your WoW Server and managing your MySQL server for security reasons. 

Click **Tools** -> **User Manager**

![Alt text](https://raw.githubusercontent.com/TrinityCoreClassic/core/refs/heads/vanilla_classic/docs/heidsql2.png).

In addition, tick these boxes on\off on your new MySQL user that you just made. It'll bolster the security further more. Hit **Save**.

![Alt text](https://github.com/TrinityCoreClassic/core/blob/vanilla_classic/docs/heidsql3.png?raw=true)

6. Install Boost. 1.78.0 to 1.84.0 is supported. Make sure you've downloaded the Windows binaries for Visual Studio 22 **boost_1_84_0-msvc-14.3-64.exe**.

After installing it you'll have to add a system variable path pointing towards where you installed it.   
**Settings** -> **System** -> **About** (bottom) -> **Advanced system settings**.  

![Alt text](https://raw.githubusercontent.com/TrinityCoreClassic/core/refs/heads/vanilla_classic/docs/boost.png)

7. Install the latest version of OpenSSL. If you install OpenSSL to the default location **(C:\Program Files\OpenSSL-Win64)** you won't need to set a system variable path as the core will auto-detect it. If not installed to the default location you'll have to add another system variable path pointing towards where you installed it.  
**Settings** -> **System** -> **About** (bottom) -> **Advanced system settings**.  

![Alt text](https://github.com/TrinityCoreClassic/core/blob/vanilla_classic/docs/openssl.png?raw=true)

# Cloning & compiling the core

In this guide I'll be using **C:\TrinityCoreClassic** as my cloning destination path as an example. You can choose whatever you'd like. 

1. Make a folder on your computer. This will be where you clone the core to.

2. Open the command-line prompt. Just type **cmd** into the start menu and open it.  

3. Type **cd C:\TrinityCoreClassic** to browse into the destination where you'd you want the core to be cloned to.    

If your command-line prompt window looks like the picture below, you're ready to clone the core.    

![Alt text](https://github.com/TrinityCoreClassic/core/blob/vanilla_classic/docs/cloning.png?raw=true)

Now type **git clone --branch vanilla_classic --single-branch https://github.com/TrinityCoreClassic/core.git** and hit enter and wait.   

When it is done cloning there will be a subfolder named **core** in **C:\TrinityCoreClassic**. This is where the core is.  

4. You will now build the project and make it ready to get compiled.  

Open CMake and click **Browse source** and browse your way to the core folder inside **C:\TrinityCoreClassic** and click **Select Folder**.  
Now make a folder inside **C:\TrinityCoreClassic** named **build**. Once done, click **Browse build** in CMake and browse your way to the build folder. Click **Select Folder**. Click **Configure**. A little window should pop up. Click **Finish**.  

![Alt text](https://github.com/TrinityCoreClassic/core/blob/vanilla_classic/docs/cmake2.png?raw=true)  

You should now see a lot of red as shown in the picture below. This is normal as long as you don't see any error messages. Click **Configure** once more.

![Alt text](https://github.com/TrinityCoreClassic/core/blob/vanilla_classic/docs/cmake3.png?raw=true)  

Click **Generate**. Click **Open Project**. This will launch Visual Studio 22. If you get prompted, choose to open it with Visual Studio 22.  

![Alt text](https://github.com/TrinityCoreClassic/core/blob/vanilla_classic/docs/cmake4.png?raw=true)

6. You should now be inside Visual Studio ready to compile the project.  

At the top left where it says Debug, change it to Release.  

![Alt text](https://raw.githubusercontent.com/TrinityCoreClassic/core/refs/heads/vanilla_classic/docs/visual%20studio-compile.png)

To the top right, right click Solution and click **Build Solution**. How long this takes depends on your CPU, but should not be too long.  

![Alt text](https://raw.githubusercontent.com/TrinityCoreClassic/core/refs/heads/vanilla_classic/docs/visualstudio%20-compile2.png)

If the compilation was a success, you should see this:  

![Alt text](https://github.com/TrinityCoreClassic/core/blob/vanilla_classic/docs/visualstudio-compile3.png?raw=true)

Your core is now compiled to **C:\TrinityCoreClassic\build\bin\Release**

# Creating and populating the core map database (mmaps/vmaps etc)

1. Now you will be building the map database for the core.  
Browse to **C:\TrinityCoreClassic\build\bin\Release**, here you'll need to copy the following 4 files to where your WowClassic.exe is in your 1.14.0 Client is:  

- **mapextractor.exe**  
- **mmaps_generator.exe**  
- **vmap4assembler.exe**  
- **vmap4extractor.exe**  

Browse to **C:\TrinityCoreClassic\core\contrib**, here you'll need to copy the following file to the same location as mentioned above.
**run_extractors.bat**

2. Once those are copied, browse to your client where you copied those files and run **run_extractors.bat**.  
This will generate the map files for the core. This **will** take some time, depending on your CPU.  

3. Once the map files are generated you should be seeing these folders in your Client.  

- **Buildings**  
- **cameras**  
- **dbc**  
- **gt**  
- **maps**  
- **mmaps**  
- **vmaps**  

Move those to where you compiled your core **C:\TrinityCoreClassic\build\bin\Release**.  

# Additional files required

1. Browse to **C:\Program Files\MySQL\MySQL Server 8.0\lib** and copy the following file to where you compiled the core **C:\TrinityCoreClassic\build\bin\Release**:
- **libmysql.dll**

2. Go to **https://github.com/TrinityCoreClassic/core/releases** and download the latest **TCC.world.xxxx.sql** release and extract the sql file to **C:\TrinityCoreLassic\sql\base**.  

# Server setup

1. Browse to where you compiled the core **C:\TrinityCoreClassic\build\bin\Release** and copy the following 2 files to where you are now:  
- **bnetserver.conf.dist**  
- **worldserver.conf.dist**

Rename these 2 files to:  
- **bnetserver.conf**  
- **worldserver.conf**

2. Open **bnetserver.conf** with a text editor and locate the line shown below on the picture. 

![Alt text](https://raw.githubusercontent.com/TrinityCoreClassic/core/refs/heads/vanilla_classic/docs/bnetconf.png)

Change **trinity;trinity** to the new username you made earlier in HeidiSQL + the password you set.  
Example: **yourusername;yourpassword**  

Open **worldserver.conf** with a text editor and locate the line shown below on the picture.  

![Alt text](https://raw.githubusercontent.com/TrinityCoreClassic/core/refs/heads/vanilla_classic/docs/worldserverconf.png)
And do the exact same thing you did to **bnetserver.conf**.  

3. Start **worldserver.exe**. When starting it for the very first time it will attempt to build the MySQL databases. You should be greeted with this screen below:  

![Alt text](https://github.com/TrinityCoreClassic/core/blob/vanilla_classic/docs/worldserver.exe.png?raw=true)

Type **y** every time it asks if you want to create a database (tcc_auth, tcc_characters, tcc_hotfixes and tcc_world) and it'll automatically populate those databases, assuming you've done everything correct.  

When it is done close the window.

4. Start **bnetserver.exe** and **worldserver.exe** (again) and keep them open.

5. Now you need to create an account that you will be using to log into the game with + give yourself server admin privileges.

You should have 2 command-line windows open now. Go into the **worldserver.exe** command-line window and type the following 2 highlighted commands:

**bnetaccount create yourusername@yourusername yourpassword**  
**gm set account gmlevel 1#1 3 -1**  

(for reference: 1#1 = tcc_auth.account.id#tcc_auth.realmlist.id)

# Client Setup

 1. You need the Arctium Launcher for 1.14.x clients now.

Copy it into your client where **WowClassic.exe** is and right click drag it and make a shortcut of it.
Right click the shortcut -> Properties and add the argument   **--version=ClassicEra** next to where it says target.

It should look like this:
**"C:\yourWoWfolder\_classic_era_\Arctium WoW Launcher.exe" --version=ClassicEra**

2. Browse to **C:\yourWoWfolder\_classic_era_\WTF** and open **Config.wtf** in a text editor.

Here you'll need to change **SET portal "EU"** to **Set portal "127.0.0.1"**

# **Congratulations!** 

You've just compiled your very own Classic WoW server!
