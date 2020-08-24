# winmm-midi-wrapper
This project redirects midi data an application outputs into a small separate server from which you can do whatever you want with it.  
Currently, it will just make use of virtualMIDI and loop back the application output into a new midi input port so other applications can process it.  
Note that this as of now does not include the midiStream interface, so if your application uses that for output, it will be silent.  
I may implement that interface in the future, this is more just a small demo of how to write a very small wrapper .dll file.  
To compile it, you need GCC 8.0 or later and need to provide the virtualMIDI SDK yourself to compile the demo server.    

# Usage
To get the demo up and running, make sure you have virtualMIDI installed.  
Then, simply start up the server executable and copy over the wrapper winmm.dll into the application folder you want to redirect.  
Once you then start the application, you should see the server reporting that the client has connected.  
From there you can listen to it from the new midi input port the server created.  
Just as example, using VSTHost you can have a virtual synth play back that midi data.  