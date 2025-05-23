/*
 * Copyright (c) 2003, 2024, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include <stdio.h>
#include <string.h>
#include "jvmti.h"
#include "agent_common.h"

extern "C" {


#define JVMTI_ERROR_CHECK(str,res) if (res != JVMTI_ERROR_NONE) { printf(str); printf("%d\n",res); return res; }
#define JVMTI_ERROR_CHECK_VOID(str,res) if (res != JVMTI_ERROR_NONE) { printf(str); printf("%d\n",res); iGlobalStatus = 2; }

#define THREADS_LIMIT 8

jrawMonitorID access_lock;
jvmtiEnv *jvmti;
jint iGlobalStatus = 0;
jthread susp_thrd[THREADS_LIMIT];
static jvmtiEventCallbacks callbacks;
static jvmtiCapabilities jvmti_caps;


int printdump = 0;
int gc_start_count =0;
int gc_finish_count =0;


void debug_printf(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    if (printdump) {
        vprintf(fmt, args);
    }
    va_end(args);
}


void JNICALL vmInit(jvmtiEnv *jvmti_env, JNIEnv *env, jthread thread) {

    debug_printf("VMInit event  done\n");
}

void JNICALL vmExit(jvmtiEnv *jvmti_env, JNIEnv *env) {
    debug_printf("------------ JVMTI_EVENT_VM_DEATH ------------\n");
    debug_printf("JVMTI_EVENT_GARBAGE_COLLECTION_START count: %d\n",gc_start_count);
    debug_printf("JVMTI_EVENT_GARBAGE_COLLECTION_FINISH count: %d\n",gc_finish_count);
}

void JNICALL gc_start(jvmtiEnv *jvmti_env) {

    gc_start_count++;
    debug_printf("Event: JVMTI_EVENT_GC_START\n");
}

void JNICALL gc_finish(jvmtiEnv *jvmti_env) {

    gc_finish_count++;
    debug_printf("Event: JVMTI_EVENT_GC_FINISH\n");
}


void init_callbacks() {
    memset((void *)&callbacks, 0, sizeof(jvmtiEventCallbacks));
    callbacks.VMInit = vmInit;
    callbacks.VMDeath = vmExit;
    callbacks.GarbageCollectionStart = gc_start;
    callbacks.GarbageCollectionFinish = gc_finish;
}


#ifdef STATIC_BUILD
JNIEXPORT jint JNICALL Agent_OnLoad_gc(JavaVM *jvm, char *options, void *reserved) {
    return Agent_Initialize(jvm, options, reserved);
}
JNIEXPORT jint JNICALL Agent_OnAttach_gc(JavaVM *jvm, char *options, void *reserved) {
    return Agent_Initialize(jvm, options, reserved);
}
JNIEXPORT jint JNI_OnLoad_gc(JavaVM *jvm, char *options, void *reserved) {
    return JNI_VERSION_1_8;
}
#endif
jint Agent_Initialize(JavaVM * jvm, char *options, void *reserved) {
    jint res;

    if (options && strlen(options) > 0) {
        if (strstr(options, "printdump")) {
            printdump = 1;
        }
    }

    res = jvm->GetEnv((void **) &jvmti, JVMTI_VERSION_1_1);
    if (res < 0) {
        debug_printf("Wrong result of a valid call to GetEnv!\n");
        return JNI_ERR;
    }

    /* Create data access lock */
    res = jvmti->CreateRawMonitor("_access_lock", &access_lock);
    JVMTI_ERROR_CHECK("RawMonitorEnter in monitor_contended_entered failed with error code ", res);


    /* Add capabilities */
    res = jvmti->GetPotentialCapabilities(&jvmti_caps);
    JVMTI_ERROR_CHECK("SetEventCallbacks returned error", res);

    res = jvmti->AddCapabilities(&jvmti_caps);
    JVMTI_ERROR_CHECK("SetEventCallbacks returned error", res);

    /* Enable events */
    init_callbacks();
    res = jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));
    JVMTI_ERROR_CHECK("SetEventCallbacks returned error", res);

    res = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, nullptr);
    JVMTI_ERROR_CHECK("SetEventNotificationMode for VM_INIT returned error", res);

    res = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_DEATH, nullptr);
    JVMTI_ERROR_CHECK("SetEventNotificationMode for vm death event returned error", res);

    res = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_GARBAGE_COLLECTION_START, nullptr);
    JVMTI_ERROR_CHECK("SetEventNotificationMode for gc start returned error", res);

    res = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_GARBAGE_COLLECTION_FINISH, nullptr);
    JVMTI_ERROR_CHECK("SetEventNotificationMode for gc finish returned error", res);

    return JNI_OK;
}




JNIEXPORT void JNICALL
Java_nsk_jvmti_unit_functions_ForceGarbageCollection_gc_checkGCStart(JNIEnv * env, jclass cls) {
    if (gc_start_count == 0) {
        printf("Error: JVMTI_EVENT_GARBAGE_COLLECTION_START count = 0\n");
        iGlobalStatus = 2;
    }
}

JNIEXPORT void JNICALL
Java_nsk_jvmti_unit_functions_ForceGarbageCollection_gc_checkGCFinish(JNIEnv * env, jclass cls) {
    if (gc_finish_count == 0) {
        printf("Error: JVMTI_EVENT_GARBAGE_COLLECTION_FINISH count = 0\n");
        iGlobalStatus = 2;
    }
}

JNIEXPORT jint JNICALL
Java_nsk_jvmti_unit_functions_ForceGarbageCollection_gc_GetResult(JNIEnv * env, jclass cls) {
    return iGlobalStatus;
}


JNIEXPORT void JNICALL
Java_nsk_jvmti_unit_functions_ForceGarbageCollection_gc_jvmtiForceGC (JNIEnv * env, jclass cls) {
    jvmtiError ret;

    debug_printf("jvmti Force gc requested \n");
    ret = jvmti->ForceGarbageCollection();

    if (ret != JVMTI_ERROR_NONE) {
        printf("Error: ForceGarbageCollection %d \n", ret);
        iGlobalStatus = 2;
    }
}

}
