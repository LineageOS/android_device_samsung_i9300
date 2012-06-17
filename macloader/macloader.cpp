/*
 * Copyright (C) 2012, The CyanogenMod Project
 *                     Daniel Hillenbrand <codeworkx@cyanogenmod.com>
 *                     Marco Hillenbrand <marco.hillenbrand@googlemail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <cutils/log.h>

#define LOG_TAG "macloader"
#define LOG_NDEBUG 0

#define MACADDR_PATH "/efs/wifi/.mac.info"
#define CID_PATH "/data/.cid.info"

/*
 * murata:
 * 00:37:6d
 * 88:30:8a
 * 
 * semcove:
 * 
 */

int main() {
    FILE* file;
    FILE* cidfile;
    char* str;
    char mac_addr_half[9];
    int ret = -1;
    int amode;
    
    /* open mac addr file */
    file = fopen(MACADDR_PATH, "r");
    if(file == 0) {
        fprintf(stderr, "open(%s) failed\n", MACADDR_PATH);
        LOGE("Can't open %s\n", MACADDR_PATH);
        return -1;
    }

    /* get and compare mac addr */
    str = fgets(mac_addr_half, 9, file);    
    if(str == 0) {
        fprintf(stderr, "fgets() from file %s failed\n", MACADDR_PATH);
        LOGE("Can't read from %s\n", MACADDR_PATH);
        return -1;
    }
    
    /* murata */
    if(strncmp(mac_addr_half, "00:37:6d", 9) == 0 || strncmp(mac_addr_half, "88:30:8a", 9) == 0) {

        /* open cid file */
        cidfile = fopen(CID_PATH, "w");
        if(cidfile == 0) {
            fprintf(stderr, "open(%s) failed\n", CID_PATH);
            LOGE("Can't open %s\n", CID_PATH);
            return -1;
        }

        /* write murata to cid file */
        LOGD("Writing murata to %s\n", CID_PATH);
        ret = fputs("murata", cidfile);
        if(ret != 0) {
            fprintf(stderr, "fputs() to file %s failed\n", CID_PATH);
            LOGE("Can't write to %s\n", CID_PATH);
            return -1;
        }
        fclose(cidfile);
        
        /* set permissions on cid file */
        LOGD("Setting permissions on %s\n", CID_PATH);
        amode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
        ret = chmod(CID_PATH, amode);

        char* chown_cmd = (char*) malloc(strlen("chown system ") + strlen(CID_PATH));
        char* chgrp_cmd = (char*) malloc(strlen("chgrp system ") + strlen(CID_PATH));
        sprintf(chown_cmd, "chown system %s", CID_PATH);
        sprintf(chgrp_cmd, "chgrp system %s", CID_PATH);
        system(chown_cmd);
        system(chgrp_cmd);

        if(ret != 0) {
            fprintf(stderr, "chmod() on file %s failed\n", CID_PATH);
            LOGE("Can't set permissions on %s\n", CID_PATH);
            return ret;
        }

    } else {
        /* delete cid file if not murata or semcove */
        LOGD("Deleting file %s\n", CID_PATH);
        remove(CID_PATH);
    }
    fclose(file);
    return 0;
}
