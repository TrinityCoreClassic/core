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
![Alt text](https://raw.githubusercontent.com/TrinityCoreClassic/core/refs/heads/vanilla_classic/docs/Screenshot%202025-08-09%20113033.png)

3. Install the latest version of CMake. We'll get into more details later.  

4. Install MySQL Server Community Edition. Only need to install server. I suggest using root as username for now but make a secure password and don't forget it.  

5. Install a MySQL Client. This is for managing the databases. I recommend HediSQL as it's easy and beginner friendly. Open up HeidiSQL and make a new session. Name it TrinityCoreClassic and type "127.0.0.1" right of Hostname\IP if it doesn't already say so. Insert username (root) and the password you gave when installing MySQL server in step 4. Now click "Open".  
LINK
I highly recommend making a new user now with a distinctive username and password that you will use from now on for connecting to your WoW Server and managing your MySQL server security reasons. Tick the boxes according to the picture below and save your new user. Use these credentials from now on when managing your MySQL server unless you know what you're doing.  
LINK

6. Install Boost. 1.78.0 to 1.84.0 is supported. Make sure you've downloaded the Windows binaries for Visual Studio 22 (example: **boost_1_84_0-msvc-14.3-64.exe**). After installing it you'll have to add a system variable path pointing towards where you installed it.  
Settings -> System -> About (bottom) -> Advanced system settings.  
LINK

7. Install the latest version of OpenSSL. If you install OpenSSL to the default location **(C:\Program Files\OpenSSL-Win64)** you won't need to set a system variable path as the core will auto-detect it. If not installed to the default location you'll have to add another system variable path poiting towards where you installed it.  
Settings -> System -> About (bottom) -> Advanced system settings.  
LINK

# Cloning & installing the core

1. Make a folder on your computer. This will be where you clone the core to. For this guide I'll be using **C:\TrinityCoreClassic** as an example.  

2. Open the command-line promt. Just type "cmd" into the start menu and click it.  

3. Type **cd C:\TrinityCoreClassic** to browse into the destination where'd you want the core to be cloned to.    
If your command-line prompt window looks like the picture below, you're ready to clone the core.    
LINK  
Now type **git clone --branch vanilla_classic --single-branch https://github.com/TrinityCoreClassic/core.git** and hit enter and wait.   
When it is done cloning there will be a subfolder named **core** in **C:\TrinityCoreClassic**. This is where the core is.  

4. You will now build the project and make it ready to get compiled.  
Open CMake and click **Browse source** and browse your way to the core the **core** folder inside **C:\TrinityCoreClassic** and click select folder.  
Now make a folder inside **C:\TrinityCoreClasic** named **build**. Once done, click **Browse build** in CMake and browse your way to the build folder and click select folder.  
LINK  

5. Now click Configure and click Finish. You should now see a lot of red, this is normal as long as you don't get any error messages.  
LINK   
Click Configure once more. Now all the red is gone.  
LINK  
Click Generate  
LINK  
Click Open Project. This will launch Visual Studio. If you get prompted, choose to open it with Visual Studio 22.  

6. You should now be inside Visual Studio ready to compile the project.  
At the top left where it says Debug, change it to Release.  
LINK  
To the top right, right click Solution and click Build Solution. How long this takes depends on your CPU, but should not be too long.  
LINK  
If the compilation was a success, you should see this:  
LINK  
Your core is now installed to **C:\TrinityCoreClassic\build\bin\Release**

# Creating and poulating the core map database (mmaps/vmaps etc)

1. Now you will be building the map database for the core.  
Browse to **C:\TrinityCoreClassic\build\bin\Release**, here you'll need to copy the following 5 files to where your WowClassic.exe is in your 1.14.0 Client is:  
mapextractor.exe  
mmaps_generator.exe  
vmap4assembler.exe  
vmap4extractor.exe  

Browse to **C:\TrinityCoreClassic\core\contrib**, here you'll need to copy the following file to the same location as mentioned above.
run_extractors.bat

2. Once those are copied, browse to your client where you copied those files and run **run_extractors.bat**.  
This will generate the map files for the core and **will** take some time, depending on your CPU.  

3. Once the map files are generated you should be seeing these folders in your Client.  
Buildings  
cameras  
dbc  
gt  
maps  
mmaps  
vmaps  

Move those to where you compiled your core **C:\TrinityCoreClassic\build\bin\Release**.  

# Additional files required

1. Browse to **C:\Program Files\MySQL\MySQL Server 8.0\lib** and copy over libmysql.dll and drop it into where you compiled your core **C:\TrinityCoreClassic\build\bin\Relase**.  

2. Go to https://github.com/TrinityCoreClassic/core/releases and download the latest TCC.world.xxxx.sql release and unpack the sql file into **C:\TrinityCoreLassic\sql\base**.  

# Server setup

1. Browse to where you compiled the core **C:\TrinityCoreClassic\build\bin\Release** and simply copy both bnetserver.conf.dist and worldserver.conf.dist into the same folder. Rename the copied files to bnetserver.conf and worldserver.conf.  

2. Open bnetserver.conf with a text editor and locate the line shown below on the picture. 
LINK
Change **trinity;trinity** to the new username you made earlier in HeidiSQL + the password you set.  
Example: **yourusername;yourpassword**  

3. Open worldserver.conf with a text editor and locate the line shown below on the picture.  
LINK 
And do the exact same thing you did to bnetserver.conf.  

4. Start **worldserver.exe**. When starting it for the very first time, it will attempt to build the MySQL databases. You should be greeted with this screen below:  
LINK  
Type **y** every time it asks if you want to create a database and it'll automatically populate the databases, assuming you've done everything correct.  

