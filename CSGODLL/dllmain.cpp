// dllmain.cpp : Defines the entry point for the DLL application.
#include <Windows.h>
#include "csgo.h"
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <algorithm>
#include <unordered_map>
#include <iostream>
#include <Psapi.h>


// latest offsets from https://github.com/frk1/hazedumper/blob/master/csgo.hpp
namespace offsets {
    // wrt client
    constexpr auto dwGlowObjectManager = 0x531C058;
    constexpr auto dwEntityList = 0x4DD344C;

    // wrt player
    constexpr auto m_iGlowIndex = 0x10488;
}

HMODULE hmod;
HMODULE hClient, hEngine;
C_CSPlayer* pLocalPlayer; // pointer to local player
cBaseEntityList* pEntityList; // pointer to entity list
Vector* pCameraOffset;
uintptr_t glowObjectManager;

// global bools
bool norecoil = false;
bool bhop = false;
bool triggerbot = false;
bool display_stats = false;
bool aim_assist = false;
bool glowies = false;

// hook related stuff
CreateMoveFn real_CreateMove;
ApplyMouseFn real_ApplyMouse;

// Using IDA we know that first 4 bytes of the class is the RTTI, then the Vtable info begins
// https://alex-robenko.gitbook.io/bare_metal_cpp/compiler_output/rtti
struct vtable_struct {
    void* RTTI;
    void* functions[1500]; // lets just arbitrarily pick 1000 for now
} vtable;

vtable_struct fake_cinput_vtable;


void** old_ccsplayer_vtable_address = 0; // localplayer vtable
void** old_ccsinput_vtable_address = 0; // same as gcinput

// for using traceRay, obtained via interface
CEngineTrace* game_cEngineTrace;

// another interface export, need GetLocalPlayer() from this
IVEngineClient* engineClient;

// vtable 1
void** cinput_vtable;
cInput* gcinput;

bool dll_exiting;

DWORD waitForToggles(LPVOID lpThreadParameter)
{
    while (!dll_exiting)
    {
        if (GetAsyncKeyState(VK_F1)) {
            // toggle displaying stats
            norecoil = !norecoil;
            if (norecoil == true) {
                puts("norecoil turned on");
            }
            else {
                puts("norecoil turned off");
            }
        }
        if (GetAsyncKeyState(VK_F2)) {
            // toggle displaying stats
            bhop = !bhop;
            if (bhop == true) {
                puts("bhop turned on");
            }
            else {
                puts("bhop turned off");
            }
        }
        if (GetAsyncKeyState(VK_F4)) {
            // toggle displaying stats
            triggerbot = !triggerbot;
            if (triggerbot == true) {
                puts("triggerbot turned on");
            }
            else {
                puts("triggerbot turned off");
            }
        }
        if (GetAsyncKeyState(VK_F6)) { // f5 is snipping tool
            // toggle displaying stats
            aim_assist = !aim_assist;
            if (aim_assist == true) {
                puts("aim assist turned on");
            }
            else {
                puts("aim assist turned off");
            }
        }
        //if (GetAsyncKeyState(VK_F7)) {
        //    // glow enemies
        //    glowies = !glowies;
        //    if (glowies) {
        //        puts("enemy glow turned on");
        //    }
        //    else {
        //        puts("enemy glow turned off");
        //    }
        //}
        if (GetAsyncKeyState(VK_F3))
        {
            break;
        }
        Sleep(100);
    }
    return 0;
}

/*
BaseAddress: Starting Address to search from
SearchLen: How many bytes to search
pattern: The actual signature
mask: which characters we care to search for

returns: The first address where the pattern matches
*/
void* Find_Pattern(BYTE* BaseAddress, uintptr_t SearchLen, BYTE* pattern, BYTE* mask) {
    BYTE* end = BaseAddress + SearchLen;
    int pattern_length = strlen((char*) mask);
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
    return nullptr;
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

// method to dump all the offets in table t
void NetVarHelpers(RecvTable* t, int level) {
    if (!t) {
        return;
    }
    for (int i = 0; i < t->m_nProps; i++) {
        RecvProp* prop = &t->m_pProps[i];
        if (prop->m_pVarName && !isdigit(*prop->m_pVarName)) {
            // only print vars with non integer names
            for (int j = 0; j < level; j++) printf(" ");
            printf("%d, +%05x, %s, %s\n", i, prop->m_Offset, prop->m_pVarName, SendPropType_to_string(prop->m_RecvType));
        }
        if (prop->m_pDataTable && prop->m_RecvType == DPT_DataTable) {
            // recurse
            NetVarHelpers(prop->m_pDataTable, level + 1);
        }

    }
}

// method to look for a specific property in a table

ptrdiff_t get_netvar_offset(RecvTable* t, const char* prop_name, ptrdiff_t offset = 0) {
    for (int i = 0; i < t->m_nProps; i++) {
        RecvProp* prop = &t->m_pProps[i];
        if (prop->m_pVarName && !strcmp(prop->m_pVarName, prop_name)) {
            // found prop
            return offset + prop->m_Offset;
        }
        if (prop->m_pDataTable && prop->m_RecvType == DPT_DataTable) {
            // recursively search for it
            ptrdiff_t res = get_netvar_offset(prop->m_pDataTable, prop_name, offset + prop->m_Offset);
            if (res != -1) {
                return res;
            }
        }
    }
    // not found
    return -1;
}

// use a template because netvar variables come in 3 kinds of types- vector, int and float
std::unordered_map<ClientClass*, std::unordered_map<std::string, ptrdiff_t>> offset_hashmap;
template <typename T>
T getNetVar(IClientNetworkable* ent, const char* prop_name) {
    ClientClass* clientClass = ent->GetClientClass();
    if (offset_hashmap.find(clientClass) != offset_hashmap.end()) {
        auto cache = offset_hashmap[clientClass];
        if (cache.find(std::string(prop_name)) != cache.end()) {
            // we have the offset, just return the value (using our template type!)
            // this offset is relative to the entire entity base table address, which we can get using GetDataTableBasePtr()
            return *(T*)((uintptr_t)ent->GetDataTableBasePtr() + cache[std::string(prop_name)]);
        }
        else {
            goto search;
        }
    }
search:
    ptrdiff_t offset = get_netvar_offset(clientClass->m_pRecvTable, prop_name);
    // store this offset in the cache so we don't have to look for it next time
    offset_hashmap[clientClass][std::string(prop_name)] = offset;
    return *(T*)((uintptr_t)ent->GetDataTableBasePtr() + offset);
}

// doesn't work when called, but when you put it in Main, it works. WTF? 
void NetVarStuff() {
    // Netvars
    ClientClass* clientClass = pLocalPlayer->GetClientNetworkable()->GetClientClass();

    printf("Client class: %p\n", clientClass);
    printf("Class name: %s\n", clientClass->m_pNetworkName);

    //// get the RecvTable 
    RecvTable* recvTable = clientClass->m_pRecvTable;
    printf("RecvTable: %p\n", recvTable);
    printf("RecvTable name: %s\n", recvTable->m_pNetTableName);

    NetVarHelpers(recvTable, 0);

}

void  __fastcall do_norecoil(void* thisptr, int nSlot, QAngle& viewangles, CUserCmd* cmd, float& mouse_x, float& mouse_y) {
    // printf("Mouse coords: %f %f\n", mouse_x, mouse_y);

    // we make these vars static because we want them to live after the ApplyMouse function call
    static int lastUpdateTick = 0;
    static Vector lastAimPunchAngle;

    float recoil_scale = 2.0; // default
    float m_yaw = 0.022f;

    Vector currentPunchAngles = getNetVar<Vector>(pLocalPlayer->GetClientNetworkable(), "m_aimPunchAngle");
    int currentTick = getNetVar<int>(pLocalPlayer->GetClientNetworkable(), "m_nTickBase");
    if (lastUpdateTick != currentTick) {
        // new tick to deal with
        Vector angleChange = currentPunchAngles - lastAimPunchAngle;
        angleChange *= (-recoil_scale / m_yaw);
        // printf("%f %f %f %f %f %f \n", currentPunchAngles.x, currentPunchAngles.y, currentPunchAngles.z, lastAimPunchAngle.x, lastAimPunchAngle.y, lastAimPunchAngle.z);
        // printf("%f %f \n", angleChange.x, angleChange.y);

        // right is pos, left is negative. up is negative, down is positive
        // recoil goes (vertical_change, -horizontal change)
        mouse_x += -angleChange.y;
        mouse_y += angleChange.x;

        lastUpdateTick = currentTick;
        lastAimPunchAngle = currentPunchAngles;
    }
    
}


void __fastcall do_aim_assist(float& mouse_x) {
    // entity 0 is world, 1 is player
    float smallestYaw = 1000.f;
    int nearestEntIdx = 65; // because there can only be 64 entities

    Vector myPos = *(Vector*)((uintptr_t)pLocalPlayer + 0xA0);
    myPos += *pCameraOffset;
    float myYaw = clamp(*(float*)((uintptr_t)pLocalPlayer + 0x130));

    for (int i = 0; i < 64; i++) {
        IClientNetworkable* nent = interfaceEntityList->GetClientNetworkable(i);
        if (!interfaceEntityList->GetClientEntity(i) || !nent) {

            continue;
        }
        if (strcmp(nent->GetClientClass()->GetName(), "CCSPlayer")) {

            continue;
        }
        C_CSPlayer* other_player = (C_CSPlayer*)interfaceEntityList->GetClientEntity(i);
        if (other_player == pLocalPlayer || other_player->playerHealth == 0) {

            continue;
        }

        Vector theirPos = other_player->position;
        theirPos += *pCameraOffset;
        Vector diff = theirPos - myPos; // a vector that originates from them to us

        //printf("myPos: %f %f %f\n", myPos.x, myPos.y, myPos.z);
        //printf("theirPos: %f %f %f\n", theirPos.x, theirPos.y, theirPos.z);
        //printf("diff: %f %f %f\n", diff.x, diff.y, diff.z);
        //// check if player can even see this other player
        //Ray_t initialRay;
        //Ray_Init(&initialRay, myPos, diff);
        //MyITraceFilter traceFilter;
        //trace_t trace;

        //// ask the game to trace a ray for us given the current state in the game, return value is in trace
        //game_cEngineTrace->TraceRay(&initialRay, MASK_VISIBLE, (ITraceFilter*)&traceFilter, &trace);
        //const char* name = trace.m_pEnt->GetClientNetworkable()->GetClientClass()->GetName();
        //if (strcmp(name, "CCSPlayer") != 0) {
        //    printf("%f %f %f \n", trace.endpos.x, trace.endpos.y, trace.endpos.z);
        //    // then we didn't hit a player so don't consider aim-assisting this player since its not visible
        //    continue;
        //}
        
        float deltaYaw = clamp(atan2f(diff.y, diff.x) * (180.0f / 3.14159265358f)); // returns value between -pi and pi radians
        float angle_difference = deltaYaw - myYaw;
        // printf("%d %f \n", i, angle_difference);
        if (abs(angle_difference) < abs(smallestYaw)) {
            smallestYaw = angle_difference; // dont store the absolute value version because we need the sign for later
            nearestEntIdx = i;
        }

    }
    // if we found an entity in our FOV, then move the mouse slightly towards their position every tick
    // 15 degrees field of view on either side, anything else will not be aimassisted
    float aimassist_fov = 15.0f;
    float aimassist_strength = 0.7f;
    if (nearestEntIdx != 65 && abs(smallestYaw) < aimassist_fov) {
        printf("\raim assisting Ent: %d, at angle: %f\n", nearestEntIdx, smallestYaw);
        //int correct_way = copysignf(1.f, smallestYaw);
        //int mouse_way = copysignf(1.f, -mouse_x);
        //float scale = aimassist_strength * abs(smallestYaw) / aimassist_fov;
        //printf("aimassist %.02fx\n", scale);
        //if (correct_way == mouse_way)
        //{
        //    mouse_x *= (1.f + aimassist_strength);
        //}
        //else
        //{
        //    mouse_x *= (1.f - aimassist_strength);
        //}
        //
        
        float sign = mouse_x * smallestYaw; // check if mouse movement + yaw requirements are the same. If they are different this will be pos number (neg*neg or pos*pos), else it will be negative
        if (sign < 0) {
            // wrong direction, nerf movement
            mouse_x *= 1.f + aimassist_strength;
        }
        else {
            // right direciton, increase movement
            mouse_x *= 1.f - aimassist_strength;
        }
        
    }
}
volatile float accum_mouse_dx = 0.0f;

// called every time the mouse moves I think? Doesn't happen at the tick rate
bool __fastcall wrap_ApplyMouse(void* thisptr, DWORD edx, int nSlot, QAngle& viewangles, CUserCmd* cmd, float mouse_x, float mouse_y) {
    
    // accumulate all the mouse changes between ticks, every time the server ticks this gets reset to 0
    accum_mouse_dx += mouse_x;
    if (norecoil) {
        do_norecoil(thisptr, nSlot, viewangles, cmd, mouse_x, mouse_y);
    }

    if (aim_assist) {
        do_aim_assist(mouse_x);
    }

    // printf("%f %f\n", mouse_x, mouse_y);
    return real_ApplyMouse(thisptr, 0, nSlot, viewangles, cmd, mouse_x, mouse_y);
}



void do_bhop(void* thisptr, float flInputSampleTime, CUserCmd* cmd) {
    int curFlags = pLocalPlayer->m_fFlags;
    if (curFlags & FL_ONGROUND) {
        *(int*)((uintptr_t)hClient + 0x3203988) = 0;
        *(int*)((uintptr_t)hClient + 0x3203994) = 0;
        cmd->buttons |= IN_JUMP;
    }
    else {
        // strafe in air, in a direction depending on overall mouse input since last tick

        // printf("mousedx: %hu\n", accum_mouse_dx);
        if (accum_mouse_dx < 0) {
            // left
            cmd->sidemove = -450.0f;
            printf("\rMoving Left\n");
            //*(int*)((uintptr_t)hClient + 0x3203988) = 1;
        }
        else if (accum_mouse_dx > 0) {
            cmd->sidemove = 450.0f;
            printf("\rMoving Right\n");
            //*(int*)((uintptr_t)hClient + 0x3203994) = 1;
        }

    }
    accum_mouse_dx = 0.0f;
    // infinite ducking?
    cmd->buttons |= IN_BULLRUSH;
}

void do_triggerbot(void* thisptr, float flInputSampleTime, CUserCmd* cmd) {
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

    // figure out what weapon we're holding
    CBaseHandle weapon_ehandle = getNetVar<CBaseHandle>(pLocalPlayer->GetClientNetworkable(), "m_hActiveWeapon");
    int weapon_ent_index = weapon_ehandle.m_Index & ENT_ENTRY_MASK;
    auto weapon_ent = (C_WeaponCSBase*) interfaceEntityList->GetClientEntity(weapon_ent_index);
    if (!weapon_ent_index || !weapon_ent)
    {
        printf("bad weapon?\n");
        return;
    }

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
        // printf("Aiming At: %s\n", targetClassName);
        // check if a player object
        if (strcmp(targetClassName, "CCSPlayer") == 0) {
            // can cast to player object now, "upgraded" from just entity
            C_CSPlayer* other_player = (C_CSPlayer*)trace.m_pEnt;
            // check if same team + isnt dead already
            printf("%d\n", other_player->m_iTeamNum);
            if (other_player->m_iTeamNum != pLocalPlayer->m_iTeamNum && (other_player->playerHealth != 0)) {
                // check headshots only
                if (trace.hitgroup == HITGROUP_HEAD) {
                    cmd->buttons |= IN_ATTACK;
                }
            }
        }

    }

    // test if the offset into player class is working 
    // printf("Player Health Test: %d\n", pLocalPlayer->playerHealth);

    if (display_stats) {
        display_stats = false;
    }
}

// doesnt work :(
void do_glow() {
    printf("glowObjectManager: %p\n", glowObjectManager);
    for (auto i = 2; i < 64; i++) {
        C_BaseEntity* player = (C_BaseEntity*)((uintptr_t)pEntityList + i * 0x10);
        printf("player: %p\n", player);
        if (player->m_iTeamNum == pLocalPlayer->m_iTeamNum) {
            // don't light up our teammates
            continue;
        }
        int32_t glowIndex = *(int32_t*)((uintptr_t) player + offsets::m_iGlowIndex);
        printf("red: %p\n", glowObjectManager + (glowIndex * 0x38) + 0x8);
        *(float*)(glowObjectManager + (glowIndex * 0x38) + 0x8) = 1.f; // r
        *(float*)(glowObjectManager + (glowIndex * 0x38) + 0xC) = 0.f; // g
        *(float*)(glowObjectManager + (glowIndex * 0x38) + 0x10) = 0.f; // b
        *(float*)(glowObjectManager + (glowIndex * 0x38) + 0x14) = 1.f; // a

        *(bool*)(glowObjectManager + (glowIndex * 0x38) + 0x27) = 1; // render occlude
        *(bool*)(glowObjectManager + (glowIndex * 0x38) + 0x28) = 1; // render unocclude
    }
}

// the function that we will replace when hooking. We know this function is 
// called with _thiscall, so ecx will have *this pointer
// this function will be called repeatedly by the game, and we can do whatever we want now.
// it also seems to pass in a CUserCmd object, which if we investigate further, holds info about
// the user view angles. So we can use this
bool __fastcall wrap_CreateMove(void* thisptr, DWORD edx, float flInputSampleTime, CUserCmd* cmd) {
    if (triggerbot) {
        do_triggerbot(thisptr, flInputSampleTime, cmd);
    }

    if (bhop) {
        do_bhop(thisptr, flInputSampleTime, cmd);
    }
    //if (glowies) {
    //    do_glow();
    //}
    
    // XD ok this doesnt work :/
    // *(int*)(pLocalPlayer->playerHealth) = 100;

    // printf("Calling CreateMove %p %f %p\n", (void*) thisptr, flInputSampleTime, cmd);
    return real_CreateMove(thisptr, 0, flInputSampleTime, cmd);
    //return true;
}



void hook_createMove() {
    printf("\n");
    printf("Doing CreateMove hook\n");
    // save old vtable address
    old_ccsplayer_vtable_address = *(void***)pLocalPlayer;
    printf("Real vtable address: %p\n", old_ccsplayer_vtable_address);

    // Copy the old vtable into our mock vtable. 
    memcpy(&vtable, (void*)((uintptr_t)old_ccsplayer_vtable_address - 4), sizeof(vtable));

    //for (int i = 0; i < 5; i++) {
    //    printf("%p\n", vtable.functions[i]);
    //}

    // next, get the real function address for createMove. We will need this later
    real_CreateMove = (CreateMoveFn)vtable.functions[289];
    printf("Real createMove address: %p\n", real_CreateMove);
    
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

void resolveExportedThings() {
    // get a handle to the module called client(.dll)
    hClient = GetModuleHandleA("client");
    hEngine = GetModuleHandleA("engine");
    printf("hClient: %p\n", hClient);
    printf("hEngine: %p\n", hEngine);

    if (!hClient || !hEngine) {
        printf("Missing an interface from csgo\n");
        return;
    }

    // get the address of the CreateInterface function in the dll, and cast that to a function ptr
    // of type CreateInterfaceFn which we have defined above

    CreateInterfaceFn Client_CreateInterface = (CreateInterfaceFn)GetProcAddress(hClient, "CreateInterface");
    CreateInterfaceFn Engine_CreateInterface = (CreateInterfaceFn)GetProcAddress(hEngine, "CreateInterface");

    // get the interface. We can now call functions like TraceRay()
    int returnValue;
    game_cEngineTrace = (CEngineTrace*)Engine_CreateInterface("EngineTraceClient004", &returnValue);
    printf("CEngineTrace: %p\n", game_cEngineTrace);

    // also got this offset from Cheat engine (or from hazedumper, they call it dwEntityList
    pEntityList = (cBaseEntityList*)((uintptr_t)hClient + offsets::dwEntityList);

    // alternatively you can just use the interface
    interfaceEntityList = (IClientEntityList*)Client_CreateInterface("VClientEntityList003", &returnValue);

    engineClient = (IVEngineClient*)Engine_CreateInterface("VEngineClient014", &returnValue);
}

void hook_ApplyMouse() {
    printf("\nDoing ApplyMouse hook..\n");
    IMAGE_DOS_HEADER* dos_hdr = (IMAGE_DOS_HEADER*)hClient;
    IMAGE_NT_HEADERS* nt_hdr = (IMAGE_NT_HEADERS*)((uintptr_t)hClient + dos_hdr->e_lfanew);
    DWORD imageSize = nt_hdr->OptionalHeader.SizeOfImage;
    printf("Size of client.dll: %d bytes\n", imageSize);

    // pattern scan for GCInput so we can install hooks into it
    void* pat = Find_Pattern((BYTE*)hClient, imageSize, (BYTE*)"\xB9\xFF\xFF\xFF\xFF\x6A\xFF\xFF\x90\xFF\xFF\xFF\xFF\x85\xC0\x75\xFF\x8B\x06\x8B\xCE\xFF\x90\xFF\xFF\xFF\xFF",
        (BYTE*)"x????x?xx????xxx?xxxxxx????");

    printf("Pattern located at %p\n", pat);
    gcinput = *(cInput**)((uintptr_t)pat + 1); // get address of gcinput, which points to the cInput vtable

    printf("gcinput at %p\n", gcinput);

    //printf("cinput_addr: %p\n", cinput_addr);
    // cinput_vtable = *(void***)((uintptr_t)hClient + 0x5224A30);

    cinput_vtable = *(void***)(gcinput);
    printf("original cinput vtable at %p\n", cinput_vtable);
    memcpy(&fake_cinput_vtable, (void*)((uintptr_t)cinput_vtable - 4), sizeof(fake_cinput_vtable));

    real_ApplyMouse = (ApplyMouseFn)fake_cinput_vtable.functions[55];
    printf("applymouse at %p\n", real_ApplyMouse);
    fake_cinput_vtable.functions[55] = wrap_ApplyMouse;

    // install new vtable
    *(void***)gcinput = fake_cinput_vtable.functions;
}

void setupCheat() {
   
    resolveExportedThings();

    hook_ApplyMouse();

    // offset found using cheat engine. Dereference the pointer once though
    // pLocalPlayer = *(void**)((uintptr_t) hClient + 0x52239CC);

    C_CSPlayer* currentPlayer;
    
    while (!GetAsyncKeyState(VK_F3)) {
        int localPlayerIdx = engineClient->GetLocalPlayer();
        currentPlayer = (C_CSPlayer*)interfaceEntityList->GetClientEntity(localPlayerIdx); // returns 0 when not in game
        // while we ein menu or if playing the game
        while (!currentPlayer || currentPlayer == pLocalPlayer) {
            // do nothing
            Sleep(200);
            if (!currentPlayer) {
                puts("\rPlease join a game to begin...\n");
            }
            // printf("localPlayerIdx: %d, currentPlayer %p, pLocalPlayer = %p\n", localPlayerIdx, currentPlayer, pLocalPlayer);
            localPlayerIdx = engineClient->GetLocalPlayer();
            currentPlayer = (C_CSPlayer*)interfaceEntityList->GetClientEntity(localPlayerIdx);
            if (GetAsyncKeyState(VK_F3)) {
                goto unhook_and_exit;
            }
        }

        // otherwise we have just joined game or switched levels
        pLocalPlayer = currentPlayer;
        printf("localPlayer: %p\n", pLocalPlayer);
        // we will use camera offset here because otherwise the position value is the player's feet? I think
        // this is always 0, 0, 64.06
        pCameraOffset = (Vector*)((uintptr_t)pLocalPlayer + 0x108);
        glowObjectManager = ((uintptr_t) hClient + offsets::dwGlowObjectManager);

        // do the hook
        hook_createMove();
        printf("F1: No recoil, F2: BHop, F3: Quit, F4: Trigger Bot, F6: Aim Assist\n");
    }

    // NetVarStuff();

    // ClientClass* clientClass = pLocalPlayer->GetClientNetworkable()->GetClientClass();
    //printf("Client class: %p\n", clientClass);
    //printf("Class name: %s\n", clientClass->m_pNetworkName);
    //RecvTable* recvTable = clientClass->m_pRecvTable;
    //printf("RecvTable: %p\n", recvTable);
    //printf("RecvTable name: %s\n", recvTable->m_pNetTableName);

    //NetVarHelpers(recvTable, 0);
    //Beep(200, 1000);

    // Get size of client.dll
    //LPMODULEINFO res{};
    //GetModuleInformation(GetCurrentProcess(), hClient, res, sizeof(LPMODULEINFO));
    //printf("Size of client.dll: %x\n", res->SizeOfImage);

    //ptrdiff_t p = get_netvar_offset(clientClass->m_pRecvTable, "m_aimPunchAngle");
    //printf("m_aimPunchAngle offset: %x\n", p);

    //// enter infinite loop until we say to end. Also this lets us toggle buttons to activate cheats

unhook_and_exit:
    dll_exiting = true;
    //// restore old vtables (createMove hook might not exist if we only sat in the menu)
    if (old_ccsplayer_vtable_address && pLocalPlayer) {
        *(void***)pLocalPlayer = old_ccsplayer_vtable_address;
    }

    *(void***)gcinput = cinput_vtable;

    printf("vtables restored");
}

DWORD CALLBACK Main(LPVOID arg) {
    dll_exiting = false;
    Beep(440, 100);
    DWORD threadId;
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) waitForToggles, NULL, NULL, &threadId);
    printf("Input handle thread started with ID %d\n", threadId);
    AllocConsole();
    // allow us to use that console
    FILE* f;
    // redirect your output stream to the newly created console.
    (void) freopen_s(&f, "CONIN$", "r", stdin);
    (void) freopen_s(&f, "CONOUT$", "w", stdout);
    (void) freopen_s(&f, "CONOUT$", "w", stderr);

    SetStdHandle(STD_INPUT_HANDLE, stdin);
    SetStdHandle(STD_OUTPUT_HANDLE, stdout);

    // printf("Hello\n");
    // std::cout.clear();
    // std::cout << "hi";
    // MessageBox(NULL, (L"Pause to see console output."), (L"Pause Here"), MB_OK | MB_SYSTEMMODAL | MB_ICONEXCLAMATION);
    
    setupCheat();


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

