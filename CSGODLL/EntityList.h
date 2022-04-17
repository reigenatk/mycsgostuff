#pragma once
#include <stdint.h>
#include "NetVar.h"

// forward declare some classes that we don't wanna define so we don't get compiler error
// they are big classes.

class IClientUnknown;
class CBaseHandle;
class IClientEntity;


// declare this one because its small
struct EntityCacheInfo_t
{
	// Cached off because GetClientNetworkable is called a *lot*
	IClientNetworkable* m_pNetworkable;
	unsigned short m_BaseEntitiesIndex;	// Index into m_BaseEntities (or m_BaseEntities.InvalidIndex() if none).
	unsigned short m_bDormant;	// cached dormant state - this is only a bit
};

// Legit Entity List (from source)
class IClientEntityList
{
public:
	// Get IClientNetworkable interface for specified entity
	virtual IClientNetworkable * GetClientNetworkable(int entnum) = 0;
	virtual IClientNetworkable* GetClientNetworkableFromHandle(CBaseHandle hEnt) = 0;
	virtual IClientUnknown* GetClientUnknownFromHandle(CBaseHandle hEnt) = 0;

	// NOTE: This function is only a convenience wrapper.
	// It returns GetClientNetworkable( entnum )->GetIClientEntity().
	virtual IClientEntity* GetClientEntity(int entnum) = 0;
	virtual IClientEntity* GetClientEntityFromHandle(CBaseHandle hEnt) = 0;

	// Returns number of entities currently in use
	virtual int					NumberOfEntities(bool bIncludeNonNetworkable) = 0;

	// Returns highest index actually used
	virtual int					GetHighestEntityIndex(void) = 0;

	// Sizes entity list to specified size
	virtual void				SetMaxEntities(int maxents) = 0;
	virtual int					GetMaxEntities() = 0;
	virtual EntityCacheInfo_t* GetClientNetworkableArray() = 0;
};

IClientEntityList* interfaceEntityList;

// A ghetto entity list I derived using ReClass
// Created with ReClass.NET 1.2 by KN4CK3R
class C_BaseEntity
{
public:
	char pad_0000[160]; //0x0000
	float xPosition; //0x00A0
	float yPosition; //0x00A4
	float zPosition; //0x00A8
	char pad_00AC[72]; //0x00A0
	int32_t	m_iTeamNum;
	int32_t	m_iPendingTeamNum;
	int32_t	m_nNextThinkTick;
	int32_t HEALTH; //0x0100
	char pad_0104[4]; //0x0104
	float CameraOffset_X; //0x0108
	float CameraOffset_Y; //0x010C
	float CameraOffset_Z; //0x0110
	char pad_0114[24]; //0x0114
	float Pitch; //0x012C
	float Yaw; //0x0130
	float Row; //0x0134
	char pad_0138[36]; //0x0138

public:
	virtual void pad1() = 0;
	virtual void pad2() = 0;
	virtual void pad3() = 0;
	virtual void pad4() = 0;
	virtual IClientNetworkable* GetClientNetworkable() = 0;
}; //Size: 0x015C

class CEntInfo
{
public:
	C_BaseEntity* m_pEntity; //0x0000
	int32_t m_SerialNumber; //0x0004
	CEntInfo* m_pPrev; //0x0008
	CEntInfo* m_pNext; //0x000C
}; //Size: 0x0010

class cBaseEntityList
{
public:
	CEntInfo m_EntyPtrArray[64]; //0x0000
	char pad_0400[2096]; //0x0400
}; //Size: 0x0C30

