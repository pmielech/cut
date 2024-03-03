# Cpu Usage Tracker
The application allows the user to read the current CPU usage. The program utilizes the POSIX libraries available in the C programming language. The program reads data stored under the proc/stat path and calculates current usage based on this method. This application only applies to UNIX-based systems which use this path to inform about the system information fields.

| macOS with debbug setting                                                                                               |Debian based linux distro                  |
|-----------------------------------------------------------------------------------------------------|-------------------------------|       
| ![image](https://github.com/pmielech/cut/assets/95683261/fa9a6b5d-4c6f-49ee-88ca-46d6db22e0e0) |![image](https://github.com/pmielech/cut/assets/95683261/869d506a-301a-49aa-a061-e5a45f8046fe)|

### **WARNING**
`Under the macOS, there is no such path as proc/stat. In the current situation, there is a DEBUG directive implemented to allow program debugging under that specific operational system. When run with DEBUG setting program uses the snapshot of that file located in the test_path directory.`

### Threads
The project utilizes the PThreads library to run subsequently the next steps in the workflow. In addition, added a LOGGER thread to log additional info under the logs/ directory.

PRINTER thread uses the ncurses library to display the collected data in a formatted manner. It refreshes only when the analyzer function completes Its job.

Currently implemented threads:
* LOGGER
* READER
* ANALYZER
* PRINTER
  
TODO:
* WATCHDOG

