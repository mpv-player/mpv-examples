This example uses JNA to access the functions in libmpv, and places the
video on a JavaFX Stage.  It also uses the user32 library (Windows) to get 
the  Window ID of the Stage in order to direct the video to this window.  
This is platform specific, but similar functions exist on other platforms.

To run this example, import as a Maven project.  Create a folder "Lib" and 
place the "mpv-1.dll" in it.

It is also possible to put the dll in the "src/main/resources/win32-x86-64/lib" 
folder.  This allows you to package multiple libraries into a target JAR and
have JNA load the correct platform specific library automatically. 
