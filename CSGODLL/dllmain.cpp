// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <stdio.h>
#include <stdint.h>


HMODULE hmod;
HMODULE hClient, hEngine;

// grab all this from CSGO source code
typedef void* (*CreateInterfaceFn)(const char*, int*);

// if you look in source its just 3 floats
struct Vector
{
    float x, y, z;
};

struct VectorAligned {
    float x, y, z, pad; // pad for byte alignment I think
};

typedef void* matrix3x4_t;

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
        return true; // so it always hits said entity
    }
    TraceType_t	GetTraceType() const override {
        return TRACE_ENTITIES_ONLY; // not sure why we return this one but oK
    }
};

typedef uint8_t byte;

struct cplane_t {
    Vector	normal;
    float	dist;
    byte	type;			// for fast side tests
    byte	signbits;		// signx + (signy<<1) + (signz<<1)
    byte	pad[2];
};

// was CBaseTrace (trace.h)
struct trace_t {
    // these members are aligned!!
    Vector			startpos;				// start position
    Vector			endpos;					// final position
    cplane_t		plane;					// surface normal at impact

    float			fraction;				// time completed, 1.0 = didn't hit anything

    int				contents;				// contents on other side of surface hit
    unsigned short	dispFlags;				// displacement flags for marking surfaces with data

    bool			allsolid;				// if true, plane is not valid
    bool			startsolid;				// if true, the initial point was in a solid area

};
class CEngineTrace
{
    // Returns the contents mask at a particular world-space position
    virtual int		pad0() = 0;
    virtual int		pad1() = 0;
    virtual int		pad2() = 0;

    // Traces a ray against a particular edict
    virtual int		pad3() = 0;

    // A version that simply accepts a ray (can work as a traceline or tracehull)
    virtual void	TraceRay(const Ray_t& ray, unsigned int fMask, ITraceFilter* pTraceFilter, trace_t* pTrace);
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
    CreateInterfaceFn Engine_CreateInterface = (CreateInterfaceFn)GetProcAddress(hClient, "CreateInterface");
    


    // remove the DLL
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

