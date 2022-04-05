// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <stdio.h>
#include <stdint.h>
#include <math.h>

// from csgo wiki
#define MAX_RANGE 8192.f

// All this stuff is pasted from bspflags.h
// contents flags are seperate bits
// a given brush can contribute multiple content bits
// multiple brushes can be in a single leaf

// these definitions also need to be in q_shared.h!

// lower bits are stronger, and will eat weaker brushes completely
#define	CONTENTS_EMPTY			0		// No contents

#define	CONTENTS_SOLID			0x1		// an eye is never valid in a solid
#define	CONTENTS_WINDOW			0x2		// translucent, but not watery (glass)
#define	CONTENTS_AUX			0x4
#define	CONTENTS_GRATE			0x8		// alpha-tested "grate" textures.  Bullets/sight pass through, but solids don't
#define	CONTENTS_SLIME			0x10
#define	CONTENTS_WATER			0x20
#define	CONTENTS_BLOCKLOS		0x40	// block AI line of sight
#define CONTENTS_OPAQUE			0x80	// things that cannot be seen through (may be non-solid though)
#define	LAST_VISIBLE_CONTENTS	CONTENTS_OPAQUE

#define ALL_VISIBLE_CONTENTS (LAST_VISIBLE_CONTENTS | (LAST_VISIBLE_CONTENTS-1))

#define CONTENTS_TESTFOGVOLUME	0x100
#define CONTENTS_UNUSED			0x200	
// unused 
// NOTE: If it's visible, grab from the top + update LAST_VISIBLE_CONTENTS
// if not visible, then grab from the bottom.
// CONTENTS_OPAQUE + SURF_NODRAW count as CONTENTS_OPAQUE (shadow-casting toolsblocklight textures)
#define CONTENTS_BLOCKLIGHT		0x400

#define CONTENTS_TEAM1			0x800	// per team contents used to differentiate collisions 
#define CONTENTS_TEAM2			0x1000	// between players and objects on different teams

// ignore CONTENTS_OPAQUE on surfaces that have SURF_NODRAW
#define CONTENTS_IGNORE_NODRAW_OPAQUE	0x2000

// hits entities which are MOVETYPE_PUSH (doors, plats, etc.)
#define CONTENTS_MOVEABLE		0x4000

// remaining contents are non-visible, and don't eat brushes
#define	CONTENTS_AREAPORTAL		0x8000

#define	CONTENTS_PLAYERCLIP		0x10000
#define	CONTENTS_MONSTERCLIP	0x20000

#define	CONTENTS_BRUSH_PAINT	0x40000
#define	CONTENTS_GRENADECLIP	0x80000
#define	CONTENTS_UNUSED2		0x100000
#define	CONTENTS_UNUSED3		0x200000
#define	CONTENTS_UNUSED4		0x400000
#define	CONTENTS_UNUSED5		0x800000

#define	CONTENTS_ORIGIN			0x1000000	// removed before bsping an entity

#define	CONTENTS_MONSTER		0x2000000	// should never be on a brush, only in game
#define	CONTENTS_DEBRIS			0x4000000
#define	CONTENTS_DETAIL			0x8000000	// brushes to be added after vis leafs
#define	CONTENTS_TRANSLUCENT	0x10000000	// auto set if any surface has trans
#define	CONTENTS_LADDER			0x20000000
#define CONTENTS_HITBOX			0x40000000	// use accurate hitboxes on trace

// -----------------------------------------------------
// spatial content masks - used for spatial queries (traceline,etc.)
// USE FOR fMask in TraceRay 
// -----------------------------------------------------
#define	MASK_ALL					(0xFFFFFFFF)
// everything that is normally solid
#define	MASK_SOLID					(CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_WINDOW|CONTENTS_MONSTER|CONTENTS_GRATE)
// everything that blocks player movement
#define	MASK_PLAYERSOLID			(CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_PLAYERCLIP|CONTENTS_WINDOW|CONTENTS_MONSTER|CONTENTS_GRATE)
// blocks npc movement
#define	MASK_NPCSOLID				(CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_MONSTERCLIP|CONTENTS_WINDOW|CONTENTS_MONSTER|CONTENTS_GRATE)
// blocks fluid movement
#define	MASK_NPCFLUID				(CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_MONSTERCLIP|CONTENTS_WINDOW|CONTENTS_MONSTER)
// water physics in these contents
#define	MASK_WATER					(CONTENTS_WATER|CONTENTS_MOVEABLE|CONTENTS_SLIME)
// everything that blocks lighting
#define	MASK_OPAQUE					(CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_OPAQUE)
// everything that blocks lighting, but with monsters added.
#define MASK_OPAQUE_AND_NPCS		(MASK_OPAQUE|CONTENTS_MONSTER)
// everything that blocks line of sight for AI
#define MASK_BLOCKLOS				(CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_BLOCKLOS)
// everything that blocks line of sight for AI plus NPCs
#define MASK_BLOCKLOS_AND_NPCS		(MASK_BLOCKLOS|CONTENTS_MONSTER)
// everything that blocks line of sight for players
#define	MASK_VISIBLE					(MASK_OPAQUE|CONTENTS_IGNORE_NODRAW_OPAQUE)
// everything that blocks line of sight for players, but with monsters added.
#define MASK_VISIBLE_AND_NPCS		(MASK_OPAQUE_AND_NPCS|CONTENTS_IGNORE_NODRAW_OPAQUE)
// bullets see these as solid
#define	MASK_SHOT					(CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_MONSTER|CONTENTS_WINDOW|CONTENTS_DEBRIS|CONTENTS_HITBOX)
// for finding floor height
#define MASK_FLOORTRACE				(CONTENTS_SOLID|CONTENTS_MOVEABLE|                 CONTENTS_WINDOW|CONTENTS_DEBRIS)
// for csgo weapon clipping
#define MASK_WEAPONCLIPPING			(CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_MONSTER|CONTENTS_WINDOW|CONTENTS_DEBRIS)
// bullets see these as solid, except monsters (world+brush only)
#define MASK_SHOT_BRUSHONLY			(CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_WINDOW|CONTENTS_DEBRIS)
// non-raycasted weapons see this as solid (includes grates)
#define MASK_SHOT_HULL				(CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_MONSTER|CONTENTS_WINDOW|CONTENTS_DEBRIS|CONTENTS_GRATE)
// hits solids (not grates) and passes through everything else
#define MASK_SHOT_PORTAL			(CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_WINDOW|CONTENTS_MONSTER)
// everything normally solid, except monsters (world+brush only)
#define MASK_SOLID_BRUSHONLY		(CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_WINDOW|CONTENTS_GRATE)
// everything normally solid for player movement, except monsters (world+brush only)
#define MASK_PLAYERSOLID_BRUSHONLY	(CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_WINDOW|CONTENTS_PLAYERCLIP|CONTENTS_GRATE)
// everything normally solid for npc movement, except monsters (world+brush only)
#define MASK_NPCSOLID_BRUSHONLY		(CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_WINDOW|CONTENTS_MONSTERCLIP|CONTENTS_GRATE)
// just the world, used for route rebuilding
#define MASK_NPCWORLDSTATIC			(CONTENTS_SOLID|CONTENTS_WINDOW|CONTENTS_MONSTERCLIP|CONTENTS_GRATE)
// just the world, used for route rebuilding
#define MASK_NPCWORLDSTATIC_FLUID	(CONTENTS_SOLID|CONTENTS_WINDOW|CONTENTS_MONSTERCLIP)
// These are things that can split areaportals
#define MASK_SPLITAREAPORTAL		(CONTENTS_WATER|CONTENTS_SLIME)

// everything that blocks corpse movement
// UNDONE: Not used yet / may be deleted
#define	MASK_DEADSOLID				(CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_WINDOW|CONTENTS_GRATE)


HMODULE hmod;
HMODULE hClient, hEngine;
uintptr_t pLocalPlayer; // pointer to local player

// interface.cpp
typedef void* (*CreateInterfaceFn)(const char* pName, int* pReturnCode);

// if you look in source its just 3 floats
struct Vector
{
    float x, y, z;
};

Vector operator+(Vector& v1, Vector &v2) {
    Vector vv;
    vv.x = v1.x + v2.x;
    vv.y = v1.y + v2.y;
    vv.z = v1.z + v2.z;
    return vv;
}

struct VectorAligned {
    float x, y, z, pad; // pad for byte alignment I think
};

typedef void* matrix3x4_t;

// cmodel.h
struct Ray_t {
    VectorAligned  m_Start;	// starting point, centered within the extents
    VectorAligned  m_Delta;	// direction + length of the ray
    VectorAligned  m_StartOffset;	// Add this to m_Start to get the actual ray start
    VectorAligned  m_Extents;	// Describes an axis aligned box extruded along a ray
    const matrix3x4_t* m_pWorldAxisTransform;
    bool	m_IsRay;	// are the extents zero?
    bool	m_IsSwept;	// is delta != 0?
};

typedef void* IHandleEntity;
enum TraceType_t
{
    TRACE_EVERYTHING = 0,
    TRACE_WORLD_ONLY,				// NOTE: This does *not* test static props!!!
    TRACE_ENTITIES_ONLY,			// NOTE: This version will *not* test static props
    TRACE_EVERYTHING_FILTER_PROPS,	// NOTE: This version will pass the IHandleEntity for props through the filter, unlike all other filters
};

class ITraceFilter
{
public:
    virtual bool ShouldHitEntity(IHandleEntity * pEntity, int contentsMask) = 0;
    virtual TraceType_t	GetTraceType() const = 0;
};

class MyITraceFilter : ITraceFilter {
    bool ShouldHitEntity(IHandleEntity* pEntity, int contentsMask) override {
        printf("ShouldHitEntity called: pEntity is %p, contentsMask: %d\n", pEntity, contentsMask);
        // don't count collisions with our local player
        if (pEntity == (void*) pLocalPlayer) {
            return false;
        }
        return true; // so it always hits said entity
    }
    TraceType_t	GetTraceType() const override {
        return TRACE_EVERYTHING; // we want to trace everything
    }
};

void pause()
{
    while (1)
    {
        if (GetAsyncKeyState(VK_F3))
        {
            break;
        }
        Sleep(100);
    }
}

typedef uint8_t byte;

// cmodel.h
struct csurface_t
{
    const char* name;
    short		surfaceProps;
    unsigned short	flags;		// BUGBUG: These are declared per surface, not per material, but this database is per-material now
};

struct cplane_t {
    Vector	normal;
    float	dist;
    byte	type;			// for fast side tests
    byte	signbits;		// signx + (signy<<1) + (signz<<1)
    byte	pad[2];
};

typedef void* C_BaseEntity;

// was CBaseTrace (trace.h->CGameTrace which inherits from CBaseTrace)
// trace_t contains info on 
// the start and end position of a ray, the time it took to hit the surface, what surface it hit, etc.
// populated by a function like TraceRay
struct trace_t {
    // from cBaseTrace
    Vector			startpos;				// start position
    Vector			endpos;					// final position
    cplane_t		plane;					// surface normal at impact

    float			fraction;				// time completed, 1.0 = didn't hit anything

    int				contents;				// contents on other side of surface hit
    unsigned short	dispFlags;				// displacement flags for marking surfaces with data

    bool			allsolid;				// if true, plane is not valid
    bool			startsolid;				// if true, the initial point was in a solid area

    // from CGameTrace (gametrace.h)
    float			fractionleftsolid;	// time we left a solid, only valid if we started in solid
    csurface_t		surface;			// surface hit (impact surface)

    int				hitgroup;			// 0 == generic, non-zero is specific body part

    short			physicsbone;		// physics bone hit by trace in studio
    unsigned short	worldSurfaceIndex;	// Index of the msurface2_t, if applicable

    // macro in gametrace.h, make void*. The full class is in c_baseentity.h, but its 
    // pretty long so we don't wanna copypaste it all here
    C_BaseEntity* m_pEnt;
    int			hitbox;					// box hit by trace in studio

    // avoid stack corruption
    int  spam[100];
};

// this class mimics IEngineTrace in IEngineTrace.h, which has 5 virtual functions before TraceRay
// which we pad away
class CEngineTrace
{
public:
    // Returns the contents mask at a particular world-space position
    virtual int		pad0() = 0;
    virtual int		pad1() = 0;
    virtual int		pad2() = 0;

    // Traces a ray against a particular edict
    virtual int		pad3() = 0;
    virtual int		pad4() = 0;

    // A version that simply accepts a ray (can work as a traceline or tracehull)
    virtual void	TraceRay(const Ray_t* ray, unsigned int fMask, ITraceFilter* pTraceFilter, trace_t* pTrace);
};




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

#define DEG2RAD(x) ((x/180.0f) * 3.141592653f)

// from mathlib/mathlib_base.cpp
// change QAngle& to Vector* in function arguments
void AngleVectors(const Vector* angles, Vector* forward)
{

    float	sp, sy, cp, cy; // p = pitch, y = yaw

    // extract the components
    sy = sin(DEG2RAD(angles->y));
    cy = cos(DEG2RAD(angles->x));
    sp = sin(DEG2RAD(angles->x));
    cp = cos(DEG2RAD(angles->x));

    forward->x = cp * cy;
    forward->y = cp * sy;
    forward->z = -sp;
}

// from clientmode_shared.cpp:316. We tweak it a bit. This will store the real CreateMove function!
// bool ClientModeShared::CreateMove(float flInputSampleTime, CUserCmd* cmd);
typedef bool(__fastcall* CreateMoveFn)(DWORD ecx, DWORD edx, float flInputSampleTime, void* cmd);

// this global variable houses the address of the real CreateMove function from csgo
CreateMoveFn real_CreateMove;
CEngineTrace* game_cEngineTrace;

// usercmd.h. Since this is actually a class we gotta add the vpointer
struct CUserCmd {

    void* vpointer;

    // For matching server and client commands for debugging
    int		command_number;

    // the tick the client created this command
    int		tick_count;

    // Player instantaneous view angles.
    Vector	viewangles;
    Vector	aimdirection;	// For pointing devices. 
    // Intended velocities
    //	forward velocity.
    float	forwardmove;
    //  sideways velocity.
    float	sidemove;
    //  upward velocity.
    float	upmove;
    // Attack button states
    int		buttons;
    // Impulse command issued.
    byte    impulse;
    // Current weapon id
    int		weaponselect;
    int		weaponsubtype;

    int		random_seed;	// For shared random functions

#ifndef CLIENT_DLL
    int		server_random_seed; // Only the server populates this seed
#endif

    short	mousedx;		// mouse accum in x from create move
    short	mousedy;		// mouse accum in y from create move

    // Client only, tracks whether we've predicted this command at least once
    bool	hasbeenpredicted;

};

// just like in shareddefs.h
#define HITGROUP_HEAD 1
// the function that we will replace when hooking. We know this function is 
// called with _thiscall, so ecx will have *this pointer
// this function will be called repeatedly by the game, and we can do whatever we want now.
// it also seems to pass in a CUserCmd object, which if we investigate further, holds info about
// the user view angles. So we can use this
bool __fastcall wrap_CreateMove(DWORD thisptr, DWORD edx, float flInputSampleTime, CUserCmd* cmd) {

    // offsets found using reclass. Also cast to a vector ptr
    Vector* pPosition = (Vector*)(pLocalPlayer + 0xA0);

    
    // we get this from the cmd object actually instead of the offset, turns out the 
    // one from cmd object is more in sync and we will miss shots if we use the offset.
    // Vector* pViewAngles = (Vector*)((uintptr_t) pLocalPlayer + 0x12C);
    Vector* pViewAngles = &cmd->viewangles;

    // we will use camera offset here because otherwise the position value is the player's feet
    Vector* pCameraOffset = (Vector*)((uintptr_t) pLocalPlayer + 0x108);
    printf("pPosition: %f %f %f\n", pPosition->x, pPosition->y, pPosition->z);
    printf("pViewAngles: %f %f %f\n", pViewAngles->x, pViewAngles->y, pViewAngles->z);
    printf("pCameraOffset: %f %f %f\n", pCameraOffset->x, pCameraOffset->y, pCameraOffset->z);
    /* RAYCAST OUTPUT */
    // create angle vector for player position
    // angle vector takes an input and an output and does some weird shit with the values
    // apparently its getting the basis vectors? Not sure. Anyways do this with the view angles
    Vector angleOut;
    memset(&angleOut, 0, sizeof(angleOut));
    AngleVectors(pViewAngles, &angleOut);
    printf("angleOut %f %f %f\n", angleOut.x, angleOut.y, angleOut.z);

    Vector pStart = *pPosition;
    pStart = pStart + *pCameraOffset;

    
    angleOut.x *= MAX_RANGE;
    angleOut.y *= MAX_RANGE;
    angleOut.z *= MAX_RANGE;
    // not sure why we're adding this but OK
    angleOut = angleOut + pStart;

    printf("end position %f %f %f\n", angleOut.x, angleOut.y, angleOut.z);



    // now we can make a ray apparently
    // pass in starting position as "start" and the angle vector we just made as end.
    // a ray is created into initialRay which represents a start position + and a magnitude/direction
    Ray_t initialRay;
    Ray_Init(&initialRay, *pPosition, angleOut);

    // santiy check
    printf("initialRay: %f %f %f\n", initialRay.m_Start.x, initialRay.m_Start.y, initialRay.m_Start.z);

    // call traceRay function from inside of CEngineTrace using the pointer we got via CreateInterface
    // traceRay will populate the last parameter of type trace_t.
    MyITraceFilter traceFilter;
    trace_t trace;
    game_cEngineTrace->TraceRay(&initialRay, MASK_SHOT, (ITraceFilter*)&traceFilter, &trace);
    printf("trace endpos: %f %f %f\n", trace.endpos.x, trace.endpos.y, trace.endpos.z);

    // not totally sure what fraction is, i think its how long it takes the traceray to complete?
    // it says 1.0 if it doesn't hit anything...
    printf("trace fraction: %f\n", trace.fraction);

    // get world entity using offset
    C_BaseEntity world_ent = *(void**) ((uintptr_t)hClient + 0x4DA20CC);

    // we have direct access to the entity object that we hit actually through the m_pEnt member variable
    // let's filter out the world entity and say that if we hit a player we will trigger the kill
    // so basically if we mouse over a player's head then we will kill him
    if (trace.m_pEnt && trace.m_pEnt != world_ent && trace.hitgroup == HITGROUP_HEAD) {
        // make the user shoot!
        printf("shooting: %d %d\n", trace.hitbox, trace.hitgroup);
        cmd->buttons |= 1;
    }
    else {
        cmd->buttons = 0;
    }

    printf("Calling CreateMove %p %f %p", thisptr, flInputSampleTime, cmd);
    return real_CreateMove(thisptr, 0, flInputSampleTime, cmd);
}

// first 4 bytes are just zeros, then the RTTI, then the address of first virtual func
// https://alex-robenko.gitbook.io/bare_metal_cpp/compiler_output/rtti
struct vtable_struct {
    void* zeros;
    void* RTTI;
    void* functions[1000]; // lets just arbitrarily pick 1000 for now
};

void** old_vtable_address;

void hook_createMove() {
    // save old vtable address
    old_vtable_address = *(void***)pLocalPlayer;

    // Hook the vtable and add in our custom function!
    vtable_struct vtable;
    // pointer to vtable is a double ptr, (ptr to a bunch of addresses, aka ptr to a ptr)
    // lets cast it to point to the first address and then specify the size. Basically copy 
    // the table into vtable.
    memcpy(&vtable, *(void**)pLocalPlayer, sizeof(vtable));

    // next, get the real function address for createMove. We will need this later
    real_CreateMove = (CreateMoveFn)vtable.functions[288];
    // load our version into the vtable
    vtable.functions[288] = wrap_CreateMove;

    // reload the entire vtable back into localPlayer with our hooked function
    // we change the VPOINTER (triple *) to point to our new vtable's address
    // which is unmodified except our new hook function. I thought you would want to 
    // preserve the RTII + the 0 padding but I guess not?
    *(void***)pLocalPlayer = vtable.functions;
    printf("vtable now pointing at %p\n", *(void***)pLocalPlayer);
}

DWORD CALLBACK Main(LPVOID arg) {
    Beep(440, 1000);
    AllocConsole();

    // allow us to use that console
    freopen("CONIN$", "r", stdin);
    freopen("CONOUT$", "w", stdout);
    printf("Hello\n");
    
    // get a handle to the module called client(.dll)
    hClient = GetModuleHandleA("client");
    hEngine = GetModuleHandleA("engine");

    // get the address of the CreateInterface function in the dll, and cast that to a function ptr
    // of type CreateInterfaceFn which we have defined above
    CreateInterfaceFn Client_CreateInterface = (CreateInterfaceFn) GetProcAddress(hClient, "CreateInterface");
    CreateInterfaceFn Engine_CreateInterface = (CreateInterfaceFn) GetProcAddress(hEngine, "CreateInterface");
    
    // get the interface. We can now call functions like TraceRay()
    int returnValue;
    game_cEngineTrace = (CEngineTrace*) Engine_CreateInterface("EngineTraceClient004", &returnValue);
    printf("CEngineTrace: %p\n", game_cEngineTrace);

    // offset found using cheat engine. Dereference the pointer once though
    pLocalPlayer = *(uintptr_t*)((uintptr_t) hClient + 0x04DA20DC);

    printf("localPlayer: %p\n", pLocalPlayer);

    // do the hook
    hook_createMove();

    // enter infinite loop until we say to end
    pause();
    // restore old vtable
    *(void***) pLocalPlayer = old_vtable_address;

    // remove this DLL by calling FreeLibrary with handle to this DLL as the argument
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
        CloseHandle(CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Main, NULL, 0, NULL));
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

