//
//  main.c
//  Packages
//
//  Created by Theo Weidmann on 31.03.15.
//  Copyright (c) 2015 Theo Weidmann. All rights reserved.
//

#define _XOPEN_SOURCE 500
#include "EmojicodeAPI.h"
#include "EmojicodeString.h"
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ftw.h>

PackageVersion getVersion(){
    return (PackageVersion){0, 1};
}

#define handleNEP(failureCondition)\
if((failureCondition))\
    return somethingObject(newError(strerror(errno), errno));\


//MARK: files
Something filesMkdir(Thread *thread){
    int state = mkdir(stringToChar(stackGetVariable(0, thread).object->value), 0755);
    
    handleNEP(state != 0);
    return NOTHINGNESS;
}

Something filesSymlink(Thread *thread){
    char *s = stringToChar(stackGetVariable(0, thread).object->value);
    int state = symlink(s, stringToChar(stackGetVariable(1, thread).object->value));
    free(s);
    
    handleNEP(state != 0);
    return NOTHINGNESS;
}

Something filesFileExists(Thread *thread){
    char *s = stringToChar(stackGetVariable(0, thread).object->value);
    Something x = (access(s, F_OK) == 0) ? EMOJICODE_TRUE : EMOJICODE_FALSE;
    free(s);
    return x;
}

Something filesIsReadable(Thread *thread){
    char *s = stringToChar(stackGetVariable(0, thread).object->value);
    Something x = (access(s, R_OK) == 0) ? EMOJICODE_TRUE : EMOJICODE_FALSE;
    free(s);
    return x;
}

Something filesIsWriteable(Thread *thread){
    char *s = stringToChar(stackGetVariable(0, thread).object->value);
    Something x = (access(s, W_OK) == 0)  ? EMOJICODE_TRUE : EMOJICODE_FALSE;
    free(s);
    return x;
}

Something filesIsExecuteable(Thread *thread){
    char *s = stringToChar(stackGetVariable(0, thread).object->value);
    Something x = (access(s, X_OK) == 0)  ? EMOJICODE_TRUE : EMOJICODE_FALSE;
    free(s);
    return x;
}

Something filesRemove(Thread *thread){
    char *s = stringToChar(stackGetVariable(0, thread).object->value);
    int state = remove(s);
    free(s);
    
    handleNEP(state != 0);
    return NOTHINGNESS;
}

Something filesRmdir(Thread *thread){
    char *s = stringToChar(stackGetVariable(0, thread).object->value);
    int state = rmdir(s);
    free(s);
    
    handleNEP(state != 0);
    return NOTHINGNESS;
}

int filesRecursiveRmdirHelper(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf){
    int state;
    
    if(typeflag == FTW_F || typeflag == FTW_SL || typeflag == FTW_SLN){
        state = remove(fpath);
    }
    else if(typeflag == FTW_DP){
        state = rmdir(fpath);
    }
    else {
        state = -4;
    }
    
    return state;
}

Something filesRecursiveRmdir(Thread *thread){
    char *s = stringToChar(stackGetVariable(0, thread).object->value);
    
    int state = nftw(s, filesRecursiveRmdirHelper, 64, FTW_DEPTH | FTW_PHYS);
    handleNEP(state != 0);
    
    state = rmdir(s);
    handleNEP(state != 0);
    
    free(s);
    return NOTHINGNESS;
}

Something filesSize(Thread *thread){
    char *s = stringToChar(stackGetVariable(0, thread).object->value);
    
    FILE *file = fopen(s, "r");
    free(s);
    
    if(file == NULL){
        return somethingInteger(-1);
    }
    
    fseek(file, 0L, SEEK_END);
    
    long length = ftell(file);
    
    fclose(file);
    
    return somethingInteger((EmojicodeInteger)length);
}


//MARK: file

//Shortcuts

Something fileStringPut(Thread *thread){
    char *s = stringToChar(stackGetVariable(0, thread).object->value);
    FILE *file = fopen(s, "w");
    free(s);
    
    handleNEP(file == NULL);
    
    s = stringToChar(stackGetVariable(1, thread).object->value);
    fwrite(s, 1, strlen(s), file);
    free(s);
    
    handleNEP(ferror(file));
    
    fclose(file);
    return NOTHINGNESS;
}

Something fileDataPut(Thread *thread){
    char *s = stringToChar(stackGetVariable(0, thread).object->value);
    FILE *file = fopen(s, "wb");
    free(s);
    
    handleNEP(file == NULL);
    
    Data *d = stackGetVariable(1, thread).object->value;
    
    fwrite(d->bytes, 1, d->length, file);
    
    free(s);
    
    handleNEP(ferror(file));
    
    fclose(file);
    return NOTHINGNESS;
}

Something fileStringGet(Thread *thread){
    char *s = stringToChar(stackGetVariable(0, thread).object->value);
    FILE *file = fopen(s, "r");
    free(s);
    
    if(file == NULL){
        return NOTHINGNESS;
    }
    
    fseek(file, 0, SEEK_END);
    
    long length = ftell(file);
    
    fseek(file, 0, SEEK_SET);
    
    char *stringBuffer = malloc(length + 1);
    if (stringBuffer){
        fread(stringBuffer, 1, length, file);
        if(ferror(file)){
            fclose(file);
            return NOTHINGNESS;
        }
    }
    else {
        fclose(file);
        return NOTHINGNESS;
    }
    fclose(file);
    stringBuffer[length] = 0;
        
    Object *string = stringFromChar(stringBuffer);
    free(stringBuffer);
    
    return somethingObject(string);
}

Something fileDataGet(Thread *thread){
    char *s = stringToChar(stackGetVariable(0, thread).object->value);
    FILE *file = fopen(s, "r");
    free(s);
    
    if(file == NULL){
        return NOTHINGNESS;
    }
    
    int state = fseek(file, 0, SEEK_END);
    
    long length = ftell(file);
    
    state = fseek(file, 0, SEEK_SET);
    
    char *stringBuffer = malloc(length);
    if (stringBuffer){
        fread(stringBuffer, 1, length, file);
        if(ferror(file)){
            fclose(file);
            return NOTHINGNESS;
        }
    }
    else {
        fclose(file);
        return NOTHINGNESS;
    }
    fclose(file);
    
    Object *obj = newObject(CL_DATA);
    Data *data = obj->value;
    data->bytes = stringBuffer;
    data->length = length;
    
    return somethingObject(obj);
}

#define file(obj) (*((FILE**)(obj)->value))

Something fileStdinGet(Thread *thread){
    Object *obj = newObject(stackGetThisClass(thread));
    file(obj) = stdin;
    return somethingObject(obj);
}

Something fileStdoutGet(Thread *thread){
    Object *obj = newObject(stackGetThisClass(thread));
    file(obj) = stdout;
    return somethingObject(obj);
}

Something fileStderrGet(Thread *thread){
    Object *obj = newObject(stackGetThisClass(thread));
    file(obj) = stderr;
    return somethingObject(obj);
}

//Constructors

void fileForWriting(Thread *thread){
    char *p = stringToChar(stackGetVariable(0, thread).object->value);
    FILE *f = fopen(p, "wb");
    if (f){
        file(stackGetThis(thread)) = f;
    }
    else {
        stackGetThis(thread)->value = NULL;
    }
    free(p);
}

void fileForReading(Thread *thread){
    char *p = stringToChar(stackGetVariable(0, thread).object->value);
    FILE *f = fopen(p, "rb");
    if (f){
        file(stackGetThis(thread)) = f;
    }
    else {
        stackGetThis(thread)->value = NULL;
    }
    free(p);
}

Something fileWriteData(Thread *thread){
    FILE *f = file(stackGetThis(thread));
    Data *d = stackGetVariable(0, thread).object->value;
    
    fwrite(d->bytes, 1, d->length, f);
    fflush(f);
    
    handleNEP(ferror(f));
    return NOTHINGNESS;
}

Something fileReadData(Thread *thread){
    FILE *f = file(stackGetThis(thread));
    EmojicodeInteger n = unwrapInteger(stackGetVariable(0, thread));
    
    char *bytes = malloc(n);
    
    size_t read = fread(bytes, 1, n, f);
    
    if(read == 0 || ferror(f)){
        return NOTHINGNESS;
    }
    
    Object *obj = newObject(CL_DATA);
    Data *data = obj->value;
    data->bytes = bytes;
    data->length = n;
    
    return somethingObject(obj);
}

Something fileSeekTo(Thread *thread){
    fseek(file(stackGetThis(thread)), unwrapInteger(stackGetVariable(0, thread)), SEEK_SET);
    return NOTHINGNESS;
}

Something fileSeekToEnd(Thread *thread){
    fseek(file(stackGetThis(thread)), 0, SEEK_END);
    return NOTHINGNESS;
}

Something fileReadLine(Thread *thread){
    return NOTHINGNESS;
}

void closeFile(void *file){
    fclose((*((FILE**)file)));
}

ClassMethodHandler handlerPointerForClassMethod(EmojicodeChar cl, EmojicodeChar symbol){
    if(cl == 0x1F4D1){
        switch (symbol) {
            case 0x1F4C1:
                return filesMkdir;
            case 0x1F517:
                return filesSymlink;
            case 0x1F4C3:
                return filesFileExists;
            case 0x1F4DC:
                return filesIsReadable;
            case 0x1F4DD:
                return filesIsWriteable;
            case 0x1F45F:
                return filesIsExecuteable;
            case 0x1F52B:
                return filesRemove;
            case 0x1F525:
                return filesRmdir;
            case 0x1F4A3:
                return filesRecursiveRmdir;
            case 0x1F4CF:
                return filesSize;
        }
    }
    else { //0x1F4C4
        switch (symbol) {
            case 0x1F4BE:
                return fileStringPut;
            case 0x1F4D6:
                return fileStringGet;
            case 0x1F4FB:
                return fileDataPut;
            case 0x1F4C7:
                return fileDataGet;
            case 0x1F4E5:
                return fileStdinGet;
            case 0x1F4E4:
                return fileStdoutGet;
            case 0x1F4EF:
                return fileStderrGet;
        }
    }
    return NULL;
}

MethodHandler handlerPointerForMethod(EmojicodeChar cl, EmojicodeChar symbol){
    switch (symbol) {
        case 0x270F:
            return fileWriteData;
        case 0x1F4D3:
            return fileReadData;
        case 0x1F51B:
            return fileSeekTo;
        case 0x1F51A:
            return fileSeekToEnd;
        case 0x1F5E1:
            return fileReadLine;
    }
    return NULL;
}

InitializerHandler handlerPointerForInitializer(EmojicodeChar cl, EmojicodeChar symbol){
    switch (symbol) {
        case 0x1F4DD:
            return fileForWriting;
        case 0x1F4DC:
            return fileForReading;
    }
    return NULL;
}

Marker markerPointerForClass(EmojicodeChar cl){
    return NULL;
}

uint_fast32_t sizeForClass(Class *cl, EmojicodeChar name) {
    switch (name) {
        case 0x1F4C4:
            return sizeof(FILE*);
    }
    return 0;
}

Deinitializer deinitializerPointerForClass(EmojicodeChar cl){
    if(cl == 0x1F4C4){
        return closeFile;
    }
    return NULL;
}
