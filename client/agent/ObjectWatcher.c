
/**
 * Custom implementation of tracking object creation/deletion
 * @author Jigar Joshi
 * This is based on JDK8's example heapTracker
 */


#include "stdlib.h"
#include "heapTracker.h"
#include "java_crw_demo.h"
#include "jni.h"
#include "jvmti.h"
#include "agent_util.h"

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

// constants
#define HEAP_TRACKER_class                      HeapTracker
#define OBJECT_class                            java/lang/Object

#define HEAP_TRACKER_newobj                     newobj
#define HEAP_TRACKER_newarr                     newarr
#define HEAP_TRACKER_native_newobj              _newobj
#define HEAP_TRACKER_native_newarr              _newarr
#define HEAP_TRACKER_engaged                    engaged
#define MAX_FRAMES                              5

// macros
#define _STRING(s)      #s
#define STRING(s)       _STRING(s)

typedef enum {
    TRACE_FIRST = 0,
    TRACE_USER = 0,
    TRACE_BEFORE_VM_START = 1,
    TRACE_BEFORE_VM_INIT = 2,
    TRACE_VM_OBJECT = 3,
    TRACE_MYSTERY = 4,
    TRACE_LAST = 4
} TraceFlavor;

static char * flavorDesc[] = {
    "U", // User Object
    "BVS", // before VM starts
    "BVI", // before VM initialized
    "V", // VM Object
    "X" //unknown
};

typedef struct Trace {
    jint numberOfFrames;
    jvmtiFrameInfo frames[MAX_FRAMES + 2];
    TraceFlavor flavor;
} Trace;

typedef struct TraceInfo {
    Trace trace;

    jlong allocationTime;
    jlong deallocationTime;

    jlong id;
} TraceInfo;

typedef struct {
    jvmtiEnv *jvmti;

    jboolean vmStarted;
    jboolean vmInitialized;
    jboolean vmDead;

    int maxDump;

    jrawMonitorID lock;

    jlong counter;
    TraceInfo *emptyTrace[TRACE_LAST + 1];
    char *serverHostname;
    int port;
    int socket_desc;
    struct sockaddr_in server;
} GlobalAgentData;

static GlobalAgentData *gdata;

/**
 * Acquires the lock
 * @param jvmti
 */
static void
lock(jvmtiEnv *jvmti) {
    jvmtiError error;
    error = (*jvmti)->RawMonitorEnter(jvmti, gdata->lock);
    check_jvmti_error(jvmti, error, "error getting lock");
}

/**
 * Releases the lock
 * @param jvmti
 */
static void
unlock(jvmtiEnv *jvmti) {
    jvmtiError error;
    error = (*jvmti)->RawMonitorExit(jvmti, gdata->lock);
    check_jvmti_error(jvmti, error, "error unlocking");
}

/**
 * creates socket connection to server
 */
static void initiateSocketConnection() {
    //create socket
    gdata->socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (gdata->socket_desc == -1) {
        printf("could not create socket");
    }
    printf("connecting to %s:%d \n", gdata->serverHostname, gdata->port);
    gdata->server.sin_addr.s_addr = inet_addr(gdata->serverHostname);
    gdata->server.sin_family = AF_INET;
    gdata->server.sin_port = htons(gdata->port);

    //Connect to remote server
    if (connect(gdata->socket_desc, (struct sockaddr *) &gdata->server, sizeof (gdata->server)) < 0) {
        puts("connect error\n");
        return;
    }

    puts("Connected\n");
}

/**
 * Writes message to socket
 * @param message
 */
static void
flushToSocket(char* message) {
    if (send(gdata->socket_desc, message, strlen(message), 0) < 0) {
        puts("Send failed");
        return;
    }
}

/**
 * Returns empty stacktrace
 * @param flavor
 * @return
 */
static TraceInfo *
emptyTrace(TraceFlavor flavor) {
    return gdata->emptyTrace[flavor];
}

/**
 * Converts FrameInfo into String
 * @param jvmti
 * @param buf
 * @param buflen
 * @param finfo
 */
static void
frameToString(jvmtiEnv *jvmti, char *buf, int buflen, jvmtiFrameInfo *finfo) {
    jvmtiError error;
    jclass klass;
    char *signature;
    char *methodname;
    char *methodsig;
    jboolean isNative;
    char *filename;
    int lineCount;
    jvmtiLineNumberEntry*lineTable;
    int lineNumber;

    buf[0] = 0;
    klass = NULL;
    signature = NULL;
    methodname = NULL;
    methodsig = NULL;
    isNative = JNI_FALSE;
    filename = NULL;
    lineCount = 0;
    lineTable = NULL;
    lineNumber = 0;

    error = (*jvmti)->GetMethodDeclaringClass(jvmti, finfo->method, &klass);
    check_jvmti_error(jvmti, error, "cannot get method's class");

    // get the class signature
    error = (*jvmti)->GetClassSignature(jvmti, klass, &signature, NULL);
    check_jvmti_error(jvmti, error, "cannot get class signature");

    // skip for HeapTracker class
    if (strcmp(signature, "L" STRING(HEAP_TRACKER_class) ";") == 0) {
        deallocate(jvmti, signature);
        return;
    }

    // get the name and signature for the method
    error = (*jvmti)->GetMethodName(jvmti, finfo->method,
            &methodname, &methodsig, NULL);
    check_jvmti_error(jvmti, error, "cannot method name");

    // Check to see if it's a native method, which means no lineNumber
    error = (*jvmti)->IsMethodNative(jvmti, finfo->method, &isNative);
    check_jvmti_error(jvmti, error, "Cannot get method native status");

    // get source file name
    error = (*jvmti)->GetSourceFileName(jvmti, klass, &filename);
    if (error != JVMTI_ERROR_NONE && error != JVMTI_ERROR_ABSENT_INFORMATION) {
        check_jvmti_error(jvmti, error, "Cannot get source filename");
    }

    // Get lineNumber if we can
    if (!isNative) {
        int i;

        // Get method line table
        error = (*jvmti)->GetLineNumberTable(jvmti, finfo->method, &lineCount, &lineTable);
        if (error == JVMTI_ERROR_NONE) {
            // Search for line
            lineNumber = lineTable[0].line_number;
            for (i = 1; i < lineCount; i++) {
                if (finfo->location < lineTable[i].start_location) {
                    break;
                }
                lineNumber = lineTable[i].line_number;
            }
        } else if (error != JVMTI_ERROR_ABSENT_INFORMATION) {
            check_jvmti_error(jvmti, error, "Cannot get method line table");
        }
    }

    // TODO: i18n
    (void) sprintf(buf, "%s.%s@%d[%s:%d]",
            (signature == NULL ? "UnknownClass" : signature),
            (methodname == NULL ? "UnknownMethod" : methodname),
            (int) finfo->location,
            (filename == NULL ? "UnknownFile" : filename),
            lineNumber);

    /// Free up JVMTI space allocated by the above calls
    deallocate(jvmti, signature);
    deallocate(jvmti, methodname);
    deallocate(jvmti, methodsig);
    deallocate(jvmti, filename);
    deallocate(jvmti, lineTable);
}

/**
 * Gets current time in millisecond
 * @return
 */
static jlong
getTime() {
    time_t seconds;
    time(&seconds);
    unsigned long long millis = (unsigned long long) seconds * 1000;
    return millis;
}

/**
 * Constructs TraceInfo
 * @param trace
 * @param flavor
 * @return
 */
static TraceInfo *
constructTraceInfo(Trace *trace, TraceFlavor flavor) {
    TraceInfo *tinfo;
    tinfo = (TraceInfo*) malloc(sizeof (TraceInfo));
    if (tinfo == NULL) {
        fatal_error("ERROR: Ran out of malloc() space\n");
    } else {
        gdata->counter = gdata->counter + 1;
        tinfo->trace = *trace;
        tinfo->trace.flavor = flavor;
        tinfo->id = gdata->counter;
        tinfo->allocationTime = getTime();
    }
    return tinfo;
}

/**
 * prints traceinfo
 *
 * @param jvmti
 * @param tinfo
 * @param stringData
 */
static void
printTraceInfo(jvmtiEnv *jvmti, TraceInfo* tinfo, char* stringData) {
    if (tinfo == NULL) {
        fatal_error("%d: NULL ENTRY ERROR\n", index);
        return;
    }

    if (tinfo->trace.numberOfFrames > 0) {
        int i;
        int fcount;

        fcount = 0;
        strcpy(stringData, "\0");
        for (i = 0; i < tinfo->trace.numberOfFrames; i++) {
            char buf[4096];

            frameToString(jvmti, buf, (int) sizeof (buf), tinfo->trace.frames + i);
            if (buf[0] == 0) {
                // skip Tracker's
                continue;
            }
            fcount++;
            strcat(stringData, buf);
            if (i < (tinfo->trace.numberOfFrames - 1)) {
                //stdout_message(",");
                strcat(stringData, ",\0");
            }
        }
        strcat(stringData, "\n\0");
    } else {
        strcpy(stringData, "<empty>\n\0");
    }
}

/**
 * Custom event handler for deallocation of object
 * @param id
 */
static void
eventDeallocatation(jlong id) {
    char *message;
    asprintf(&message, "d_%ld_%ld\n", id, getTime());
    flushToSocket(message);
    deallocate(gdata->jvmti, message);
}

/**
 * Custom event handler for allocation of object
 * @param tinfo
 */
static void
eventAllocation(TraceInfo *tinfo) {
    // limit it to USER flavor for now
    if (!tinfo->trace.flavor == TRACE_USER) {
        return;
    }
    char* stringData = (char*) malloc(4096 * sizeof (char));
    printTraceInfo(gdata->jvmti, tinfo, stringData);
    char *headerPart;
    char *entireMessage;
    char *code = "c";
    asprintf(&headerPart, "%s_%d_%s_%ld", code, tinfo->id, flavorDesc[tinfo->trace.flavor], getTime());
    asprintf(&entireMessage, "%s_%s", headerPart, stringData);
    flushToSocket(entireMessage);
    deallocate(gdata->jvmti, entireMessage);
    deallocate(gdata->jvmti, headerPart);
    deallocate(gdata->jvmti, stringData);
}

/**
 * Process the trace
 * @param jvmti
 * @param trace
 * @param flavor
 * @return
 */
static TraceInfo *
processTrace(jvmtiEnv *jvmti, Trace *trace, TraceFlavor flavor) {
    TraceInfo *tinfo;
    lock(jvmti);
    {
        tinfo = constructTraceInfo(trace, flavor);
        eventAllocation(tinfo);
    }
    unlock(jvmti);
    return tinfo;
}

/**
 * Gets the TraceInfo
 * @param jvmti
 * @param thread
 * @param flavor
 * @return
 */
static TraceInfo *
getTraceInfo(jvmtiEnv *jvmti, jthread thread, TraceFlavor flavor) {
    TraceInfo *tinfo;
    jvmtiError error;

    tinfo = NULL;
    if (thread != NULL) {
        static Trace empty;
        Trace trace;

        // Before VM_INIT thread could be NULL, watch out
        trace = empty;
        error = (*jvmti)->GetStackTrace(jvmti, thread, 0, MAX_FRAMES + 2,
                trace.frames, &(trace.numberOfFrames));
        // If we get a PHASE error, the VM isn't ready, or it died
        if (error == JVMTI_ERROR_WRONG_PHASE) {
            // It is assumed this is before VM_INIT
            if (flavor == TRACE_USER) {
                //tinfo = emptyTrace(TRACE_BEFORE_VM_INIT);
            } else {
                //tinfo = emptyTrace(flavor);
            }
        } else {
            check_jvmti_error(jvmti, error, "Cannot get stack trace");
            tinfo = processTrace(jvmti, &trace, flavor);
        }
    } else {
        // If thread==NULL, it's assumed this is before VM_START
        if (flavor == TRACE_USER) {
            // TODO: take care of these later
            //tinfo = emptyTrace(TRACE_BEFORE_VM_START);
        } else {
            //tinfo = emptyTrace(flavor);
        }
    }
    return tinfo;
}

/**
 * Tags object with heap-inspector's object id
 * @param jvmti
 * @param object
 * @param tinfo
 */
static void
tagObjectWithId(jvmtiEnv *jvmti, jobject object, TraceInfo *tinfo) {
    jvmtiError error;
    jlong tag;
    tag = (jlong) (ptrdiff_t) (void*) tinfo;
    error = (*jvmti)->SetTag(jvmti, object, tinfo->id);
    check_jvmti_error(jvmti, error, "Cannot tag object");
}

/**
 * Java Native Method for Object.<init>
 * @param env
 * @param klass
 * @param thread
 * @param o
 */
static void JNICALL
HEAP_TRACKER_native_newobj(JNIEnv *env, jclass klass, jthread thread, jobject o) {
    TraceInfo *tinfo;

    if (gdata->vmDead) {
        return;
    }
    tinfo = getTraceInfo(gdata->jvmti, thread, TRACE_USER);
    if (tinfo != NULL) {
        tagObjectWithId(gdata->jvmti, o, tinfo);
        (void) free((TraceInfo*) tinfo);
    }
}

/**
 * Java Native Method for newarray
 * @param env
 * @param klass
 * @param thread
 * @param a
 */
static void JNICALL
HEAP_TRACKER_native_newarr(JNIEnv *env, jclass klass, jthread thread, jobject a) {
    TraceInfo *tinfo;

    if (gdata->vmDead) {
        return;
    }
    tinfo = getTraceInfo(gdata->jvmti, thread, TRACE_USER);
    if (tinfo != NULL) {
        tagObjectWithId(gdata->jvmti, a, tinfo);
        (void) free((TraceInfo*) tinfo);
    }
}

/**
 * Callback for JVMTI_EVENT_VM_START
 * @param jvmti
 * @param env
 */
static void JNICALL
onVMStart(jvmtiEnv *jvmti, JNIEnv *env) {
    lock(jvmti);
    {
        jclass klass;
        jfieldID field;
        jint rc;
        gdata->counter = 0;
        // Java Native Methods for class
        static JNINativeMethod registry[2] = {
            {STRING(HEAP_TRACKER_native_newobj), "(Ljava/lang/Object;Ljava/lang/Object;)V",
                (void*) &HEAP_TRACKER_native_newobj},
            {STRING(HEAP_TRACKER_native_newarr), "(Ljava/lang/Object;Ljava/lang/Object;)V",
                (void*) &HEAP_TRACKER_native_newarr}
        };

        // Register Natives for class whose methods we use
        klass = (*env)->FindClass(env, STRING(HEAP_TRACKER_class));
        if (klass == NULL) {
            fatal_error("ERROR: JNI: Cannot find %s with FindClass\n",
                    STRING(HEAP_TRACKER_class));
        }
        rc = (*env)->RegisterNatives(env, klass, registry, 2);
        if (rc != 0) {
            fatal_error("ERROR: JNI: Cannot register natives for class %s\n",
                    STRING(HEAP_TRACKER_class));
        }

        // Engage calls
        field = (*env)->GetStaticFieldID(env, klass, STRING(HEAP_TRACKER_engaged), "I");
        if (field == NULL) {
            fatal_error("ERROR: JNI: Cannot get field from %s\n",
                    STRING(HEAP_TRACKER_class));
        }
        (*env)->SetStaticIntField(env, klass, field, 1);

        /* Indicate VM has started */
        gdata->vmStarted = JNI_TRUE;
    }
    unlock(jvmti);
}

/**
 * Callback for JVMTI_EVENT_VM_INIT
 * @param jvmti
 * @param env
 * @param thread
 */
static void JNICALL
onVMInit(jvmtiEnv *jvmti, JNIEnv *env, jthread thread) {
    lock(jvmti);
    {
        // Indicate VM is initialized
        gdata->vmInitialized = JNI_TRUE;
    }
    unlock(jvmti);
}

/**
 * Callback for JVMTI_EVENT_VM_DEATH
 * @param jvmti
 * @param env
 */
static void JNICALL
onVMDeath(jvmtiEnv *jvmti, JNIEnv *env) {
    jvmtiError error;

    // Force garbage collection now so we get our ObjectFree calls
    error = (*jvmti)->ForceGarbageCollection(jvmti);
    check_jvmti_error(jvmti, error, "cannot force garbage collection");


    // Process VM Death
    lock(jvmti);
    {
        jclass klass;
        jfieldID field;

        // Disengage calls in HEAP_TRACKER_class
        klass = (*env)->FindClass(env, STRING(HEAP_TRACKER_class));
        if (klass == NULL) {
            fatal_error("ERROR: JNI: Cannot find %s with FindClass\n",
                    STRING(HEAP_TRACKER_class));
        }
        field = (*env)->GetStaticFieldID(env, klass, STRING(HEAP_TRACKER_engaged), "I");
        if (field == NULL) {
            fatal_error("ERROR: JNI: Cannot get field from %s\n",
                    STRING(HEAP_TRACKER_class));
        }
        (*env)->SetStaticIntField(env, klass, field, 0);
        gdata->vmDead = JNI_TRUE;
    }
    unlock(jvmti);
}

/**
 * Callback for JVMTI_EVENT_VM_OBJECT_ALLOC
 * @param jvmti
 * @param env
 * @param thread
 * @param object
 * @param object_klass
 * @param size
 */
static void JNICALL
onVMObjectAlloc(jvmtiEnv *jvmti, JNIEnv *env, jthread thread,
        jobject object, jclass object_klass, jlong size) {
    TraceInfo *tinfo;
    // don't care if VM is already dead
    if (gdata->vmDead) {
        return;
    }
    tinfo = getTraceInfo(jvmti, thread, TRACE_VM_OBJECT);
    tagObjectWithId(jvmti, object, tinfo);
}

/**
 * Callback for JVMTI_EVENT_OBJECT_FREE
 * @param jvmti
 * @param tag
 */
static void JNICALL
onObjectFree(jvmtiEnv *jvmti, jlong tag) {
    if (gdata->vmDead) {
        return;
    }
    eventDeallocatation(tag);
}

/**
 * Callback for JVMTI_EVENT_CLASS_FILE_LOAD_HOOK
 * @param jvmti
 * @param env
 * @param class_being_redefined
 * @param loader
 * @param name
 * @param protection_domain
 * @param class_data_len
 * @param class_data
 * @param new_class_data_len
 * @param new_class_data
 */
static void JNICALL
onClassFileLoadHook(jvmtiEnv *jvmti, JNIEnv* env,
        jclass class_being_redefined, jobject loader,
        const char* name, jobject protection_domain,
        jint class_data_len, const unsigned char* class_data,
        jint* new_class_data_len, unsigned char** new_class_data) {
    lock(jvmti);
    {
        // It's possible we get here right after VmDeath event, be careful
        // if vm is dead, don't care as we only want to hook into bootstrap classes
        if (!gdata->vmDead) {
            const char * classname;
            // name can be NULL
            if (name == NULL) {
                classname = java_crw_demo_classname(class_data, class_data_len,
                        NULL);
                if (classname == NULL) {
                    fatal_error("ERROR: No classname in classfile\n");
                }
            } else {
                classname = strdup(name);
                if (classname == NULL) {
                    fatal_error("ERROR: Ran out of malloc() space\n");
                }
            }

            *new_class_data_len = 0;
            *new_class_data = NULL;

            // The tracker class itself? --> ignore
            if (strcmp(classname, STRING(OBJECT_class)) == 0) {
                jint cnum;
                int systemClass;
                unsigned char *newImage;
                long newLength;
                printf("[agent] instrumented %s\n", classname);

                /*  Is it a system class? If the class load is before VmStart
                 *  then we will consider it a system class that should
                 *  be treated carefully. (See java_crw_demo)
                 */
                systemClass = 0;
                if (!gdata->vmStarted) {
                    systemClass = 1;
                }

                newImage = NULL;
                newLength = 0;

                // instrumentation
                java_crw_demo(cnum,
                        classname,
                        class_data,
                        class_data_len,
                        systemClass,
                        STRING(HEAP_TRACKER_class),
                        "L" STRING(HEAP_TRACKER_class) ";",
                        NULL, NULL,
                        NULL, NULL,
                        STRING(HEAP_TRACKER_newobj), "(Ljava/lang/Object;)V",
                        STRING(HEAP_TRACKER_newarr), "(Ljava/lang/Object;)V",
                        &newImage,
                        &newLength,
                        NULL,
                        NULL);

                // If we got back a new class image, return it back as "the"
                // new class image. This must be JVMTI Allocate space.

                if (newLength > 0) {
                    unsigned char *jvmti_space;

                    jvmti_space = (unsigned char *) allocate(jvmti, (jint) newLength);
                    (void) memcpy((void*) jvmti_space, (void*) newImage, (int) newLength);
                    *new_class_data_len = (jint) newLength;
                    *new_class_data = jvmti_space; /* VM will deallocate */
                }

                // Always free up the space we get from java_crw_demo()
                if (newImage != NULL) {
                    (void) free((void*) newImage);
                }
            }
            (void) free((void*) classname);
        }
    }
    unlock(jvmti);
}

/**
 * Options parsing
 * @param options
 */
static void
parse_agent_options(char *options) {
#define MAX_TOKEN_LENGTH        50
    char token[MAX_TOKEN_LENGTH];
    char *next;
    gdata->maxDump = 20;

    printf("\n\nOptions = %s\n\n", options);
    if (options == NULL) {
        return;
    }
    // Get the first token from the options string
    next = get_token(options, ",=", token, (int) sizeof (token));

    // While not at the end of the options string, process this option
    while (next != NULL) {
        printf("\n\ntoken=%s\n\n", token);
        if (strcmp(token, "help") == 0) {
            stdout_message("The heapTracker JVMTI demo agent\n");
            stdout_message("\n");
            stdout_message(" java -agent:heapTracker[=options] ...\n");
            stdout_message("\n");
            stdout_message("The options are comma separated:\n");
            stdout_message("\t help\t\t\t Print help information\n");
            stdout_message("\t maxDump=n\t\t\t How many TraceInfo's to dump\n");
            stdout_message("\t server=n\t\t\t server hostname/IP\n");
            stdout_message("\t port=n\t\t\t server's port\n");
            stdout_message("\n");
            exit(0);
        } else if (strcmp(token, "maxDump") == 0) {
            char number[MAX_TOKEN_LENGTH];
            next = get_token(next, ",=", number, (int) sizeof (number));
            if (next == NULL) {
                fatal_error("ERROR: Cannot parse maxDump=number: %s\n", options);
            }
            printf("%s", number);
            gdata->maxDump = atoi(number);
        } else if (strcmp(token, "server") == 0) {
            char server[MAX_TOKEN_LENGTH];
            next = get_token(next, ",=", server, (int) sizeof (server));
            if (next == NULL) {
                fatal_error("ERROR: Cannot parse maxDump=number: %s\n", options);
            }
            printf("%s", server);
            asprintf(&gdata->serverHostname, "%s", server);

        } else if (strcmp(token, "port") == 0) {
            char port[MAX_TOKEN_LENGTH];
            next = get_token(next, ",=", port, (int) sizeof (port));
            if (next == NULL) {
                fatal_error("ERROR: Cannot parse maxDump=number: %s\n", options);
            }
            printf("%s", port);

            gdata->port = atoi(port);
        } else if (token[0] != 0) {
            // unknown option supplied
            fatal_error("ERROR: Unknown option: %s\n", token);
        }
        /* Get the next token (returns NULL if there are no more) */
        next = get_token(next, ",=", token, (int) sizeof (token));
    }
}

/**
 * Agent on load event first code to get executed
 * @param vm
 * @param options
 * @param reserved
 * @return
 */
JNIEXPORT jint JNICALL
Agent_OnLoad(JavaVM *vm, char *options, void *reserved) {
    static GlobalAgentData data;
    jvmtiEnv *jvmti;
    jvmtiError error;
    jint res;
    TraceFlavor flavor;
    jvmtiCapabilities capabilities;
    jvmtiEventCallbacks callbacks;
    static Trace empty;
    printf("\n\nagent loaded \n\n");
    // allocation for global data
    (void) memset((void*) &data, 0, sizeof (data));
    gdata = &data;
    gdata->serverHostname = "127.0.0.1";
    gdata->port = 9000;
    // First thing we need to do is get the jvmtiEnv* or JVMTI environment
    res = (*vm)->GetEnv(vm, (void **) &jvmti, JVMTI_VERSION_1);
    if (res != JNI_OK) {
        // VM's and JVMTI's version incompatibility
        fatal_error("ERROR: Unable to access JVMTI Version 1 (0x%x),"
                " is your JDK a 5.0 or newer version?"
                " JNIEnv's GetEnv() returned %d\n",
                JVMTI_VERSION_1, res);
    }

    // save it into global scope
    gdata->jvmti = jvmti;
    // options parsing
    parse_agent_options(options);

    // ask VMs for the capabilities
    (void) memset(&capabilities, 0, sizeof (capabilities));
    capabilities.can_generate_all_class_hook_events = 1;
    capabilities.can_tag_objects = 1;
    capabilities.can_generate_object_free_events = 1;
    capabilities.can_get_source_file_name = 1;
    capabilities.can_get_line_numbers = 1;
    capabilities.can_generate_vm_object_alloc_events = 1;
    error = (*jvmti)->AddCapabilities(jvmti, &capabilities);
    // did we really got capabilities from VM
    check_jvmti_error(jvmti, error, "Unable to get necessary JVMTI capabilities.");

    // registers pointers to callback functions
    (void) memset(&callbacks, 0, sizeof (callbacks));
    // JVMTI_EVENT_VM_START
    callbacks.VMStart = &onVMStart;
    // JVMTI_EVENT_VM_INIT
    callbacks.VMInit = &onVMInit;
    // JVMTI_EVENT_VM_DEATH
    callbacks.VMDeath = &onVMDeath;
    // JVMTI_EVENT_OBJECT_FREE
    callbacks.ObjectFree = &onObjectFree;
    // JVMTI_EVENT_VM_OBJECT_ALLOC
    callbacks.VMObjectAlloc = &onVMObjectAlloc;
    // JVMTI_EVENT_CLASS_FILE_LOAD_HOOK
    callbacks.ClassFileLoadHook = &onClassFileLoadHook;
    error = (*jvmti)->SetEventCallbacks(jvmti, &callbacks, (jint)sizeof (callbacks));
    check_jvmti_error(jvmti, error, "Cannot set jvmti callbacks");

    // At first the only initial events we are interested in are VM
    // initialization, VM death, and Class File Loads.
    // Once the VM is initialized we will request more events.

    error = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE,
            JVMTI_EVENT_VM_START, (jthread) NULL);
    check_jvmti_error(jvmti, error, "Cannot set event notification");
    error = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE,
            JVMTI_EVENT_VM_INIT, (jthread) NULL);
    check_jvmti_error(jvmti, error, "Cannot set event notification");
    error = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE,
            JVMTI_EVENT_VM_DEATH, (jthread) NULL);
    check_jvmti_error(jvmti, error, "Cannot set event notification");
    error = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE,
            JVMTI_EVENT_OBJECT_FREE, (jthread) NULL);
    check_jvmti_error(jvmti, error, "Cannot set event notification");
    error = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE,
            JVMTI_EVENT_VM_OBJECT_ALLOC, (jthread) NULL);
    check_jvmti_error(jvmti, error, "Cannot set event notification");
    error = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE,
            JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, (jthread) NULL);
    check_jvmti_error(jvmti, error, "Cannot set event notification");

    // create monitor
    error = (*jvmti)->CreateRawMonitor(jvmti, "agent data", &(gdata->lock));
    check_jvmti_error(jvmti, error, "Cannot create raw monitor");

    // create the TraceInfo for various flavors of empty traces
    for (flavor = TRACE_FIRST; flavor <= TRACE_LAST; flavor++) {
        gdata->emptyTrace[flavor] =
                constructTraceInfo(&empty, flavor);
    }
    // setup socket connection to our server
    initiateSocketConnection();
    // say all is well
    return JNI_OK;
}

/**
 * Agent is unloading
 * @param vm
 */
JNIEXPORT void JNICALL
Agent_OnUnload(JavaVM *vm) {
    // VM is about to die anyway
}
