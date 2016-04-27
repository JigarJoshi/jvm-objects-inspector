# JVM Inspector

This is an *experimental* project, it reports realtime user object creation in JVM and indexes it into Elasticsearch, Purpose of the project is to identify memory leak and study object life cycle for any app

Note: 

 - It is based on [heapTracker](http://hg.openjdk.java.net/jdk8/jdk8/jdk/file/687fd7c7986d/src/share/demo/jvmti/heapTracker/heapTracker.c) example from JDK 
 - It is NOT advisable to use it in production


## Setup

**Client**

 - build the native agent
 - compile boot class
 - start JVM with agentlib and add extra class to bootclasspath 
 
**Server**

 - configure & start the server