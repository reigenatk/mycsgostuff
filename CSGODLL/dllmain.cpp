// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "csgo.h"
#include "C_CSPlayer.h"
#include "NetVar.h"
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <iostream>


HMODULE hmod;
HMODULE hClient, hEngine;
C_CSPlayer* pLocalPlayer; // pointer to local player
cBaseEntityList* pEntityList; // pointer to entity list

bool display_stats = false;

// this global variable houses the address of the real CreateMove function from csgo
CreateMoveFn real_CreateMove;
CEngineTrace* game_cEngineTrace;

void pause()
{
    while (1)
    {
        if (GetAsyncKeyState(VK_F2)) {
            // toggle displaying stats
            display_stats = true;
        }
        if (GetAsyncKeyState(VK_F3))
        {
            break;
        }
        Sleep(100);
    }
}

void* Find_Pattern(BYTE* BaseAddress, uintptr_t SearchLen, BYTE* pattern, BYTE* mask) {
    BYTE* end = BaseAddress + SearchLen;
    int pattern_length = strlen((char*) pattern);
    for (BYTE* p = BaseAddress; p < end; p++) {
        for (int i = 0; i < pattern_length; i++) {
            // so only way this fails is if its just not equal and the mask isn't a '?'
            if (mask[i] == '?' || (pattern[i] == p[i])) {
                continue;
            }
            else {
                goto bad;
            }
        }
        // found a pattern
        return p;
    bad:
        continue;
    }
}

// we add the first argument ray here, originally only has start + end but its in a class so
void Ray_Init(Ray_t* ray, Vector const& start, Vector const& end)
{
    // Assert(&end);
    // VectorSubtract(end, start, m_Delta);
    ray->m_Delta.x = end.x - start.x;
    ray->m_Delta.y = end.y - start.y;
    ray->m_Delta.z = end.z - start.z;
    ray->m_Delta.pad = 0;

    // m_IsSwept = (m_Delta.LengthSqr() != 0);
    ray->m_IsSwept = true; // set this to true for now

    //VectorClear(m_Extents);
    memset(&ray->m_Extents, 0, sizeof(VectorAligned));


    ray->m_pWorldAxisTransform = NULL;
    ray->m_IsRay = true;

    // Offset m_Start to be in the center of the box...
    // VectorClear(m_StartOffset);
    memset(&ray->m_StartOffset, 0, sizeof(VectorAligned));
    // VectorCopy(start, m_Start);
    memcpy(&ray->m_Start, &start, sizeof(Vector));
    ray->m_Start.pad = 0;
}



// from mathlib/mathlib_base.cpp
// change & to Vector* in function arguments
void AngleVectors(const Vector* angles, Vector* forward)
{

    float	sp, sy, cp, cy; // p = pitch, y = yaw

    // extract the components
    sy = sin(DEG2RAD(angles->y));
    cy = cos(DEG2RAD(angles->y));
    sp = sin(DEG2RAD(angles->x));
    cp = cos(DEG2RAD(angles->x));

    forward->x = cp * cy;
    forward->y = cp * sy;
    forward->z = -sp;
}

// the function that we will replace when hooking. We know this function is 
// called with _thiscall, so ecx will have *this pointer
// this function will be called repeatedly by the game, and we can do whatever we want now.
// it also seems to pass in a CUserCmd object, which if we investigate further, holds info about
// the user view angles. So we can use this
bool __fastcall wrap_CreateMove(void* thisptr, DWORD edx, float flInputSampleTime, CUserCmd* cmd) {

    // Beep(1000, 100);
    // because CreateMove uses the thiscall convention which passes in the object as ecx (which is our first argument thanks to fastcall)
    void* localPlayerFromCMD = thisptr;

    // YEP THIS OFFSET IS A0, the height is the height displayed in game - 64 which is the height of the player. 
    // so in game height is where player head is, but the coordinates stored in memory are for the feet, and player height is at offset 0x108. OK
    // offsets found using reclass. Also cast to a vector ptr
    Vector* pPosition = (Vector*)((uintptr_t)localPlayerFromCMD + 0xA0);

    // pitch 12C offset, 0x130 is yaw. SO 0x12C (VERIFIED IN CHEAT ENGINE)
    // we get this from the cmd object actually instead of the offset, turns out the 
    // one from cmd object is more in sync and we will miss shots if we use the offset.
    // Vector* pViewAngles = (Vector*)((uintptr_t) pLocalPlayer + 0x12C);

    // turns out this is better- get info directly from CreateMove
    Vector* pViewAngles = &cmd->viewangles;

    // we will use camera offset here because otherwise the position value is the player's feet
    // this is always 0, 0, 64.06
    Vector* pCameraOffset = (Vector*)((uintptr_t)localPlayerFromCMD + 0x108);

    /* RAYCAST OUTPUT */
    // create angle vector for player position
    // angle vector takes an input and an output and does some weird shit with the values
    // apparently its getting the basis vectors? Not sure. Anyways do this with the view angles
    Vector angleOut;
    memset(&angleOut, 0, sizeof(angleOut));
    AngleVectors(pViewAngles, &angleOut);

    // add the 64 so that the position is at player head and not feed
    Vector pStart = *pPosition + *pCameraOffset;

    
    angleOut.x *= MAX_RANGE;
    angleOut.y *= MAX_RANGE;
    angleOut.z *= MAX_RANGE;
    
    // add current player position to angle
    angleOut = angleOut + pStart;

    if (display_stats) {
        printf("angleOut %f %f %f\n", angleOut.x, angleOut.y, angleOut.z);
        printf("pViewAngles: %f %f %f\n", pViewAngles->x, pViewAngles->y, pViewAngles->z);
        printf("pStart: %f %f %f\n", pStart.x, pStart.y, pStart.z);
        printf("AngleOut scaled %f %f %f\n", angleOut.x, angleOut.y, angleOut.z);
        printf("end position %f %f %f\n", angleOut.x, angleOut.y, angleOut.z);
    }

    // now we can make a ray apparently
    // pass in starting position as "start" and the angle vector we just made as end.
    // a ray is created into initialRay which represents a start position + and a magnitude/direction
    Ray_t initialRay;
    Ray_Init(&initialRay, pStart, angleOut);



    // call traceRay function from inside of CEngineTrace using the pointer we got via CreateInterface
    // traceRay will populate the last parameter of type trace_t.
    MyITraceFilter traceFilter;
    trace_t trace;

    // ask the game to trace a ray for us given the current state in the game, return value is in trace
    game_cEngineTrace->TraceRay(&initialRay, MASK_SHOT, (ITraceFilter*)&traceFilter, &trace);

    if (display_stats) {
        // santiy check
        printf("initialRay: %f %f %f\n", initialRay.m_Start.x, initialRay.m_Start.y, initialRay.m_Start.z);
        // this is kinda like where the ray ends up hitting I believe. A "collision"?
        printf("trace endpos: %f %f %f\n", trace.endpos.x, trace.endpos.y, trace.endpos.z);

        // not totally sure what fraction is, i think its how long it takes the traceray to complete?
        // it says 1.0 if it doesn't hit anything...
        printf("trace fraction: %f\n", trace.fraction);
    }
   

    // get world entity using offset
    C_BaseEntity* world_ent = pEntityList->m_EntyPtrArray[0].m_pEntity;
    // void* world_ent  = *(void**)((uintptr_t)hClient + 0x4DA20CC);

    // we have direct access to the entity object that we hit actually through the m_pEnt member variable
    // let's filter out the world entity and say that if we hit a player we will trigger the kill
    // so basically if we mouse over a player's head then we will kill him
    bool numShots = 0;
    // cmd->buttons = 0;
    if (trace.m_pEnt) {
        // make the user shoot!
        // printf("shooting: %d %d\n", trace.hitbox, trace.hitgroup);
        // Beep(400, 50);
        IClientNetworkable* i = trace.m_pEnt->GetClientNetworkable();
        char* targetClassName = i->GetClientClass()->m_pNetworkName;
        printf("Aiming At: %s\n", targetClassName);
        // check if a player object
        if (strcmp(targetClassName, "CCSPlayer") == 0) {
            // check if same team
            if (trace.m_pEnt->m_iTeamNum != pLocalPlayer->m_iTeamNum) {
                
                if (trace.hitgroup == HITGROUP_HEAD) {
                    cmd->buttons = 1;
                }
            }
        }

    }

    // test if the offset into player class is working 
    // printf("Player Health Test: %d\n", pLocalPlayer->playerHealth);

    if (display_stats) {
        display_stats = false;
    }
    // printf("Calling CreateMove %p %f %p\n", (void*) thisptr, flInputSampleTime, cmd);
    return real_CreateMove(thisptr, 0, flInputSampleTime, cmd);
    //return true;
}

// Using IDA we know that first 4 bytes of the class is the RTTI, then the Vtable info begins
// https://alex-robenko.gitbook.io/bare_metal_cpp/compiler_output/rtti
struct vtable_struct {
    void* RTTI;
    void* functions[1500]; // lets just arbitrarily pick 1000 for now
};

void** old_vtable_address;

void hook_createMove() {
    // save old vtable address
    old_vtable_address = *(void***)pLocalPlayer;
    printf("real vtable address: %p\n", old_vtable_address);

    // Hook the vtable and add in our custom function!
    vtable_struct vtable;

    // IT goes (*localPlayer + 289)
    memcpy(&vtable, (void*)((uintptr_t) old_vtable_address - 4), sizeof(vtable));

    for (int i = 0; i < 5; i++) {
        printf("%p\n", vtable.functions[i]);
    }

    // next, get the real function address for createMove. We will need this later
    real_CreateMove = (CreateMoveFn)vtable.functions[289];
    printf("real createMove address: %p\n", real_CreateMove);
    
    // load our version into the vtable
    vtable.functions[289] = wrap_CreateMove;

    printf("Our createMove address: %p\n", wrap_CreateMove);

    // reload the entire vtable back into localPlayer with our hooked function
    // we change the VPOINTER (triple *) to point to our new vtable's address
    // which is unmodified except our new hook function. I thought you would want to 
    // preserve the RTII + the 0 padding but I guess not?
    *(void***)pLocalPlayer = vtable.functions;
    printf("vtable now pointing at %p\n", vtable.functions);
    
}

void NetVarHelpers(RecvTable* t, int level) {
    if (!t) {
        return;
    }
    for (int i = 0; i < t->m_nProps; i++) {
        RecvProp* prop = &t->m_pProps[i];
        if (prop->m_pVarName && !isdigit(*prop->m_pVarName)) {
            // only print vars with non integer names
            for (int j = 0; j < level; j++) printf(" ");
            printf("%d, +%03x, %s, %s\n", i, prop->m_Offset, prop->m_pVarName, SendPropType_to_string(prop->m_RecvType));
        }
        if (prop->m_pDataTable && prop->m_RecvType == DPT_DataTable) {
            // recurse
            NetVarHelpers(prop->m_pDataTable, level + 1);
        }
        
    }
}


// doesn't work when called, but when you put it in Main, it works. WTF? 
//void NetVarStuff() {
//    // Netvars
//    ClientClass* clientClass = pLocalPlayer->GetClientNetworkable()->GetClientClass();
//
//    printf("Client class: %p\n", clientClass);
//    printf("Class name: %s\n", clientClass->m_pNetworkName);
//
//    //// get the RecvTable 
//    RecvTable* recvTable = clientClass->m_pRecvTable;
//    printf("RecvTable: %p\n", recvTable);
//    printf("RecvTable name: %s\n", recvTable->m_pNetTableName);
//
//    NetVarHelpers(recvTable, 0);
//
//}

DWORD CALLBACK Main(LPVOID arg) {
    Beep(440, 100);
    AllocConsole();
    // allow us to use that console
    FILE* f;
    // redirect your output stream to the newly created console.
    (void) freopen_s(&f, "CONIN$", "r", stdin);
    (void) freopen_s(&f, "CONOUT$", "w", stdout);
    (void) freopen_s(&f, "CONOUT$", "w", stderr);

    SetStdHandle(STD_INPUT_HANDLE, stdin);
    SetStdHandle(STD_OUTPUT_HANDLE, stdout);

    printf("Hello\n");
    std::cout.clear();
    // std::cout << "hi";
    // MessageBox(NULL, (L"Pause to see console output."), (L"Pause Here"), MB_OK | MB_SYSTEMMODAL | MB_ICONEXCLAMATION);
    
    
    // get a handle to the module called client(.dll)
    hClient = GetModuleHandleA("client");
    hEngine = GetModuleHandleA("engine");
    printf("hClient: %p\n", hClient);
    printf("hEngine: %p\n", hEngine);

    // get the address of the CreateInterface function in the dll, and cast that to a function ptr
    // of type CreateInterfaceFn which we have defined above
    CreateInterfaceFn Client_CreateInterface = (CreateInterfaceFn) GetProcAddress(hClient, "CreateInterface");
    CreateInterfaceFn Engine_CreateInterface = (CreateInterfaceFn) GetProcAddress(hEngine, "CreateInterface");
    
    // get the interface. We can now call functions like TraceRay()
    int returnValue;
    game_cEngineTrace = (CEngineTrace*) Engine_CreateInterface("EngineTraceClient004", &returnValue);
    printf("CEngineTrace: %p\n", game_cEngineTrace);

    // also got this offset from Cheat engine
    pEntityList = (cBaseEntityList*)((uintptr_t)hClient + 0x4DD244C);

    // alternatively you can just use the interface
    interfaceEntityList = (IClientEntityList*)Client_CreateInterface("VClientEntityList003", &returnValue);

    // offset found using cheat engine. Dereference the pointer once though
    // pLocalPlayer = *(void**)((uintptr_t) hClient + 0x52239CC);

    // alternatively use an interface
    pLocalPlayer = (C_CSPlayer*) interfaceEntityList->GetClientEntity(1);

    printf("localPlayer: %p\n", pLocalPlayer);

    // do the hook
    hook_createMove();

    // NetVarStuff();

    //ClientClass* clientClass = pLocalPlayer->GetClientNetworkable()->GetClientClass();
    //printf("Client class: %p\n", clientClass);
    //printf("Class name: %s\n", clientClass->m_pNetworkName);
    //RecvTable* recvTable = clientClass->m_pRecvTable;
    //printf("RecvTable: %p\n", recvTable);
    //printf("RecvTable name: %s\n", recvTable->m_pNetTableName);

    //NetVarHelpers(recvTable, 0);
    //Beep(200, 1000);

    //// enter infinite loop until we say to end
    pause();
    //// restore old vtable
    *(void***) pLocalPlayer = old_vtable_address;
    printf("vtable restored");

    //// remove this DLL by calling FreeLibrary with handle to this DLL as the argument
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) FreeLibrary, hmod, 0, NULL);
    
    return 0;
   
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{

    hmod = hModule;
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        HANDLE h = nullptr;
        h = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Main, NULL, 0, NULL);
        if (h) {
            CloseHandle(h);
        }
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

