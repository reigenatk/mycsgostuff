#pragma once

#include "EntityList.h"
#include "NetVar.h"
#include "C_CSPlayer.h"
#include <stdio.h>

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


#define DEG2RAD(x) ((x/180.0f) * 3.141592653f)

// just like in shareddefs.h
#define HITGROUP_HEAD 1

// ----------------------------------------- TYPEDEFS ------------------------------------------- //

// from clientmode_shared.cpp:316. We tweak it a bit. This will store the real CreateMove function!
// bool ClientModeShared::CreateMove(float flInputSampleTime, CUserCmd* cmd);
typedef bool(__fastcall* CreateMoveFn)(void* ecx, DWORD edx, float flInputSampleTime, void* cmd);

// interface.cpp
typedef void* (*CreateInterfaceFn)(const char* pName, int* pReturnCode);
typedef void* matrix3x4_t;

typedef void* IHandleEntity;
typedef uint8_t byte;





// if you look in source its just 3 floats
struct Vector
{
    float x, y, z;
};

Vector operator+(Vector& v1, Vector& v2) {
    Vector vv;
    vv.x = v1.x + v2.x;
    vv.y = v1.y + v2.y;
    vv.z = v1.z + v2.z;
    return vv;
}

struct VectorAligned {
    float x, y, z, pad; // pad for byte alignment I think
};


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
    virtual bool ShouldHitEntity(IHandleEntity* pEntity, int contentsMask) = 0;
    virtual TraceType_t	GetTraceType() const = 0;
};

extern C_CSPlayer* pLocalPlayer; // pointer to local player

class MyITraceFilter : ITraceFilter {
    bool ShouldHitEntity(IHandleEntity* pEntity, int contentsMask) override {
        // printf("ShouldHitEntity called: pEntity is %p, contentsMask: %d\n", pEntity, contentsMask);
        // don't count collisions with our local player
        if (pEntity == (IHandleEntity*) pLocalPlayer) {
            return false;
        }
        return true; // so it always hits said entity
    }
    TraceType_t	GetTraceType() const override {
        return TRACE_EVERYTHING; // we want to trace everything
    }
};



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

class C_BaseCombatWeapon;
typedef void kbutton_t;
typedef Vector QAngle;
class cInput {
public:	
    virtual		void		Init_All(void) = 0;
    virtual		void		Shutdown_All(void) = 0;
    virtual		int			GetButtonBits(bool bResetState) = 0;
    virtual		void		CreateMove(int sequence_number, float input_sample_frametime, bool active) = 0;
    virtual		void		ExtraMouseSample(float frametime, bool active) = 0;
    virtual		bool		WriteUsercmdDeltaToBuffer(int nSlot, void* buf, int from, int to, bool isnewcommand) = 0;
    virtual		void		EncodeUserCmdToBuffer(int nSlot, void* buf, int slot) = 0;
    virtual		void		DecodeUserCmdFromBuffer(int nSlot, void* buf, int slot) = 0;
    virtual		CUserCmd* GetUserCmd(int nSlot, int sequence_number) = 0;
    virtual		void		MakeWeaponSelection(C_BaseCombatWeapon* weapon) = 0;
    virtual		float		KeyState(kbutton_t* key) = 0;
    virtual		int			KeyEvent(int down, uint32_t keynum, const char* pszCurrentBinding) = 0;
    virtual		kbutton_t* FindKey(const char* name) = 0;
    virtual		void		ControllerCommands(void) = 0;
    virtual		void		Joystick_Advanced(bool bSilent) = 0;
    virtual		void		Joystick_SetSampleTime(float frametime) = 0;
    virtual		float		Joystick_GetPitch(void) = 0;
    virtual		float		Joystick_GetYaw(void) = 0;
    virtual		void		Joystick_Querry(float& forward, float& side, float& pitch, float& yaw) = 0;
    virtual		void		Joystick_ForceRecentering(int nStick, bool bSet = true) = 0;
    virtual		void		IN_SetSampleTime(float frametime) = 0;
    virtual		void		AccumulateMouse(int nSlot) = 0;
    virtual		void		ActivateMouse(void) = 0;
    virtual		void		DeactivateMouse(void) = 0;
    virtual		void		ClearStates(void) = 0;
    virtual		float		GetLookSpring(void) = 0;
    virtual		void		GetFullscreenMousePos(int* mx, int* my, int* unclampedx = NULL, int* unclampedy = NULL) = 0;
    virtual		void		SetFullscreenMousePos(int mx, int my) = 0;
    virtual		void		ResetMouse(void) = 0;
    virtual		float		GetLastForwardMove(void) = 0;
    virtual		void		ClearInputButton(int bits) = 0;
    virtual		void		CAM_Think(void) = 0;
    virtual		int			CAM_IsThirdPerson(int nSlot = -1) = 0;
    virtual		bool		CAM_IsThirdPersonOverview(int nSlot = -1) = 0;
    virtual		void		CAM_GetCameraOffset(Vector& ofs) = 0;
    virtual		void		CAM_ToThirdPerson(void) = 0;
    virtual		void		CAM_ToFirstPerson(void) = 0;
    virtual		void		CAM_ToThirdPersonShoulder(void) = 0;
    virtual		void		CAM_ToThirdPersonOverview(void) = 0;
    virtual		void		CAM_StartMouseMove(void) = 0;
    virtual		void		CAM_EndMouseMove(void) = 0;
    virtual		void		CAM_StartDistance(void) = 0;
    virtual		void		CAM_EndDistance(void) = 0;
    virtual		int			CAM_InterceptingMouse(void) = 0;
    virtual		void		CAM_Command(int command) = 0;
    virtual		void		CAM_ToOrthographic() = 0;
    virtual		bool		CAM_IsOrthographic() const = 0;
    virtual		void		CAM_OrthographicSize(float& w, float& h) const = 0;
    virtual		void		AddIKGroundContactInfo(int entindex, float minheight, float maxheight) = 0;
    virtual		void		LevelInit(void) = 0;
    virtual		void		CAM_SetCameraThirdData(void* pCameraData, const QAngle& vecCameraOffset) = 0;
    virtual		void		CAM_CameraThirdThink(void) = 0;
    virtual		void		CheckPaused(CUserCmd* cmd) = 0;
    virtual		void		CheckSplitScreenMimic(int nSlot, CUserCmd* cmd, CUserCmd* pPlayer0Command) = 0;
    virtual void		Init_Camera(void) = 0;
    virtual void		ApplyMouse(int nSlot, QAngle& viewangles, CUserCmd* cmd, float mouse_x, float mouse_y) = 0;
    virtual void	JoyStickMove(float frametime, CUserCmd* cmd) = 0;
    virtual void	SteamControllerMove(float frametime, CUserCmd* cmd) = 0;
    virtual bool	ControllerModeActive(void) = 0;
    virtual bool	JoyStickActive() = 0;
    virtual void	JoyStickSampleAxes(float& forward, float& side, float& pitch, float& yaw, bool& bAbsoluteYaw, bool& bAbsolutePitch) = 0;
    virtual void	JoyStickThirdPersonPlatformer(CUserCmd* cmd, float& forward, float& side, float& pitch, float& yaw) = 0;
    virtual void	JoyStickTurn(CUserCmd* cmd, float& yaw, float& pitch, float frametime, bool bAbsoluteYaw, bool bAbsolutePitch) = 0;
    virtual void	JoyStickForwardSideControl(float forward, float side, float& joyForwardMove, float& joySideMove) = 0;
    virtual void	JoyStickApplyMovement(CUserCmd* cmd, float joyForwardMove, float joySideMove) = 0;
    virtual void GetWindowCenter(int& x, int& y) = 0;
};
